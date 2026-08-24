import argparse
import csv
import math
import os


SCHEMA_VERSION = 2
RTT_SWEEP_MS = (0.5, 1.0, 5.0, 20.0, 60.0, 100.0)
BANDWIDTH_SWEEP_MBPS = (12.5, 80.0, 125.0, 312.5, 1250.0)
SWEEP_SETTINGS = tuple(
    ("rtt", 80.0, rtt_ms) for rtt_ms in RTT_SWEEP_MS
) + tuple(
    ("bandwidth", bandwidth_mbps, 0.5)
    for bandwidth_mbps in BANDWIDTH_SWEEP_MBPS
)

RAW_FIELDS = {
    "schema_version",
    "protocol",
    "target",
    "n_party",
    "logsz",
    "veclen",
    "logbatch",
    "attempt",
    "status",
    "off_comm_bytes",
    "off_rounds",
    "off_compute_seconds",
    "on_comm_bytes",
    "on_rounds",
    "on_compute_seconds",
}

OUTPUT_FIELDS = [
    "schema_version",
    "protocol",
    "target",
    "n_party",
    "logsz",
    "veclen",
    "selected_logbatch",
    "source_attempt",
    "sweep",
    "bandwidth_MBps",
    "rtt_ms",
    "off_comm_bytes",
    "off_rounds",
    "off_compute_seconds",
    "off_comm_seconds",
    "off_latency_seconds",
    "off_modeled_seconds",
    "on_comm_bytes",
    "on_rounds",
    "on_compute_seconds",
    "on_comm_seconds",
    "on_latency_seconds",
    "on_modeled_seconds",
    "total_modeled_seconds",
]


def modeled_phase_components(compute_seconds, comm_bytes, rounds,
                             bandwidth_mbps, rtt_ms):
    if not all(math.isfinite(value) for value in (
            compute_seconds, bandwidth_mbps, rtt_ms)):
        raise ValueError("compute time, bandwidth, and RTT must be finite")
    if compute_seconds < 0 or comm_bytes < 0 or rounds < 0:
        raise ValueError("compute time, communication, and rounds must be non-negative")
    if bandwidth_mbps <= 0 or rtt_ms < 0:
        raise ValueError("bandwidth must be positive and RTT must be non-negative")
    comm_seconds = comm_bytes / (bandwidth_mbps * 1_000_000)
    latency_seconds = rounds * rtt_ms / 1_000
    return comm_seconds, latency_seconds, (
        compute_seconds + comm_seconds + latency_seconds
    )


def modeled_phase_time(compute_seconds, comm_bytes, rounds,
                       bandwidth_mbps, rtt_ms):
    return modeled_phase_components(
        compute_seconds,
        comm_bytes,
        rounds,
        bandwidth_mbps,
        rtt_ms,
    )[2]


def read_measurements(path):
    with open(path, newline="") as source:
        reader = csv.DictReader(source)
        missing = RAW_FIELDS - set(reader.fieldnames or [])
        if missing:
            raise ValueError(
                f"{path} is not a v2 measurement file; missing: "
                + ", ".join(sorted(missing))
            )

        latest = {}
        for row in reader:
            if int(row["schema_version"]) != SCHEMA_VERSION:
                raise ValueError(f"unsupported schema version in {path}")
            if row["status"] != "ok":
                continue
            measurement = {
                "protocol": row["protocol"],
                "target": row["target"],
                "n_party": int(row["n_party"]),
                "logsz": int(row["logsz"]),
                "veclen": int(row["veclen"]),
                "logbatch": int(row["logbatch"]),
                "attempt": int(row["attempt"]),
                "off_comm_bytes": int(row["off_comm_bytes"]),
                "off_rounds": int(row["off_rounds"]),
                "off_compute_seconds": float(row["off_compute_seconds"]),
                "on_comm_bytes": int(row["on_comm_bytes"]),
                "on_rounds": int(row["on_rounds"]),
                "on_compute_seconds": float(row["on_compute_seconds"]),
            }
            key = tuple(measurement[name] for name in (
                "protocol", "target", "n_party", "logsz", "veclen", "logbatch"
            ))
            latest[key] = measurement
    return list(latest.values())


def derive_candidate(measurement, sweep, bandwidth_mbps, rtt_ms):
    off_comm, off_latency, off_modeled = modeled_phase_components(
        measurement["off_compute_seconds"],
        measurement["off_comm_bytes"],
        measurement["off_rounds"],
        bandwidth_mbps,
        rtt_ms,
    )
    on_comm, on_latency, on_modeled = modeled_phase_components(
        measurement["on_compute_seconds"],
        measurement["on_comm_bytes"],
        measurement["on_rounds"],
        bandwidth_mbps,
        rtt_ms,
    )
    return {
        "schema_version": 1,
        "protocol": measurement["protocol"],
        "target": measurement["target"],
        "n_party": measurement["n_party"],
        "logsz": measurement["logsz"],
        "veclen": measurement["veclen"],
        "selected_logbatch": measurement["logbatch"],
        "source_attempt": measurement["attempt"],
        "sweep": sweep,
        "bandwidth_MBps": bandwidth_mbps,
        "rtt_ms": rtt_ms,
        "off_comm_bytes": measurement["off_comm_bytes"],
        "off_rounds": measurement["off_rounds"],
        "off_compute_seconds": measurement["off_compute_seconds"],
        "off_comm_seconds": off_comm,
        "off_latency_seconds": off_latency,
        "off_modeled_seconds": off_modeled,
        "on_comm_bytes": measurement["on_comm_bytes"],
        "on_rounds": measurement["on_rounds"],
        "on_compute_seconds": measurement["on_compute_seconds"],
        "on_comm_seconds": on_comm,
        "on_latency_seconds": on_latency,
        "on_modeled_seconds": on_modeled,
        "total_modeled_seconds": off_modeled + on_modeled,
    }


