# Copyright 2021 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility class for analyzing sommelier buffer statistics."""

import argparse
from collections import defaultdict
import sys


class Action(object):
    """Hold data about sommelier buffer operations."""

    __slots__ = ("type", "bid", "time")

    def __init__(self, action_type, bid, time):
        self.type = action_type
        self.bid = bid
        self.time = time


class Stats:
    """For processing text files of sommelier data and printing their stats."""

    def __init__(self):
        self.surfaces = defaultdict(list)
        self.bid_sid = {}
        self.sid_bid = {}

    def read(self, filename):
        """Read in a sommelier-timing output file for later processing.

        Sample file:
        Event, Type, Surface_ID, Buffer_ID, Time    # header line 1
        0 surface_attach 12 23 1612345678.987654321 # line 2
        1 surface_commit 12 -1 1612345678.987654325 # lines 3, 4, ...

        Run sommelier with the timing-filename option to generate an event log:
          sommelier -X --glamor --trace-system \
            --timing-filename=out.txt glxgears &

        Args:
            filename (string): The path to the buffer time data.
        """

        with open(filename, "r") as f:
            # Skip the header line.
            f.readline()
            for line in f:
                try:
                    [_, action_type, sid, bid, timestamp] = line.split()
                except ValueError:
                    continue
                sid = int(sid)
                bid = int(bid)
                # use dictionary history to fill missing ids
                if sid != -1 and bid != -1:
                    self.sid_bid[sid] = bid
                    self.bid_sid[bid] = sid
                elif sid in self.sid_bid:
                    bid = self.sid_bid[sid]
                elif bid in self.bid_sid:
                    sid = self.bid_sid[bid]
                # convert string time into floating point time
                time = float(timestamp)

                self.surfaces[sid].append(Action(action_type, bid, time))

    def add(self, durr, tot, err):
        """Kahan sum function to reduce floating point error.

        Args:
          durr (float): The elapsed time between two actions.
          tot (float): The summed total of elapsed times.
          err (float): The accumulated error.

        Returns:
          tot (int): the new summed total of elapsed times.
          err (int): the new accumulated error.
        """
        y = durr - err
        t = tot + y
        err = (t - tot) - y
        tot = t
        return tot, err

    def add_acr(self, sid, time, acr, idx):
        """Update the sum/total, error, and count of each action type.

        Args:
          sid (int): the id of the surface to display stats for.
          time (float): the time the current action to place.
          acr (dict): A dicitionary containing numerical info on accumulated stats.
          idx (dict): A dictionary containing the last index of an action
        """
        for action, pos in idx.items():
            if pos is not None:
                prev_time = self.surfaces[sid][pos].time
                x = action[0]  # use first letter of action type in key names
                tot, err = self.add(time - prev_time, acr[x], acr[x + "err"])
                acr[x] = tot
                acr[x + "err"] = err
                acr[x + "_count"] += 1

    def print_stats(self, sid):
        """Print the average times between calls to a surface.

        Args:
          sid (int): the id of the surface to display stats for.
        """

        # to_attach - a dict used in calculating summed difference from any other
        #   call (a, c, r) to an "attach" call.
        # a - attach, c - commit, r - release
        # aerr - accumulated numerical error from summing differences.
        # a_count - count of attach-attach differences so far.
        to_attach = {
            "a": 0,
            "aerr": 0,
            "a_count": 0,
            "c": 0,
            "cerr": 0,
            "c_count": 0,
            "r": 0,
            "rerr": 0,
            "r_count": 0,
        }
        to_commit = {
            "a": 0,
            "aerr": 0,
            "a_count": 0,
            "c": 0,
            "cerr": 0,
            "c_count": 0,
            "r": 0,
            "rerr": 0,
            "r_count": 0,
        }
        to_release = {
            "a": 0,
            "aerr": 0,
            "a_count": 0,
            "c": 0,
            "cerr": 0,
            "c_count": 0,
            "r": 0,
            "rerr": 0,
            "r_count": 0,
        }

        # idx stores the last index of an attach, commit, and release call.
        idx = {"attach": None, "commit": None, "release": None}
        # For every action, calculate summed difference based on its type.
        # e.g. for a commit action, calculate time between it and the previous
        #   attach, commit, and release call using to_commit.
        for i, action in enumerate(self.surfaces[sid]):
            time = action.time

            if action.type == "attach":
                self.add_acr(sid, time, to_attach, idx)
                idx["attach"] = i
            elif action.type == "commit":
                self.add_acr(sid, time, to_commit, idx)
                idx["commit"] = i
            elif action.type == "release":
                self.add_acr(sid, time, to_release, idx)
                idx["release"] = i

        print(
            "attach-attach avg: ",
            to_attach["a"] * 1000 / to_attach["a_count"],
            " ms",
        )
        print(
            "commit-attach avg: ",
            to_attach["c"] * 1000 / to_attach["c_count"],
            " ms",
        )
        print(
            "release-attach avg: ",
            to_attach["r"] * 1000 / to_attach["r_count"],
            " ms",
        )
        print()
        print(
            "commit-commit avg: ",
            to_commit["c"] * 1000 / to_commit["c_count"],
            " ms",
        )
        print(
            "attach-commit avg: ",
            to_commit["a"] * 1000 / to_commit["a_count"],
            " ms",
        )
        print(
            "release-commit avg: ",
            to_commit["r"] * 1000 / to_commit["r_count"],
            " ms",
        )
        print()
        print(
            "release-release avg: ",
            to_release["r"] * 1000 / to_release["r_count"],
            " ms",
        )
        print(
            "attach-release avg: ",
            to_release["a"] * 1000 / to_release["a_count"],
            " ms",
        )
        print(
            "commit-release avg: ",
            to_release["c"] * 1000 / to_release["c_count"],
            " ms",
        )


def main(args):
    parser = argparse.ArgumentParser(
        description="Compute average of interprocess times"
    )
    parser.add_argument("file", type=str, help="the timing log file to process")
    parser.add_argument(
        "--surface_id",
        type=int,
        help="the id of the surface to compute averages on",
    )
    args = parser.parse_args()
    log = args.file
    sid = args.surface_id
    stats = Stats()
    stats.read(log)
    # Print buffer stats for the sid with the most number of actions.
    if not sid:
        sid = next(iter(stats.surfaces.keys()))
        for key in stats.surfaces:
            if len(stats.surfaces[key]) > len(stats.surfaces[sid]):
                sid = key
    stats.print_stats(sid)


if __name__ == "__main__":
    main(sys.argv[1:])
