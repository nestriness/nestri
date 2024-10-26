#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Daemon to regularly log swap-in/swap-out/page-in/page-out metrics."""

import argparse
import enum
import logging
import logging.handlers
import os
import re
import statistics
import subprocess
import time


# Set up startup and error logging to syslog.
logger = logging.getLogger("metric_collector")
_syslog_handler = logging.handlers.SysLogHandler(address="/dev/log")
_syslog_handler.append_nul = False
_syslog_handler.setFormatter(logging.Formatter(fmt=" %(name)s: %(message)s"))
logger.addHandler(_syslog_handler)
logger.setLevel(logging.INFO)

# Number of seconds between calls to garcon to report metrics.
INTERVAL = 5 * 60

# Path to file that includes metrics collected by mount-stateful.
MOUNT_STATEFUL_METRIC_FILE = "/tmp/.disk_metrics"

# Xrun counter field in /proc/asound/card0/pcm0p/sub0/status
XRUN_COUNTER_FIELD = "xrun_counter"

# Mapping from /proc/vmstat keys to ReportMetrics names.
VMSTAT_TO_METRIC = {
    "pswpin": "borealis-swap-kb-read",
    "pswpout": "borealis-swap-kb-written",
    "pgpgin": "borealis-disk-kb-read",
    "pgpgout": "borealis-disk-kb-written",
}

# Path to file that includes metrics collected by sommelier.
SOMMELIER_STATS_FILE = "/tmp/sommelier-stats"

# Steam game ids to ignore for metrics generation.
# 769 = ValveTestApp769
IGNORED_GAME_IDS = frozenset([0, 769])


class AudioDeviceType(enum.Enum):
    """Types of the audio device"""

    INPUT = enum.auto()
    OUTPUT = enum.auto()

    def __str__(self):
        return f"{self.name.lower()}"


def read_vmstat():
    """Read swap/disk IO counters from /proc/vmstat."""
    vmstat = {k: 0 for k in VMSTAT_TO_METRIC}
    with open("/proc/vmstat", encoding="utf-8") as proc_vmstat:
        for line in proc_vmstat:
            k, v = line.split()
            if k in vmstat:
                vmstat[k] = int(v)
    logger.debug("Values from /proc/vmstat: %s", vmstat)
    return vmstat


def read_meminfo():
    """Read stats from /proc/meminfo"""
    meminfo_dict = {}
    with open("/proc/meminfo", encoding="utf-8") as meminfo:
        for line in meminfo:
            line_split = line.strip().split()
            meminfo_dict[line_split[0].rstrip(":")] = int(line_split[1])
    return meminfo_dict


def read_pcminfo(device_type, device_idx):
    """Read stats from /proc/asound/card0/pcm{device_idx}{p|c}/info"""
    pcm_type = "p" if device_type == AudioDeviceType.OUTPUT else "c"
    pcminfo_dict = {}
    with open(
        f"/proc/asound/card0/pcm{device_idx}{pcm_type}/info", encoding="utf-8"
    ) as pcminfo:
        for line in pcminfo:
            key, value = map(str.strip, line.split(":"))
            if key in ["subdevices_count", "subdevices_avail"]:
                pcminfo_dict[key] = int(value)
    return pcminfo_dict


def read_pcminfos():
    """Read pcminfo stats from all devices"""
    pcminfos = {}
    for device_type in AudioDeviceType:
        pcminfos[device_type] = []
        for device_idx in range(3):
            pcminfos[device_type].append(read_pcminfo(device_type, device_idx))
    return pcminfos


def read_pcm_subdevice_status(subdevice):
    """Read stats from /proc/asound/card0/pcm0p/sub0/status"""
    status_dict = {}
    with open(
        f"/proc/asound/card0/pcm0p/sub{subdevice}/status", encoding="utf-8"
    ) as subdevice_status:
        for line in subdevice_status:
            m = re.fullmatch(r"([^ ]+) *: (\d+)\s*", line)
            if m:
                status_dict[m.group(1)] = int(m.group(2))
    return status_dict


