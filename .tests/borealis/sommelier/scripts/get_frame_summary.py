#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Summarizes Sommelier timing information."""

import argparse
from enum import Enum
import statistics
from typing import NamedTuple


_MS_PER_SEC = 1000
_US_PER_SEC = 1000000
# Floating point error bounds
_FP_ERROR = 0.01


class EventType(Enum):
    """Wayland event type."""

    COMMIT = 1
    ATTACH = 2
    RELEASE = 3
    UNKNOWN = 4


class EventInfo(NamedTuple):
    """Stores information of an event."""

    event_type: EventType
    surface_id: int
    buffer_id: int
    time: float


def parse_event_type(event_type):
    EVENT_MAP = {
        "a": EventType.ATTACH,
        "c": EventType.COMMIT,
        "r": EventType.RELEASE,
    }
    return EVENT_MAP.get(event_type, EventType.UNKNOWN)


class FrameLog:
    """Manages access to the Sommelier timing logs."""

    def __init__(self, filename):
        """Parse Sommelier timing log.

        Format of log (header line might be truncated):
        Type Surface_ID Buffer_ID Delta_Time   # header line 1
        a 12 20 4237.44                        # line 2
        ....
        EndTime 3972 1655330324.7              # last line
        Last line format: (EndTime, last event id, time since epoch (s))
        """
        self.frame_log = []
        self.surfaces = set()
        with open(filename, "r") as f:
            lines = f.read().splitlines()
            total_delta_time = 0
            last_line = lines[-1].split(" ")
            if len(last_line) != 3 or last_line[0] != "EndTime":
                print(f"Invalid EndTime: {lines[-1]}")
                return
            self.end_time = float(last_line[2])
            for l in reversed(lines[1:-1]):
                line = l.rstrip().split(" ")
                # Skip parsing line that is improperly formatted
                if len(line) != 4:
                    continue
                total_delta_time += float(line[3]) / _US_PER_SEC
                surface_id = int(line[1])
                info = EventInfo(
                    event_type=parse_event_type(line[0]),
                    surface_id=surface_id,
                    buffer_id=int(line[2]),
                    time=self.end_time - total_delta_time,
                )
                self.frame_log.append(info)
                self.surfaces.add(surface_id)

    def get_target_ft(self, target_fps, avg_fps):
        """Outputs target frame time given a target fps.

        If target_fps is None, determine automatically based on
        average FPS.

        Args:
            target_fps: Target FPS (30, 60, None).
            avg_fps: Average FPS over time window.
        """
        if not target_fps:
            # determines appropriate target FPS by finding whether the
            # average is closer to 30 or 60.
            # TODO(asriniva): Revisit this methodology. What should a title
            # averaging 31-33 FPS target? Non 30/60 FPS targets?
            target_fps = min([30, 60], key=lambda x: abs(x - avg_fps))
        # Acceptable frame time ranges, based on Battlestar's metrics.
        # The +/-3 bounds do not scale with FPS (small variance for high FPS,
        # large variance for low FPS) but can account for variability in
        # hardware.
        return [_MS_PER_SEC / target_fps - 3, _MS_PER_SEC / target_fps + 3]

    def output_fps(self, surface, windows, max_ft_ms, target_fps):
        """Outputs the summarized fps information based on frame log.

        Args:
            surface: Surface ID
            windows: List of time windows (in seconds) to summarize.
            max_ft_ms: Max frame time threshold (ms).
            target_fps: Target FPS, either 30, 60, or
                        None (automatically determined).
        """
        max_frame_ms = 0
        # only check for commit events on the given surface
        # events are in reverse chronological order
        events = [
            e
            for e in self.frame_log
            if e.surface_id == surface and e.event_type == EventType.COMMIT
        ]
        if not events:
            print(f"No commit events found for surface {surface}\n")
            return
        total_sec = self.end_time - events[-1].time
        ft_target_ms = self.get_target_ft(target_fps, len(events) / total_sec)
        for w_sec in windows + [total_sec]:
            # num frames in acceptable range
            target_frames = 0
            # num frames exceeding max_ft_ms
            max_ft_events = 0
            prev_sec = self.end_time
            frame_count = 0
            frames_ms = []
            for event in events:
                frame_ms = (prev_sec - event.time) * _MS_PER_SEC
                frames_ms.append(frame_ms)
                max_frame_ms = max(max_frame_ms, frame_ms)
                if ft_target_ms[0] < frame_ms < ft_target_ms[1]:
                    target_frames += 1
                if frame_ms > max_ft_ms:
                    max_ft_events += 1
                current_sec = self.end_time - event.time
                frame_count += 1
                # handles floating point error in the case when
                # w_sec == total_sec
                if current_sec > w_sec - _FP_ERROR:
                    print(f"Summary for last {w_sec} seconds")
                    print("-------------------------------")
                    print(f"FPS: {frame_count / current_sec}")
                    print(f"Max frame time: {max_frame_ms} ms")
                    print(f"Frame count: {frame_count} frames")
                    print(
                        f"Percentage frames within acceptable target "
                        f"{ft_target_ms} ms: "
                        f"{target_frames * 100/frame_count}%"
                    )
                    if len(frames_ms) > 1:
                        c_var = statistics.stdev(frames_ms) / statistics.mean(
                            frames_ms
                        )
                        print(f"Coefficient of variation: {c_var}")
                    print(
                        f"Frames exceeding max frame time threshold"
                        f" {max_ft_ms} ms:"
                        f" {max_ft_events} frames"
                    )
                    print()
                    break
                prev_sec = event.time
        print()

    def output_fps_sliding(self, surface, window, max_ft_ms):
        """Outputs the summarized fps information based on frame log.

        Args:
            surface: Surface ID
            window: Window size (in num of frames).
            max_ft_ms: Max frame time threshold (ms).
            target_fps: Target FPS, either 30, 60, or
                        None (automatically determined).
        """
        print(f"Sliding window aggregates for {window} events")
        print("-------------------------------")

        # only check for commit events on the given surface
        # events are in reverse chronological order
        events = [
            e
            for e in self.frame_log
            if e.surface_id == surface and e.event_type == EventType.COMMIT
        ]
        if not events:
            print(f"No commit events found for surface {surface}\n")
            return
        # For a sliding window, aggregate the following:
        # cvars: Coefficient of variation over window
        # max_ft_events: Number of frames over max_ft_ms
        cvars = []
        max_ft_events = []
        for i in range(window, len(events)):
            sl_window = events[i - window : i]
            frames_ms = [
                (sl_window[x].time - sl_window[x + 1].time) * _MS_PER_SEC
                for x in range(window - 1)
            ]

            max_fts = sum([1 for x in frames_ms if x > max_ft_ms])
            c_var = statistics.stdev(frames_ms) / statistics.mean(frames_ms)
            cvars.append(c_var)
            max_ft_events.append(max_fts)
        if len(cvars) > 1:
            print(
                "Arithmetic mean of coefficient of variation:",
                statistics.fmean(cvars),
            )
            print(
                "Geometric mean of coefficient of variation:",
                statistics.geometric_mean(cvars),
            )
        if len(max_ft_events) > 1:
            print(
                f"Average number of frame time events over threshold"
                f" {max_ft_ms} ms:",
                statistics.mean(max_ft_events),
            )
        print()

    print()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Return frame summary based on Sommelier timing log."
    )

    parser.add_argument("file", help="Filename of timing log")
    parser.add_argument(
        "--windows",
        action="extend",
        type=int,
        nargs="+",
        help="Time windows for summary (in seconds)",
        default=[10, 60, 300],
    )
    parser.add_argument(
        "--target-fps", type=int, help="Target FPS", default=None
    )
    parser.add_argument(
        "--max-frame-time",
        type=float,
        help="Max frame time threshold (in milliseconds)",
        default=200,
    )
    parser.add_argument(
        "--sliding",
        type=int,
        help="Use sliding window with size in number of frames",
        default=300,
    )
    args = parser.parse_args()
    if args.target_fps and args.target_fps < 20:
        parser.error("Choose target FPS above 20 FPS")
    log = FrameLog(args.file)
    for s in log.surfaces:
        print(f"Summary for surface {s}")
        print("-------------------------------")
        log.output_fps_sliding(
            s, max_ft_ms=args.max_frame_time, window=args.sliding
        )
        log.output_fps(
            s,
            windows=sorted(args.windows),
            max_ft_ms=args.max_frame_time,
            target_fps=args.target_fps,
        )
