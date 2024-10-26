#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Prints the Proton versions used in recent Proton game sessions"""

import argparse
from datetime import datetime
from datetime import timedelta
from datetime import timezone
import os
import re
import sys
from typing import NamedTuple


_DEFAULT_TIMEFRAME = 5  # in minutes
_LAUNCH_LOG = "/tmp/launch-log.txt"


class GameSession(NamedTuple):
    """Stores version information of a Proton game session"""

    steam_id: str
    proton: str
    slr: str  # SLR = Steam Linux Runtime
    timestamp: datetime

    def print(self):
        """Logs out the version information for a game session."""

        assert self.timestamp, "Malformed game session: no timestamp"
        assert self.steam_id, "Malformed game session: no steam_id"

        print(
            f"GameID: {self.steam_id}, Proton: {self.proton}, "
            f"SLR: {self.slr}, Timestamp: {self.timestamp}",
            file=sys.stdout,
        )


class LaunchLog:
    """Manages access to the Borealis launch wrapper launch logs.

    The member _reversed_game_sessions maintains a stateful iterator point to the
    next log line, starting from the end of the launch log file.
    """

    def __init__(self, game_session_log):
        if os.path.isfile(game_session_log):
            with open(game_session_log, "r") as f:
                self._reversed_game_sessions = reversed(f.readlines())
        else:
            self._reversed_game_sessions = []

    def get_prev_session(self):
        """Retrieves the compat tool version info of the last game session logged

        Must match the launch wrapper log format:
        <timestamp> INFO /usr/bin/launch-wrap.sh/<steam_id>[pid]: Running command: <steam_cmd>

        The regex in this function should match the launch-wrapper log format.
        The log format is tested in the launch-wrapper unit tests.
        """

        timestamp = None
        steam_id = None
        proton = None
        slr = None

        for line in self._reversed_game_sessions:
            if "Running command:" not in line:
                continue

            m = re.match(r"([^ ]+).*launch-wrap.sh/(\d+)", line)
            assert m, "Unable to parse timestamp and steam_id"
            timestamp = datetime.fromisoformat(m.group(1))
            steam_id = m.group(2)

            if m := re.match(r".*(SteamLinuxRuntime[^\'/]+)", line):
                slr = m.group(1)

            if m := re.match(r".*(Proton[^\'/]+)", line):
                proton = m.group(1)

            yield GameSession(steam_id, proton, slr, timestamp)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        "--since",
        action="store",
        type=int,
        help="The lookback timeframe to search in minutes."
        "A value of 0 indicates an unlimited timeframe",
        default=_DEFAULT_TIMEFRAME,
    )
    parser.add_argument(
        "--game-session-log",
        action="store",
        type=str,
        help="The log file to parse for game sessions",
        default=_LAUNCH_LOG,
    )
    args = parser.parse_args()

    cutoff_time = None
    if args.since != 0:
        now = datetime.now(timezone.utc)
        cutoff_time = now - timedelta(minutes=args.since)

    launch_log = LaunchLog(args.game_session_log)

    # The loop below should:
    # 1. Ensure nothing is printed when no sessions at all are found.
    # 2. Ensure there is always at least one session printed, even if outside
    #    the lookback timeframe. This is meant to service feedback reports
    #    with game sessions lasting longer than the lookback timeframe.
    # 3. Print version info for all sessions in the lookback timeframe. This is
    #    meant to service issues when a game session or series of game sessions
    #    failed to start.
    at_least_one_printed = False
    for game_session in launch_log.get_prev_session():
        if (
            at_least_one_printed
            and cutoff_time
            and game_session.timestamp < cutoff_time
        ):
            break
        game_session.print()
        at_least_one_printed = True