def read_sommelier_stats():
    """Read sommelier frame timing statistics."""
    recent = []
    FLOAT_KEYS = frozenset(
        [
            "min_fps",
            "mean_fps",
            "max_fps",
            "p0_1",
            "p1_0",
            "p50",
            "p99",
            "variance",
            "bucket_size",
        ]
    )
    try:
        with open(SOMMELIER_STATS_FILE, encoding="utf-8") as f:
            # Read the header line which determines the order of parameters.
            # Histogram must be at the end and will consume the rest of the values.
            hdr = f.readline().rstrip().split()
            histogram = False
            if hdr[-1] == "histogram...":
                hdr.pop()
                histogram = True
            # Parse each line based on the header.
            for line in f:
                values = line.rstrip().split()
                # Generate an entry by zipping the keys with the header
                # performing type conversion.
                entry = {
                    k: float(v) if k in FLOAT_KEYS else int(v)
                    for k, v in zip(hdr, values[: len(hdr)])
                }
                if histogram:
                    entry["histogram"] = [int(v) for v in values[len(hdr) :]]
                recent.append(entry)
    except FileNotFoundError:
        logger.error(
            "unable to open sommelier stats file %s",
            SOMMELIER_STATS_FILE,
        )
    return recent


def send_metrics(metrics, dry_run):
    """Send |metrics|, a list of args, to garcon to be reported to Chrome and UMA."""
    try:
        args = [
            "/opt/google/cros-containers/bin/garcon",
            "--client",
            "--metrics",
            ",".join(metrics),
        ]
        if dry_run:
            print(f"Would have called: {args}")
        else:
            logger.debug("Calling: %s", args)
            subprocess.check_call(args)
        return 0
    except subprocess.CalledProcessError as e:
        logger.error(
            "garcon failed to report metrics: %s, error code: %s",
            metrics,
            e.returncode,
        )
        return e.returncode


def collect_diff(metrics, vmstat, laststat):
    """Create a list of incremental disk/swap activity to send to the host."""
    # Calculate incremental activity.
    diff = {k: vmstat[k] - laststat[k] for k in vmstat.keys()}
    logger.debug("Incremental activity: %s", diff)
    # Turn it into a list of args to pass to garcon.
    for vmstat_id, metric_id in sorted(VMSTAT_TO_METRIC.items()):
        if diff[vmstat_id]:
            # We report metric values in KiB; multiply by 4 to convert from
            # /proc/vmstat counts, which are 4096-byte blocks.
            metric_diff_kib = diff[vmstat_id] * 4
            metrics.append(f"{metric_id}={metric_diff_kib}")


def collect_dirty_pages(metrics):
    """Collects and adds the current dirty pages, in KB, to |metrics|."""
    meminfo = read_meminfo()
    metrics.append("borealis-dirty-pages=" + str(meminfo["Dirty"]))


def collect_used_audio_subdevices(metrics, pcminfo):
    """Collects and adds the current used subdevices to |metrics|."""
    used_subdevices = pcminfo["subdevices_count"] - pcminfo["subdevices_avail"]
    if used_subdevices > 0:
        metrics.append("borealis-audio-used-subdevices=" + str(used_subdevices))


def collect_direct_alsa_xrun(metrics, audiostat, subdevices):
    """Collects the increment of xrun with direct alsa"""
    diff = 0
    found_active_stream = False
    for subdevice in range(subdevices):
        key = f"{XRUN_COUNTER_FIELD}_{subdevice}"
        substream_status = read_pcm_subdevice_status(subdevice)
        if XRUN_COUNTER_FIELD not in substream_status:
            audiostat[key] = 0
            continue

        found_active_stream = True
        current_xrun = substream_status.get(XRUN_COUNTER_FIELD, 0)
        # When the stream resets, the xrun_counter is reset
        if current_xrun < audiostat[key]:
            audiostat[key] = 0
        diff += current_xrun - audiostat[key]
        audiostat[key] = current_xrun
    if found_active_stream:
        metrics.append(f"borealis-audio-xrun-alsa-output={diff}")


