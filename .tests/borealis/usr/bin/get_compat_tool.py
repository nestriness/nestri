#!/usr/bin/env python3

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Print compat tool set for the game"""

import argparse
import sys
import vdf

COMPAT_CONFIG_PATH = "/home/chronos/.steam/steam/config/config.vdf"


def _DictDFS(find_dict: dict, target_key: str):
    """Recursively search for the key in the nested dictionary."""
    if target_key in find_dict:
        return find_dict[target_key]

    # Find the nested dictionaries and search with the key
    for maybe_nested_dict in find_dict.values():
        if isinstance(maybe_nested_dict, dict):
            result = _DictDFS(maybe_nested_dict, target_key)
            if result:
                return result

    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--game-id", type=int, help="Steam game ID to query", required=True
    )
    parser.add_argument(
        "--config-path",
        help="Path to config.vdf file to parse",
        default=COMPAT_CONFIG_PATH,
    )
    args = parser.parse_args()

    with open(args.config_path, encoding="utf-8") as f:
        config_dict = vdf.load(f)
        compat_dict = _DictDFS(config_dict, "CompatToolMapping")
        game_id_str = str(args.game_id)
        if (
            compat_dict
            and game_id_str in compat_dict
            and "name" in compat_dict[game_id_str]
        ):
            print(compat_dict[game_id_str]["name"])
            sys.exit(0)
    sys.exit(1)