def derive_sweep_rows(measurements):
    best = {}
    for measurement in measurements:
        target = measurement["target"]
        if target not in ("total_time", "on_time"):
            raise ValueError(f"unsupported optimization target: {target}")
        for sweep, bandwidth_mbps, rtt_ms in SWEEP_SETTINGS:
            candidate = derive_candidate(
                measurement, sweep, bandwidth_mbps, rtt_ms)
            key = tuple(candidate[name] for name in (
                "protocol", "target", "n_party", "logsz", "veclen",
                "sweep", "bandwidth_MBps", "rtt_ms"
            ))
            score_key = (
                "total_modeled_seconds" if target == "total_time"
                else "on_modeled_seconds"
            )
            score = (candidate[score_key], candidate["selected_logbatch"])
            current = best.get(key)
            if current is None or score < current[0]:
                best[key] = (score, candidate)

    sweep_order = {"rtt": 0, "bandwidth": 1}
    return sorted(
        (entry[1] for entry in best.values()),
        key=lambda row: (
            row["protocol"], row["target"], row["n_party"], row["logsz"],
            row["veclen"], sweep_order[row["sweep"]], row["rtt_ms"],
            row["bandwidth_MBps"],
        ),
    )


def validate_sweep_rows(rows):
    groups = {}
    for row in rows:
        group_key = tuple(row[name] for name in (
            "protocol", "target", "n_party", "logsz", "veclen"
        ))
        groups.setdefault(group_key, []).append(row)

        for prefix in ("off", "on"):
            expected = (
                row[f"{prefix}_compute_seconds"]
                + row[f"{prefix}_comm_seconds"]
                + row[f"{prefix}_latency_seconds"]
            )
            if not math.isclose(
                    expected, row[f"{prefix}_modeled_seconds"],
                    rel_tol=1e-12, abs_tol=1e-12):
                raise ValueError(f"invalid {prefix} time decomposition")
        if not math.isclose(
                row["off_modeled_seconds"] + row["on_modeled_seconds"],
                row["total_modeled_seconds"],
                rel_tol=1e-12, abs_tol=1e-12):
            raise ValueError("invalid total time decomposition")

    if not groups:
        raise ValueError("no successful measurements")

    expected_rtt = {(80.0, value) for value in RTT_SWEEP_MS}
    expected_bandwidth = {(value, 0.5) for value in BANDWIDTH_SWEEP_MBPS}
    for key, group in groups.items():
        rtt_rows = [row for row in group if row["sweep"] == "rtt"]
        bandwidth_rows = [row for row in group if row["sweep"] == "bandwidth"]
        if {(row["bandwidth_MBps"], row["rtt_ms"]) for row in rtt_rows} != expected_rtt:
            raise ValueError(f"incomplete RTT sweep for {key}")
        if {(row["bandwidth_MBps"], row["rtt_ms"]) for row in bandwidth_rows} != expected_bandwidth:
            raise ValueError(f"incomplete bandwidth sweep for {key}")

        metric = (
            "total_modeled_seconds" if group[0]["target"] == "total_time"
            else "on_modeled_seconds"
        )
        rtt_times = [
            row[metric]
            for row in sorted(rtt_rows, key=lambda row: row["rtt_ms"])
        ]
        if any(right < left for left, right in zip(rtt_times, rtt_times[1:])):
            raise ValueError(f"non-monotonic RTT sweep for {key}")

        bandwidth_times = [
            row[metric]
            for row in sorted(
                bandwidth_rows, key=lambda row: row["bandwidth_MBps"])
        ]
        if any(right > left for left, right in zip(
                bandwidth_times, bandwidth_times[1:])):
            raise ValueError(f"non-monotonic bandwidth sweep for {key}")


def write_network_sweeps(input_path, output_path, strict=True):
    rows = derive_sweep_rows(read_measurements(input_path))
    if strict:
        validate_sweep_rows(rows)
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", newline="") as destination:
        writer = csv.DictWriter(destination, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    return rows


def main():
    parser = argparse.ArgumentParser(
        description="Derive bandwidth/RTT sweeps from network-independent measurements.")
    parser.add_argument("input", help="raw_measurements_v2.csv")
    parser.add_argument(
        "--output",
        help="output CSV (default: network_sweep.csv next to the input)",
    )
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()
    output = args.output or os.path.join(
        os.path.dirname(args.input), "network_sweep.csv")
    rows = write_network_sweeps(args.input, output, strict=args.strict)
    print(f"Wrote {len(rows)} rows to {output}")


if __name__ == "__main__":
    main()
