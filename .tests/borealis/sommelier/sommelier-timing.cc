// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier-timing.h"   // NOLINT(build/include_directory)
#include "sommelier-tracing.h"  // NOLINT(build/include_directory)

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#define NSEC_PER_SEC 1000000000
#define NSEC_PER_USEC 1000

static inline int64_t timespec_to_ns(timespec const* t) {
  return (int64_t)t->tv_sec * NSEC_PER_SEC + t->tv_nsec;
}

// Records start time to calculate first delta.
void Timing::RecordStartTime() {
  clock_gettime(CLOCK_REALTIME, &last_event);
}

int64_t Timing::GetTime() {
  timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  int64_t now = timespec_to_ns(&tp);
  int64_t last = timespec_to_ns(&last_event);
  last_event = tp;
  return now - last;
}

// Create a new action, add info gained from attach call.
void Timing::UpdateLastAttach(int surface_id, int buffer_id) {
  actions[event_id % kMaxNumActions] =
      BufferAction(GetTime(), surface_id, buffer_id, BufferAction::ATTACH);
  event_id++;
}

// Create a new action, add info gained from commit call.
void Timing::UpdateLastCommit(int surface_id) {
  actions[event_id % kMaxNumActions] = BufferAction(
      GetTime(), surface_id, kUnknownBufferId, BufferAction::COMMIT);
  event_id++;
}

// Add a release action with release timing info.
void Timing::UpdateLastRelease(int buffer_id) {
  actions[event_id % kMaxNumActions] = BufferAction(
      GetTime(), kUnknownSurfaceId, buffer_id, BufferAction::RELEASE);
  event_id++;
}

// Output the recorded actions to the timing log file.
void Timing::OutputLog() {
  TRACE_EVENT("timing", "Timing::OutputLog");

  if (event_id == 0) {
    std::cout << "No events in buffer, exiting" << std::endl;
    return;
  }

  std::cout << "Writing buffer activity to the timing log file" << std::endl;

  std::string output_filename =
      std::string(filename) + "_set_" + std::to_string(saves);

  std::ofstream outfile(output_filename);

  int start = 0;
  int buf_size = event_id;
  if (event_id >= kMaxNumActions) {
    start = event_id % kMaxNumActions;
    buf_size = kMaxNumActions;
  }

  outfile << "Type Surface_ID Buffer_ID Delta_Time" << std::endl;
  for (int i = 0; i < buf_size; i++) {
    int idx = (i + start) % kMaxNumActions;
    std::string type("?");
    if (actions[idx].action_type == BufferAction::ATTACH) {
      type = "a";
    } else if (actions[idx].action_type == BufferAction::COMMIT) {
      type = "c";
    } else if (actions[idx].action_type == BufferAction::RELEASE) {
      type = "r";
    }
    outfile << type << " ";
    outfile << actions[idx].surface_id << " ";
    outfile << actions[idx].buffer_id << " ";
    outfile << static_cast<double>(actions[idx].delta_time) / NSEC_PER_USEC
            << std::endl;
  }

  std::stringstream nsec;
  nsec << std::setw(9) << std::setfill('0') << last_event.tv_nsec;
  outfile << "EndTime " << event_id - 1 << " " << last_event.tv_sec << "."
          << nsec.str() << std::endl;

  outfile.close();
  std::cout << "Finished writing " << output_filename << std::endl;
  ++saves;
}

SurfaceStats::SurfaceStats() : total_frames(0) {
  clock_gettime(CLOCK_REALTIME, &start_event);
  clock_gettime(CLOCK_REALTIME, &first_event);
  clock_gettime(CLOCK_REALTIME, &last_event);

  StartNewWindow();
}

void SurfaceStats::StartNewWindow() {
  steam_game_id = 0;
  num_frames = 0;
  num_activated = 0;
  mean_fps = 0.0;
  sq_dist_mean = 0.0;
  min_fps = 0.0;
  max_fps = 0.0;
  std::fill(buckets, buckets + kMaxBuckets, 0);
}

void SurfaceStats::AddFrame(uint32_t steam_id, bool activated) {
  // Calculate current time and normalize to ns.
  timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  int64_t now = timespec_to_ns(&tp);
  int64_t last = timespec_to_ns(&last_event);
  last_event = tp;

  steam_game_id = steam_id;
  total_frames++;

  // Initial frame does not have useful fps, track start times.
  if (total_frames == 1) {
    clock_gettime(CLOCK_REALTIME, &start_event);
    return;
  }

  num_frames++;
  if (num_frames == 1) {
    clock_gettime(CLOCK_REALTIME, &first_event);
  }
  if (activated) {
    num_activated++;
  }

  // Bounds check times to avoid divide by zero or out of bound arrays.
  if (now <= last) {
    return;
  }

  // This is at least the second frame so there should be a valid fps.
  // Note that if one frame is delayed, then the next frame might appear as
  // fast which can cause some questionable values.
  double fps = static_cast<double>(NSEC_PER_SEC) / (now - last);

  // Welford's online algorithm to calculate variance.
  double delta = fps - mean_fps;
  mean_fps += delta / num_frames;
  double delta2 = fps - mean_fps;
  sq_dist_mean += delta * delta2;

  if (num_frames == 1) {
    min_fps = fps;
    max_fps = fps;
  } else {
    min_fps = std::min(fps, min_fps);
    max_fps = std::max(fps, max_fps);
  }

  // Add to histogram bucket, avoiding overflow.
  int bucket = fps / kBucketSize;
  bucket = std::min(bucket, kMaxBuckets - 1);
  buckets[bucket]++;
}

