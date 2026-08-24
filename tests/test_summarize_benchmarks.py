import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SUMMARY_SCRIPT = ROOT / "summarize_benchmarks.py"


def write_result(path, logsz_values):
    path.parent.mkdir(parents=True, exist_ok=True)
    rows = [
        ["logsz", *logsz_values],
        ["off_comm", *[1000 + i for i, _ in enumerate(logsz_values)]],
        ["off_round", *[10 + i for i, _ in enumerate(logsz_values)]],
        ["off_time", *[1.0 + i for i, _ in enumerate(logsz_values)]],
        ["on_comm", *[100 + i for i, _ in enumerate(logsz_values)]],
        ["on_round", *[2 + i for i, _ in enumerate(logsz_values)]],
        ["on_time", *[0.1 + i for i, _ in enumerate(logsz_values)]],
    ]
    with path.open("w", newline="") as output:
        csv.writer(output).writerows(rows)


def add_variants(base, directory, n_party, logsz_values, variants):
    for protocol, target in variants:
        write_result(
            base / directory / f"{protocol}_{target}_n_{n_party}.csv",
            logsz_values,
        )


def create_fixture(base):
    add_variants(
        base,
        "semi_size",
        2,
        [14, 16, 18, 20, 22],
        [
            ("Chase_shuffle", "total_time"),
            ("Chase_shuffle", "on_time"),
            ("semi_my_shuffle", "total_time"),
        ],
    )
    for n_party in [3, 6, 9, 12, 15]:
        add_variants(
            base,
            "semi_parties",
            n_party,
            [16],
            [
                ("Chase_shuffle", "total_time"),
                ("Chase_shuffle", "on_time"),
                ("semi_my_shuffle", "total_time"),
            ],
        )

    add_variants(
        base,
        "mali_size",
        2,
        [10, 12, 14, 16, 18],
        [
            ("Song_shuffle", "total_time"),
            ("Song_shuffle", "on_time"),
            ("my_shuffle", "total_time"),
            ("my_shuffle_strong", "total_time"),
        ],
    )
    for n_party in [3, 6, 9, 12, 15]:
        add_variants(
            base,
            "mali_parties",
            n_party,
            [12],
            [
                ("Song_shuffle", "total_time"),
                ("Song_shuffle", "on_time"),
                ("my_shuffle", "total_time"),
                ("my_shuffle_strong", "total_time"),
            ],
        )

    write_result(
        base / "strong_size" / "my_shuffle_strong_total_time_n_2.csv",
        [10, 12, 14, 16, 18],
    )
    for n_party in [3, 6, 9, 12, 15]:
        write_result(
            base / "strong_parties" /
            f"my_shuffle_strong_total_time_n_{n_party}.csv",
            [12],
        )


class SummaryTests(unittest.TestCase):
    def test_all_summaries_are_complete_and_strict(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            create_fixture(base)
            env = os.environ.copy()
            env["SHUFFLE_BENCHMARK_BASE_DIR"] = str(base)

            for name in ["semi", "mali", "strong"]:
                subprocess.run(
                    [sys.executable, str(SUMMARY_SCRIPT), name, "--strict"],
                    check=True,
                    env=env,
                    capture_output=True,
                    text=True,
                )
                summary = base / f"{name}_summary.md"
                self.assertTrue(summary.is_file())
                self.assertNotIn("| NA |", summary.read_text())
                if name == "mali":
                    self.assertIn(
                        "| ours-strong | my_shuffle_strong |",
                        summary.read_text(),
                    )

            missing_mali = (
                base / "mali_parties" /
                "my_shuffle_strong_total_time_n_15.csv"
            )
            missing_mali.unlink()
            result = subprocess.run(
                [sys.executable, str(SUMMARY_SCRIPT), "mali", "--strict"],
                check=False,
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("my_shuffle_strong", result.stderr)

            missing = (
                base / "strong_parties" /
                "my_shuffle_strong_total_time_n_15.csv"
            )
            missing.unlink()
            result = subprocess.run(
                [sys.executable, str(SUMMARY_SCRIPT), "strong", "--strict"],
                check=False,
                env=env,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Missing result", result.stderr)


if __name__ == "__main__":
    unittest.main()
