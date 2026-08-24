import csv
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from derive_network_sweeps import (  # noqa: E402
    modeled_phase_components,
    validate_sweep_rows,
    write_network_sweeps,
)


RAW_FIELDS = [
    "schema_version", "protocol", "target", "n_party", "logsz",
    "veclen", "logbatch", "attempt", "status", "elapsed_wall_time",
    "off_comm_bytes", "off_rounds", "off_compute_seconds",
    "on_comm_bytes", "on_rounds", "on_compute_seconds", "reason",
]


def measurement(logbatch, compute, comm, rounds, target="total_time"):
    return {
        "schema_version": 2,
        "protocol": "my_shuffle",
        "target": target,
        "n_party": 3,
        "logsz": 12,
        "veclen": 1,
        "logbatch": logbatch,
        "attempt": 1,
        "status": "ok",
        "elapsed_wall_time": 0,
        "off_comm_bytes": 0,
        "off_rounds": 0,
        "off_compute_seconds": 0,
        "on_comm_bytes": comm,
        "on_rounds": rounds,
        "on_compute_seconds": compute,
        "reason": "",
    }


class NetworkSweepTests(unittest.TestCase):
    def test_decimal_bandwidth_and_rtt_formula(self):
        comm, latency, total = modeled_phase_components(
            1.0, 80_000_000, 2, 80.0, 60.0)
        self.assertEqual(comm, 1.0)
        self.assertEqual(latency, 0.12)
        self.assertAlmostEqual(total, 2.12)

    def test_writes_both_sweeps_and_selects_logbatch_per_setting(self):
        rows = [
            measurement(4, 0.0, 100_000_000, 0),
            measurement(5, 1.0, 0, 0),
            measurement(6, 0.2, 0, 100),
        ]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw_measurements_v2.csv"
            output = Path(directory) / "network_sweep.csv"
            with source.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=RAW_FIELDS)
                writer.writeheader()
                writer.writerows(rows)

            derived = write_network_sweeps(source, output, strict=True)
            self.assertEqual(len(derived), 11)
            self.assertTrue(output.is_file())

            selected = {
                (row["sweep"], row["bandwidth_MBps"], row["rtt_ms"]):
                row["selected_logbatch"]
                for row in derived
            }
            self.assertEqual(selected[("rtt", 80.0, 0.5)], 6)
            self.assertEqual(selected[("rtt", 80.0, 100.0)], 5)
            self.assertEqual(selected[("bandwidth", 12.5, 0.5)], 6)
            self.assertEqual(selected[("bandwidth", 1250.0, 0.5)], 4)
            validate_sweep_rows(derived)

    def test_rejects_negative_measurements(self):
        with self.assertRaises(ValueError):
            modeled_phase_components(0.1, -1, 2, 80, 60)

    def test_on_time_target_ignores_offline_time(self):
        slow_offline = measurement(4, 0.1, 0, 0, target="on_time")
        slow_offline["off_compute_seconds"] = 100
        fast_offline = measurement(5, 1.0, 0, 0, target="on_time")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "raw_measurements_v2.csv"
            output = Path(directory) / "network_sweep.csv"
            with source.open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=RAW_FIELDS)
                writer.writeheader()
                writer.writerows([slow_offline, fast_offline])
            derived = write_network_sweeps(source, output, strict=True)
            self.assertEqual(
                {row["selected_logbatch"] for row in derived}, {4})


if __name__ == "__main__":
    unittest.main()