def collect_audio_path_usage(metrics, pcminfos):
    """Collect and report metrics from usage of each audio path"""
    for device_type in AudioDeviceType:
        metric_key = f"borealis-audio-used-path-{device_type}"
        for device_idx in range(len(pcminfos[device_type])):
            pcminfo = pcminfos[device_type][device_idx]
            if pcminfo["subdevices_count"] != pcminfo["subdevices_avail"]:
                metrics.append(f"{metric_key}={device_idx}")


def combine_sommelier_periods(entries):
    """Summarize a set of sommelier statistic periods."""

    def getdv(key):
        """Helper to get all values of key from the list of entries."""
        return [d[key] for d in entries]

    summary = {}
    num_frames = sum(getdv("num_frames"))
    if num_frames:
        # For simplicity just take the mean across relevant time periods
        # since each should be of the same amount of time.
        # The game id reported might switch to/from 0, so ignore that as
        # a game id.
        game_ids = set(getdv("steam_game_id")).difference({0})
        # Return a single game id, otherwise assume we don't know it.
        summary["steam_game_id"] = (
            list(game_ids)[0] if len(game_ids) == 1 else 0
        )
        summary["num_frames"] = num_frames
        summary["activated_frames"] = sum(getdv("num_activated"))
        summary["mean_fps"] = statistics.mean(getdv("mean_fps"))
        summary["p1_0"] = statistics.mean(getdv("p1_0"))
        summary["variance"] = statistics.mean(getdv("variance"))
        slow_frames = sum(getdv("num_slow_frames"))
        summary["ratio_slow_frames"] = slow_frames / num_frames
    return summary


def add_sommelier_metrics(metrics, metric_base, summary):
    """Add the sommelier summary statistics to a given base key."""
    if summary:
        metric_key = f"{metric_base}-fps-mean"
        metrics.append(f"{metric_key}={round(summary['mean_fps'])}")
        metric_key = f"{metric_base}-fps-low"
        metrics.append(f"{metric_key}={round(summary['p1_0'])}")
        metric_key = f"{metric_base}-fps-variance"
        metrics.append(f"{metric_key}={round(summary['variance'])}")
        metric_key = f"{metric_base}-ratio-slow"
        metrics.append(
            f"{metric_key}={round(100*summary['ratio_slow_frames'])}"
        )


# TODO(davidriley): metrics is unused at this time.
# pylint: disable=unused-argument
def collect_sommelier_stats(metrics, sommelier_stats, last_sommelier):
    """Collect the recent metrics from sommelier-stats."""
    # Since sommelier-stats is logged more frequently than the metrics
    # collector, multiple time periods need to be processed.  The file
    # is guaranteed to be written out atomically, and there is a large
    # delay between metrics collector runs, so can simply just combine
    # based on what is new.

    # Filter out any reports that were previously reported.
    last_report = 0
    if last_sommelier:
        last_report = max([int(e["timestamp"]) for e in last_sommelier])
    new_reports = [
        e for e in sommelier_stats if int(e["timestamp"]) > last_report
    ]
    logger.debug(
        "%d new sommelier-stats periods since %d", len(new_reports), last_report
    )

    # Handle each surface separately.
    surface_stats = {}
    for e in new_reports:
        surface_stats.setdefault(e["surface_id"], []).append(e)
    notable_entries = []
    for entries in surface_stats.values():
        # Combine all the periods for a given surface.
        summary = combine_sommelier_periods(entries)
        if summary.get("activated_frames") == 0 or summary.get(
            "activated_frames"
        ) != summary.get("num_frames"):
            # Ignore the surface if it was not fully activated.
            continue

        # After summarizing the surface entries, track it in the generic
        # full report if:
        # - the window was activated for all frames
        # - the game id is present and not on the ignored list
        game_id = summary.get("steam_game_id")
        if game_id and game_id not in IGNORED_GAME_IDS:
            notable_entries.extend(entries)

        if game_id:
            add_sommelier_metrics(
                metrics, f"borealis-frames-game{game_id}", summary
            )

    # Summarize the notable entries.  This is done as a second full pass to
    # try and evenly account for each period since each is the same amount of
    # time. (ie avoid averages of averages).
    summary = combine_sommelier_periods(notable_entries)

    add_sommelier_metrics(metrics, "borealis-frames", summary)


