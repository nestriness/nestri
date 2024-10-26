// Copyright 2021 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_SOMMELIER_SOMMELIER_TIMING_H_
#define VM_TOOLS_SOMMELIER_SOMMELIER_TIMING_H_

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <time.h>

const int kUnknownBufferId = -1;
const int kUnknownSurfaceId = -1;

class Timing {
 public:
  explicit Timing(const char* fname) : filename(fname) {}
  void RecordStartTime();
  void UpdateLastAttach(int surface_id, int buffer_id);
  void UpdateLastCommit(int surface_id);
  void UpdateLastRelease(int buffer_id);
  void OutputLog();

 private:
  // 10 min * 60 sec/min * 60 frames/sec * 3 actions/frame = 108000 actions
  static const int kMaxNumActions = 10 * 60 * 60 * 3;

  struct BufferAction {
    enum Type { UNKNOWN, ATTACH, COMMIT, RELEASE };
    int64_t delta_time;
    int surface_id;
    int buffer_id;
    Type action_type;
    BufferAction()
        : surface_id(kUnknownSurfaceId),
          buffer_id(kUnknownBufferId),
          action_type(UNKNOWN) {}
    explicit BufferAction(int64_t dt,
                          int sid = kUnknownSurfaceId,
                          int bid = kUnknownBufferId,
                          Type type = UNKNOWN)
        : delta_time(dt), surface_id(sid), buffer_id(bid), action_type(type) {}
  };

  BufferAction actions[kMaxNumActions];
  int event_id = 0;
  int saves = 0;
  const char* filename;
  timespec last_event;

  int64_t GetTime();
};  // class Timing

// SurfaceStats tracks statistics for a single surface in a number of
// discrete contiguous windows.  Besides for some very basic accounting
// and state to track across separate windows, state is reset between
// windows.
class SurfaceStats {
 public:
  SurfaceStats();

  // Resets the state to start tracking a new reporting window.
  void StartNewWindow();

  void AddFrame(uint32_t steam_id, bool activated);
  int GetNumFrames() const { return num_frames; }
  std::string Summarize(int surface_id) const;
  static std::string GenerateHeader();

 private:
  // Number of buckets to internally accumulate to.  Internally more buckets
  // are tracked to give reasonable precision for the percentiles.
  static const int kMaxBuckets = 200;

  // Size of each bucket in fps.
  static constexpr double kBucketSize = 1.0f;

  // The number of internal buckets (of kBucketSize) to merge to form each
  // of the externally logged buckets.  Resulting logged bucket size is
  // kLogBuckets * kBucketSize.  Externally less buckets are shown to
  // avoid too much information/space while still allow characterization of
  // the data.
  static const int kLogBuckets = 5;

  // Threshold for slow frames expressed as a ration of the median.
  static constexpr double kSlowFrameThresholdRatio = 0.8f;

  // Tracks the start of the surface being tracked at all.
  timespec start_event;

  // Timestamps of the first and last frames of the current reporting period.
  timespec first_event;
  timespec last_event;

  // The total number of frames seen on this surface since statistics
  // started being tracked.
  int total_frames;

  // The rest of the statistics apply to the current reporting period only.

  // Steam game id as reported by the game, if available.
  uint32_t steam_game_id;

  // The number of frames in the current reporting period.
  int num_frames;

  // The number of frames where the window was activated at the time of the
  // frame being committed, in the current reporting period.
  int num_activated;

  // The running mean fps, per Welford's online algorithm for computing
  // variance.
  double mean_fps;

  // The aggregated squared distance from the the mean, per Welford's
  // online algorithm.
  double sq_dist_mean;

  // The minimum per-frame fps seen in the current reporting period.
  double min_fps;

  // The maximum per-frame fps seen in the current reporting period.
  double max_fps;

  // Histogram of frame timings, each bucket of kBucketSize fps.
  // The histogram should be sized to give a good view into .1% and 1%
  // low fps on a typical reporting period.
  int buckets[kMaxBuckets];

  static double GetBucketValue(int bucket_num);
  double EstimatePercentile(double percentile) const;
  int CountSlowFrames(double threshold) const;
};  // class SurfaceStats

class FrameStats {
 public:
  explicit FrameStats(const char* stats_name, const char* log_name)
      : filename(stats_name), log_filename(log_name) {}

  void AddFrame(int surface_id, uint32_t steam_id, bool activated);
  void OutputStats();

 private:
  // Total number of log lines across all surfaces.
  static const int kMaxLogSize = 60;

  const char* filename;
  const char* log_filename;
  std::map<int, SurfaceStats> surface_stats;

  std::string header;
  std::list<std::string> recent_logs;
};  // class FrameStats

#endif  // VM_TOOLS_SOMMELIER_SOMMELIER_TIMING_H_
