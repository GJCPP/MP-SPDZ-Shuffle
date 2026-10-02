"""Real multi-party round regressions; enable with SHUFFLE_ROUND_INTEGRATION=1."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROTOCOLS = ("my_shuffle", "Song_shuffle", "semi_my_shuffle", "Chase_shuffle")


@unittest.skipUnless(os.environ.get("SHUFFLE_ROUND_INTEGRATION") == "1",
                     "run make test-shuffle-rounds for multi-party integration")
class ShuffleRoundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="shuffle-round-tests-")
        cls.base = Path(cls.temporary.name)
        cls.binary = Path(os.environ.get(
            "SHUFFLE_TEST_BINARY", str(ROOT / "build/my_shuffle_main.x"))).resolve()
        cls.port = int(os.environ.get("SHUFFLE_TEST_PORT", "25000"))
        cls.cache = {}

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_case(self, protocol, n, logsz=6, batch=4, repeat=1, veclen=1):
        key = (protocol, n, logsz, batch, repeat, veclen)
        if key in self.cache:
            return self.cache[key]
        logs = self.base / "-".join(map(str, key))
        port = type(self).port
        type(self).port += 100
        result = subprocess.run(
            [sys.executable, str(ROOT / "Scripts/run-shuffle.py"), protocol,
             str(n), str(logsz), str(veclen), str(batch), str(port), str(repeat),
             "--binary", str(self.binary), "--work-dir", str(self.base / "work"),
             "--log-dir", str(logs), "--timeout", "180"],
            capture_output=True, text=True, timeout=200,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        metrics = [list(map(float, (logs / f"party{i}.out").read_text().split()))
                   for i in range(n)]
        for party in metrics:
            self.assertEqual(len(party), 6)
            self.assertEqual(party[1], metrics[0][1])
            self.assertEqual(party[4], metrics[0][4])
        rounds = (metrics[0][1], metrics[0][4])
        self.cache[key] = rounds
        print(f"{key}: offline={rounds[0]}, online={rounds[1]}", flush=True)
        return rounds

    def test_party_scaling_and_parallel_offline_depth(self):
        # The chain spans all parties, including links a party does not see.
        # Chase/Song preparation is independent of the party count.
        for n, song_online, ours_offline in ((2, 190, 122), (3, 276, 119),
                                            (6, 534, 116)):
            with self.subTest(n=n):
                self.assertEqual(self.run_case("my_shuffle", n),
                                 (ours_offline, n + 14))
                self.assertEqual(self.run_case("Song_shuffle", n), (10, song_online))
                self.assertEqual(self.run_case("semi_my_shuffle", n), (11, n + 1))
                self.assertEqual(self.run_case("Chase_shuffle", n), (9, n))

    def test_decomposition_and_mpc_exchange_depth(self):
        # For this size, offline depth is parallel Song permutation depth
        # plus preparation, log(m) power computation, openings, and MAC checks.
        for batch, song_online in ((1, 1005), (6, 195)):
            with self.subTest(batch=batch):
                self.assertEqual(self.run_case("Song_shuffle", 3, batch=batch),
                                 (10, song_online))
                self.assertEqual(self.run_case("my_shuffle", 3, batch=batch),
                                 (song_online / 3 + 27, 17))
                self.assertEqual(self.run_case("semi_my_shuffle", 3, batch=batch),
                                 (11, 4))
                self.assertEqual(self.run_case("Chase_shuffle", 3, batch=batch),
                                 (9, 3))

    def test_repetitions_keep_fractional_batch_depth(self):
        for repeat in (2, 3):
            with self.subTest(repeat=repeat):
                off, on = self.run_case("Song_shuffle", 3, repeat=repeat)
                self.assertAlmostEqual(off, 10 / repeat)
                ours_off, ours_on = self.run_case("my_shuffle", 3, repeat=repeat)
                self.assertAlmostEqual(ours_off, (on / 3 + 27) / repeat)
                self.assertEqual(ours_on, 17)
                for protocol, depth, online in (("semi_my_shuffle", 11, 4),
                                                ("Chase_shuffle", 9, 3)):
                    off, on = self.run_case(protocol, 3, repeat=repeat)
                    self.assertAlmostEqual(off, depth / repeat)
                    self.assertEqual(on, online)

    def test_small_mac_batches_and_vector_length(self):
        # Exercises a small final MAC check and pending large pre-check,
        # Packing fields leaves the online depth unchanged; the malicious
        # offline powers span m * veclen and add one round when that doubles.
        for protocol, expected in (("my_shuffle", (56, 17)),
                                   ("Song_shuffle", (10, 105)),
                                   ("semi_my_shuffle", (11, 4)),
                                   ("Chase_shuffle", (9, 3))):
            with self.subTest(protocol=protocol):
                self.assertEqual(self.run_case(protocol, 3, logsz=1, batch=1), expected)
        for protocol, expected in (("my_shuffle", (120, 17)),
                                   ("Song_shuffle", (10, 276)),
                                   ("semi_my_shuffle", (11, 4)),
                                   ("Chase_shuffle", (9, 3))):
            with self.subTest(protocol=protocol, veclen=2):
                self.assertEqual(self.run_case(protocol, 3, veclen=2), expected)


if __name__ == "__main__":
    unittest.main()