def report_mount_stateful_metrics(dry_run):
    """Collect the metrics from the mount-stateful init job and send them to garcon."""
    if not os.path.isfile(MOUNT_STATEFUL_METRIC_FILE):
        logger.error("couldn't detect .disk_metrics file")
        return
    with open(MOUNT_STATEFUL_METRIC_FILE, encoding="utf-8") as disk_metrics:
        metrics = disk_metrics.read().splitlines()
        # Error code 1 denotes an RPC error, which is likely caused by
        # the garcon to cicerone channel not being ready yet.
        while send_metrics(metrics, dry_run) == 1:
            time.sleep(5)
    os.remove(MOUNT_STATEFUL_METRIC_FILE)


def collect_and_report_metrics_forever(
    testonly_single_run=False, dry_run=False, include_sommelier=False
):
    """Collect and report metrics every INTERVAL seconds."""
    logger.info("Borealis metric collector started")
    laststat = {}
    num_subdevices = 10
    audiostat = {f"{XRUN_COUNTER_FIELD}_{i}": 0 for i in range(num_subdevices)}
    last_sommelier = []
    while 1:
        vmstat = read_vmstat()
        metrics = []
        if laststat:
            # We have at least two samples; report the diff to the host.
            collect_diff(metrics, vmstat, laststat)
        if testonly_single_run:
            # Exit immediately if we're just doing a dry run test.
            break
        collect_dirty_pages(metrics)
        pcminfos = read_pcminfos()
        # We only care about the device that is used for playback by proton
        collect_used_audio_subdevices(
            metrics, pcminfos[AudioDeviceType.OUTPUT][0]
        )
        collect_direct_alsa_xrun(metrics, audiostat, num_subdevices)
        collect_audio_path_usage(metrics, pcminfos)

        if include_sommelier:
            sommelier_stats = read_sommelier_stats()
            collect_sommelier_stats(metrics, sommelier_stats, last_sommelier)
            last_sommelier = sommelier_stats
        if metrics:
            # Reports metrics to garcon/host if we have any.
            send_metrics(metrics, dry_run)
        laststat = vmstat
        time.sleep(INTERVAL)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test-single-run",
        action="store_true",
        help="Exit after a single run, for testing.",
    )
    parser.add_argument(
        "--test-disable-syslog",
        action="store_true",
        help="Disable syslog output, for testing.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose logging to stderr.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Enable dry-run that doesn't actually send metrics.",
    )
    parser.add_argument(
        "--sommelier",
        action="store_true",
        help="Enable processing of sommelier-stats.",
    )
    args = parser.parse_args()

    if args.test_disable_syslog:
        # Disable the syslog handler, which doesn't work on Docker.
        logger.removeHandler(_syslog_handler)

    if args.verbose:
        # Log to stderr as well as syslog, and increase verbosity.
        stderr_handler = logging.StreamHandler()
        logger.addHandler(stderr_handler)
        logger.setLevel(logging.DEBUG)

    # Blocks starting up until garcon is ready to handle metrics.
    if not args.test_single_run:
        report_mount_stateful_metrics(args.dry_run)

    collect_and_report_metrics_forever(
        args.test_single_run, args.dry_run, args.sommelier
    )


if __name__ == "__main__":
    main()