std::string SurfaceStats::Summarize(int surface_id) const {
  timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);

  int64_t now = timespec_to_ns(&tp);
  int64_t first = timespec_to_ns(&first_event);
  int64_t start = timespec_to_ns(&start_event);

  // Finalize variance counters.
  double variance = 0.0;
  if (num_frames >= 2) {
    variance = sq_dist_mean / num_frames;
  }

  double median_fps = EstimatePercentile(50.0);
  double slow_frame_threshold = median_fps * kSlowFrameThresholdRatio;

  // Keep this in sync with GenerateHeader().
  std::stringstream log;
  log.setf(std::ios_base::fixed, std::ios_base::floatfield);
  log << std::setprecision(2);
  log << tp.tv_sec << " ";
  log << surface_id << " ";
  log << steam_game_id << " ";
  log << total_frames << " ";
  log << (now - start) / NSEC_PER_SEC << " ";
  log << num_frames << " ";
  log << (now - first) / NSEC_PER_SEC << " ";
  log << num_activated << " ";
  log << min_fps << " ";
  log << mean_fps << " ";
  log << max_fps << " ";
  log << EstimatePercentile(0.1) << " ";
  log << EstimatePercentile(1.0) << " ";
  log << median_fps << " ";
  log << EstimatePercentile(99.0) << " ";
  log << variance << " ";
  log << CountSlowFrames(slow_frame_threshold) << " ";
  // histogram includes a leading space.
  log << kLogBuckets * kBucketSize;

  // Accumulate internal buckets to larger buckets.
  int accumulated = 0;
  for (int i = 0; i < kMaxBuckets; ++i) {
    accumulated += buckets[i];
    if (i % kLogBuckets == kLogBuckets - 1) {
      log << " " << accumulated;
      accumulated = 0;
    }
  }
  if (accumulated) {
    log << " " << accumulated;
  }

  return log.str();
}
std::string SurfaceStats::GenerateHeader() {
  // Keep this in sync with Summarize().
  std::stringstream log;
  log << "timestamp" << " ";
  log << "surface_id" << " ";
  log << "steam_game_id" << " ";
  log << "total_frames" << " ";
  log << "total_duration" << " ";
  log << "num_frames" << " ";
  log << "current_duration" << " ";
  log << "num_activated" << " ";
  log << "min_fps" << " ";
  log << "mean_fps" << " ";
  log << "max_fps" << " ";
  log << "p0_1" << " ";
  log << "p1_0" << " ";
  log << "p50" << " ";
  log << "p99" << " ";
  log << "variance" << " ";
  log << "num_slow_frames" << " ";
  log << "bucket_size" << " ";
  log << "histogram...";

  return log.str();
}

double SurfaceStats::GetBucketValue(int bucket_num) {
  // TODO(davidriley): Consider doing a linear interpolation across the bucket.
  // TODO(davidriley): Consider calculating a mean value for overflow bucket.
  double bucket_start = bucket_num * kBucketSize;
  double bucket_end = bucket_start + kBucketSize;
  return (bucket_start + bucket_end) / 2.0;
}

double SurfaceStats::EstimatePercentile(double percentile) const {
  // Estimate percentile by counting through the histogram.
  int frames_to_go = num_frames * percentile / 100.0;

  // Look for the correct bucket.
  // TODO(davidriley): Could iterate backwards if percentile > 50.
  for (int i = 0; i != kMaxBuckets; i++) {
    if (frames_to_go < buckets[i]) {
      // Found the right bucket.
      return GetBucketValue(i);
    } else {
      frames_to_go -= buckets[i];
    }
  }
  return 0.0;
}

int SurfaceStats::CountSlowFrames(double threshold) const {
  int count = 0;
  for (int i = 0; i < kMaxBuckets; ++i) {
    double bucket_mid = GetBucketValue(i);
    if (bucket_mid < threshold) {
      count += buckets[i];
    } else {
      break;
    }
  }

  return count;
}

void FrameStats::AddFrame(int surface_id,
                          uint32_t steam_game_id,
                          bool activated) {
  surface_stats[surface_id].AddFrame(steam_game_id, activated);
}

void FrameStats::OutputStats() {
  TRACE_EVENT("timing", "FrameStats::OutputStats");

  // Write to a logfile if requested, silently failing if not specified.
  std::ofstream logfile;
  if (log_filename != nullptr) {
    logfile.open(log_filename, std::ios_base::app);
  }

  // Generate summaries for each surface.
  bool was_used = false;
  // Iterate, erasing idle surfaces.
  for (auto i = surface_stats.begin(); i != surface_stats.end();
       i = was_used ? std::next(i) : surface_stats.erase(i)) {
    std::string log = i->second.Summarize(i->first);
    recent_logs.push_back(log);
    logfile << log << std::endl;

    // If there are no frames for the period, clear the surface entirely.
    was_used = i->second.GetNumFrames();
    i->second.StartNewWindow();
  }

  // Write the current set of summaries atomically.
  // The full set is rewritten each time to avoid having to handle rotation
  // of a log that is appended to.
  if (filename != nullptr) {
    std::string output_filename = std::string(filename) + "_tmp";
    std::ofstream outfile(output_filename);

    if (header.empty()) {
      header = SurfaceStats::GenerateHeader();
    }
    outfile << header << std::endl;
    for (auto const& i : recent_logs) {
      outfile << i << std::endl;
    }
    outfile.close();
    if (outfile) {
      rename(output_filename.c_str(), filename);
    }
  }

  // Keep the log size bounded.
  while (recent_logs.size() > kMaxLogSize) {
    recent_logs.pop_front();
  }
}
