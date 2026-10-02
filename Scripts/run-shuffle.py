#!/usr/bin/env python3
"""Run all local parties, check their exits, and retain each party's logs."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
PROTOCOLS = (
    "my_shuffle", "Song_shuffle", "Chase_shuffle", "semi_my_shuffle",
    "test_my_shuffle", "test_my_shuffle_tamper", "test_semi_my_shuffle",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("protocol", choices=PROTOCOLS)
    parser.add_argument("parties", type=int)
    parser.add_argument("logsz", type=int)
    parser.add_argument("veclen", type=int)
    parser.add_argument("logbatch", type=int)
    parser.add_argument("port", type=int)
    parser.add_argument("repeat", type=int)
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--expect-mac-failure", action="store_true")
    parser.add_argument("--binary", type=Path, default=ROOT / "build/my_shuffle_main.x")
    parser.add_argument("--work-dir", type=Path, default=ROOT)
    parser.add_argument("--log-dir", type=Path)
    args = parser.parse_args()
    if (args.parties < 2 or not 1 <= args.logsz <= 30
            or min(args.veclen, args.logbatch, args.repeat) < 1
            or not 1 <= args.port < 65536 - 5 * args.parties
            or args.timeout <= 0):
        parser.error("invalid party count, dimensions, port, repeat, or timeout")
    if args.expect_mac_failure and args.protocol != "test_my_shuffle_tamper":
        parser.error("--expect-mac-failure requires test_my_shuffle_tamper")

    binary = args.binary.resolve()
    work_dir = args.work_dir.resolve()
    logs = args.log_dir or Path(tempfile.mkdtemp(prefix="shuffle-run-"))
    logs = logs.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    work_dir.mkdir(parents=True, exist_ok=True)
    print(f"Party logs: {logs}", flush=True)
    with (logs / "setup-ssl.log").open("w") as output:
        subprocess.run(
            ["bash", str(ROOT / "Scripts/setup-ssl.sh"), "--ensure",
             str(args.parties), str(work_dir / "Player-Data")],
            stdout=output, stderr=subprocess.STDOUT, check=True,
        )

    processes = []
    streams = []
    timed_out = False
    try:
        for party in range(args.parties):
            out = (logs / f"party{party}.out").open("w")
            err = (logs / f"party{party}.err").open("w")
            streams.extend((out, err))
            command = [str(binary), args.protocol, str(party), str(args.parties),
                       str(args.logsz), str(args.veclen), str(args.logbatch),
                       str(args.port), str(args.repeat)]
            processes.append(subprocess.Popen(command, cwd=work_dir,
                                              stdout=out, stderr=err))
        deadline = time.monotonic() + args.timeout
        while any(process.poll() is None for process in processes):
            if time.monotonic() >= deadline:
                timed_out = True
                break
            if not args.expect_mac_failure and any(
                    process.poll() not in (None, 0) for process in processes):
                break
            time.sleep(0.05)
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
        for process in processes:
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for stream in streams:
            stream.close()

    codes = [process.returncode for process in processes]
    errors = [(logs / f"party{party}.err").read_text(errors="replace")
              for party in range(args.parties)]
    if args.expect_mac_failure:
        passed = not timed_out and all(
            code != 0 and "MacCheck Failure" in error
            for code, error in zip(codes, errors)
        )
    else:
        passed = not timed_out and all(code == 0 for code in codes)
    print((logs / "party0.out").read_text(errors="replace"), end="")
    if not passed:
        print(f"Shuffle failed: exits={codes}, timeout={timed_out}", file=sys.stderr)
        for party, error in enumerate(errors):
            if error:
                print(f"Party {party}:\n{error[-4000:]}", file=sys.stderr)
        return 1
    print("Expected MAC failure detected by all parties." if args.expect_mac_failure
          else "All parties completed successfully.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"Cannot run shuffle: {error}", file=sys.stderr)
        sys.exit(1)
