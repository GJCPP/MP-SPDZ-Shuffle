import csv
import os
import sys


BASE_DIR = os.environ.get("SHUFFLE_BENCHMARK_BASE_DIR", "benchmark_results")
METRIC_KEYS = ["off_comm", "off_round", "off_time", "on_comm", "on_round", "on_time"]


def read_result(path):
    rows = {}
    if not os.path.isfile(path):
        return None
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if not row:
                continue
            key = row[0].strip()
            values = [x.strip() for x in row[1:] if x.strip()]
            rows[key] = values
    logsz = [int(x) for x in rows.get("logsz", [])]
    off_round = rows.get("off_round", [])
    on_round = rows.get("on_round", [])
    return {
        "logsz": logsz,
        "off_comm": [int(x) for x in rows.get("off_comm", [])],
        "off_round": [int(x) for x in off_round] if off_round else ["NA"] * len(logsz),
        "off_time": [float(x) for x in rows.get("off_time", [])],
        "on_comm": [int(x) for x in rows.get("on_comm", [])],
        "on_round": [int(x) for x in on_round] if on_round else ["NA"] * len(logsz),
        "on_time": [float(x) for x in rows.get("on_time", [])],
    }


def metric_at(result, logsz):
    if result is None or logsz not in result["logsz"]:
        return None
    i = result["logsz"].index(logsz)
    return {
        "off_comm": result["off_comm"][i],
        "off_round": result["off_round"][i],
        "off_time": result["off_time"][i],
        "on_comm": result["on_comm"][i],
        "on_round": result["on_round"][i],
        "on_time": result["on_time"][i],
    }


def result_file(directory, protocol, target, n_party):
    return os.path.join(BASE_DIR, directory, f"{protocol}_{target}_n_{n_party}.csv")


def add_rows(rows, label, directory, points, baseline, ours,
             extra_variants=()):
    variants = [
        ("baseline-total", baseline, "total_time"),
        ("baseline-online", baseline, "on_time"),
        ("ours", ours, "total_time"),
    ] + list(extra_variants)
    for point_label, n_party, logsz in points:
        for variant, protocol, target in variants:
            metrics = metric_at(read_result(result_file(directory, protocol, target, n_party)), logsz)
            row = {
                "scale": label,
                "point": point_label,
                "variant": variant,
                "protocol": protocol,
            }
            if metrics is None:
                row.update({k: "NA" for k in METRIC_KEYS})
            else:
                row.update(metrics)
            rows.append(row)


def add_single_protocol_rows(rows, label, directory, points, variant, protocol):
    for point_label, n_party, logsz in points:
        metrics = metric_at(
            read_result(result_file(directory, protocol, "total_time", n_party)),
            logsz,
        )
        row = {
            "scale": label,
            "point": point_label,
            "variant": variant,
            "protocol": protocol,
        }
        if metrics is None:
            row.update({k: "NA" for k in METRIC_KEYS})
        else:
            row.update(metrics)
        rows.append(row)


def format_value(value):
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def write_markdown(name, rows):
    out_path = os.path.join(BASE_DIR, f"{name}_summary.md")
    os.makedirs(BASE_DIR, exist_ok=True)
    headers = ["scale", "point", "variant", "protocol", "off_comm", "off_round", "off_time", "on_comm", "on_round", "on_time"]
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join(["---"] * len(headers)) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(format_value(row[h]) for h in headers) + " |")
    content = "\n".join(lines) + "\n"
    with open(out_path, "w") as f:
        f.write(content)
    print(content)
    print(f"Wrote {out_path}")


def missing_rows(rows):
    return [
        row for row in rows
        if any(row[key] == "NA" for key in METRIC_KEYS)
    ]


def build_summary(name):
    if name == "semi":
        rows = []
        add_rows(
            rows,
            "size",
            "semi_size",
            [(f"n=2, logsz={logsz}", 2, logsz) for logsz in [14, 16, 18, 20, 22]],
            "Chase_shuffle",
            "semi_my_shuffle",
        )
        add_rows(
            rows,
            "parties",
            "semi_parties",
            [(f"n={n_party}, logsz=16", n_party, 16) for n_party in [3, 6, 9, 12, 15]],
            "Chase_shuffle",
            "semi_my_shuffle",
        )
        return rows
    if name in ("mali", "malicious"):
        rows = []
        add_rows(
            rows,
            "size",
            "mali_size",
            [(f"n=2, logsz={logsz}", 2, logsz) for logsz in [10, 12, 14, 16, 18]],
            "Song_shuffle",
            "my_shuffle",
            [("ours-strong", "my_shuffle_strong", "total_time")],
        )
        add_rows(
            rows,
            "parties",
            "mali_parties",
            [(f"n={n_party}, logsz=12", n_party, 12) for n_party in [3, 6, 9, 12, 15]],
            "Song_shuffle",
            "my_shuffle",
            [("ours-strong", "my_shuffle_strong", "total_time")],
        )
        return rows
    if name == "strong":
        rows = []
        add_single_protocol_rows(
            rows,
            "size",
            "strong_size",
            [(f"n=2, logsz={logsz}", 2, logsz) for logsz in [10, 12, 14, 16, 18]],
            "strong",
            "my_shuffle_strong",
        )
        add_single_protocol_rows(
            rows,
            "parties",
            "strong_parties",
            [(f"n={n_party}, logsz=12", n_party, 12) for n_party in [3, 6, 9, 12, 15]],
            "strong",
            "my_shuffle_strong",
        )
        return rows
    raise ValueError(f"unknown summary: {name}")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != "--strict"):
        print(f"Usage: {sys.argv[0]} <semi|mali|strong> [--strict]")
        sys.exit(1)
    rows = build_summary(sys.argv[1])
    missing = missing_rows(rows)
    if len(sys.argv) == 3 and missing:
        for row in missing:
            print(
                f"Missing result: {row['scale']} / {row['point']} / "
                f"{row['variant']} / {row['protocol']}",
                file=sys.stderr,
            )
        sys.exit(1)
    write_markdown(sys.argv[1], rows)
