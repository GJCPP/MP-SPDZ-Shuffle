import csv
import subprocess
from tqdm import tqdm
import os
import time
import sys
import threading

OUTPUT_DIR = os.environ.get("SHUFFLE_BENCHMARK_DIR", "benchmark_results")
STDOUT_FILE = os.path.join(OUTPUT_DIR, "stdout")
STDERR_FILE = os.path.join(OUTPUT_DIR, "stderr_party0")
RAW_LOG_FILE = os.path.join(OUTPUT_DIR, "raw_batches.csv")

# Run a party
def run_command(command: list, redir: bool, return_codes: list, party: int):
    if redir:
        with open(STDOUT_FILE, 'w+') as f, open(STDERR_FILE, "w+") as err:
            result = subprocess.run(command, stdout=f, stderr=err)
    else:
        result = subprocess.run(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    return_codes[party] = result.returncode

# Run n_party parties
def run_protocol(protocol: str, n_party: int, logsz: int, veclen: int, logbatch: int, port_base: int, rep: int):
    threads = []
    return_codes = [None] * n_party
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    with open(STDOUT_FILE, 'w+') as f:
        f.write('-1 -1 -1 -1 -1 -1\n')

    for party in range(n_party):
        redir = (party == 0)
        command = [
            "./build/my_shuffle_main.x",
            protocol,
            str(party),
            str(n_party),
            str(logsz),
            str(veclen),
            str(logbatch),
            str(port_base),
            str(rep),
        ]
        thread = threading.Thread(
            target=run_command,
            args=(command, redir, return_codes, party),
        )
        thread.start()
        threads.append(thread)

    for thread in threads:
        thread.join()

    failures = [
        f"party {party}: exit {code}"
        for party, code in enumerate(return_codes)
        if code != 0
    ]
    if failures:
        raise RuntimeError("; ".join(failures))
    return return_codes

# Spare the output of protocol into (offline comm, rounds, time, online comm, rounds, time)
def sparse_output(proc: list):
    with open(STDOUT_FILE, 'r') as f:
        line = f.readline().split()
        print(line)
        off_comm = int(line[0])
        if len(line) == 4:
            off_round = -1
            off_time = float(line[1])
            on_comm = int(line[2])
            on_round = -1
            on_time = float(line[3])
        else:
            off_round = int(line[1])
            off_time = float(line[2])
            on_comm = int(line[3])
            on_round = int(line[4])
            on_time = float(line[5])
        return off_comm, off_round, off_time, on_comm, on_round, on_time

def append_party0_logs(protocol: str,
                       target: str,
                       n_party: int,
                       logsz: int,
                       logbatch: int,
                       attempt: int):
    run_id = f"{protocol},{target},n={n_party},logsz={logsz},logbatch={logbatch},attempt={attempt}"
    for source, destination in (
        (STDOUT_FILE, os.path.join(OUTPUT_DIR, "party0_stdout.log")),
        (STDERR_FILE, os.path.join(OUTPUT_DIR, "party0_stderr.log")),
    ):
        try:
            with open(source, "r") as f:
                content = f.read()
        except FileNotFoundError:
            content = ""
        with open(destination, "a") as out:
            out.write(f"\n===== {run_id} =====\n")
            out.write(content)
            if content and not content.endswith("\n"):
                out.write("\n")

def append_raw_result(protocol: str,
                      target: str,
                      n_party: int,
                      logsz: int,
                      logbatch: int,
                      attempt: int,
                      status: str,
                      elapsed: float,
                      off_comm: int = -1,
                      off_round: int = -1,
                      off_time: float = -1,
                      on_comm: int = -1,
                      on_round: int = -1,
                      on_time: float = -1,
                      reason: str = ""):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    write_header = not os.path.isfile(RAW_LOG_FILE) or os.path.getsize(RAW_LOG_FILE) == 0
    with open(RAW_LOG_FILE, "a", newline="") as f:
        writer = csv.writer(f)
        if write_header:
            writer.writerow([
                "protocol",
                "target",
                "n_party",
                "logsz",
                "logbatch",
                "attempt",
                "status",
                "elapsed_wall_time",
                "off_comm",
                "off_round",
                "off_time",
                "on_comm",
                "on_round",
                "on_time",
                "reason",
            ])
        writer.writerow([
            protocol,
            target,
            n_party,
            logsz,
            logbatch,
            attempt,
            status,
            elapsed,
            off_comm,
            off_round,
            off_time,
            on_comm,
            on_round,
            on_time,
            reason,
        ])

def save_result(filename : str,
                res_off_comm : list,
                res_off_round : list,
                res_off_time : list,
                res_on_comm : list,
                res_on_round : list,
                res_on_time : list,
                cur_logsz : int = -1,
                cur_logbatch : int = -1,
                cur_off_comm : int = -1,
                cur_off_round : int = -1,
                cur_off_time : float = -1,
                cur_on_comm : int = -1,
                cur_on_round : int = -1,
                cur_on_time : float = -1):
    with open(filename, "w+") as f:
        f.write('logsz, ')
        for i in all_logsz:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("off_comm, ")
        for i in res_off_comm:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("off_round, ")
        for i in res_off_round:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("off_time, ")
        for i in res_off_time:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("on_comm, ")
        for i in res_on_comm:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("on_round, ")
        for i in res_on_round:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("on_time, ")
        for i in res_on_time:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("current, " + str(cur_logsz) + ", " + str(cur_logbatch) + ", " + str(cur_off_comm) + ", " + str(cur_off_round) + ", " + str(cur_off_time) + ", " + str(cur_on_comm) + ", " + str(cur_on_round) + ", " + str(cur_on_time) + ", \n")

# Load existing result
def load_result(filename : str):
    global all_logsz
    res_off_comm = [-1 for i in all_logsz]
    res_off_round = [-1 for i in all_logsz]
    res_off_time = [-1 for i in all_logsz]
    res_on_comm = [-1 for i in all_logsz]
    res_on_round = [-1 for i in all_logsz]
    res_on_time = [-1 for i in all_logsz]
    cur_logsz = -1
    cur_logbatch = -1
    cur_off_comm = -1
    cur_off_round = -1
    cur_off_time = -1
    cur_on_comm = -1
    cur_on_round = -1
    cur_on_time = -1
    if not os.path.isfile(filename):
        save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time)
    with open(filename, "r") as f:
        for ind, line in enumerate(f.readlines()):
            if ind == 0:
                saved_all_logsz = [int(i) for i in line.strip(',\n ').split(',')[1:]]
                all_logsz = saved_all_logsz
            line = line.strip(',\n ').split(',')
            if ind == 1: # Offline comm
                print(line)
                res_off_comm = [int(i) for i in line[1:]]
                while len(res_off_comm) < len(all_logsz):
                    res_off_comm.append(-1)
            elif ind == 2: # Offline round
                res_off_round = [int(i) for i in line[1:]]
                while len(res_off_round) < len(all_logsz):
                    res_off_round.append(-1)
            elif ind == 3: # Offline time
                res_off_time = [float(i) for i in line[1:]]
                while len(res_off_time) < len(all_logsz):
                    res_off_time.append(-1)
            elif ind == 4: # Online comm
                res_on_comm = [int(i) for i in line[1:]]
                while len(res_on_comm) < len(all_logsz):
                    res_on_comm.append(-1)
            elif ind == 5: # Online round
                res_on_round = [int(i) for i in line[1:]]
                while len(res_on_round) < len(all_logsz):
                    res_on_round.append(-1)
            elif ind == 6: # Online time
                res_on_time = [float(i) for i in line[1:]]
                while len(res_on_time) < len(all_logsz):
                    res_on_time.append(-1)
            elif ind == 7:
                cur_logsz = int(line[1])
                cur_logbatch = int(line[2])
                cur_off_comm = int(line[3])
                cur_off_round = int(line[4])
                cur_off_time = float(line[5])
                cur_on_comm = int(line[6])
                cur_on_round = int(line[7])
                cur_on_time = float(line[8])
    return res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, cur_logsz, cur_logbatch, cur_off_comm, cur_off_round, cur_off_time, cur_on_comm, cur_on_round, cur_on_time

def get_filename(protocol : str, target: str, n_party: int):
    return os.path.join(OUTPUT_DIR, protocol + "_" + target + "_n_" + str(n_party) + ".csv")

def configure_suite(argv):
    all_n_party = [i for i in range(3, 18, 3)]
    all_protocol = ['Song_shuffle', 'my_shuffle']
    port_base = 10000
    suite = 'malicious'

    if len(argv) <= 1:
        return suite, all_protocol, all_n_party, port_base

    suite_or_protocol = argv[1]
    if len(argv) > 2:
        port_base = int(argv[2])

    if suite_or_protocol == 'malicious':
        suite = 'malicious'
        all_protocol = ['Song_shuffle', 'my_shuffle']
        all_n_party = [i for i in range(3, 18, 3)]
    elif suite_or_protocol == 'semi':
        suite = 'semi'
        all_protocol = ['Chase_shuffle', 'semi_my_shuffle']
        all_n_party = [2]
    elif suite_or_protocol in ('semi-parties', 'semi_party'):
        suite = 'semi-parties'
        all_protocol = ['Chase_shuffle', 'semi_my_shuffle']
        all_n_party = [i for i in range(3, 18, 3)]
    else:
        suite = suite_or_protocol
        all_protocol = [suite_or_protocol]
        if len(argv) > 3:
            all_n_party = [int(argv[3])]

    return suite, all_protocol, all_n_party, port_base

def parse_int_list(env_name, default):
    raw = os.environ.get(env_name)
    if not raw:
        return default
    return [int(item) for item in raw.split(',') if item.strip()]


os.makedirs(OUTPUT_DIR, exist_ok=True)
suite, all_protocol, all_n_party, port_base = configure_suite(sys.argv)
all_n_party = parse_int_list("SHUFFLE_BENCHMARK_PARTIES", all_n_party)
semi_protocols = {'Chase_shuffle', 'semi_my_shuffle'}
is_semi_benchmark = suite in ('semi', 'semi-parties') or all(protocol in semi_protocols for protocol in all_protocol)
default_logsz = [i for i in range(6, 22, 2)] if is_semi_benchmark else [i for i in range(6, 14, 2)]
all_logsz = parse_int_list("SHUFFLE_BENCHMARK_LOGSZ", default_logsz)
# all_logsz = [i for i in range(6, 8, 1)]
# print("WARNING: Using reduced logsz for testing.")
__all_logbatch = parse_int_list("SHUFFLE_BENCHMARK_LOGBATCH", [i for i in range(4, 11, 1)])
all_targets = ['total_time', 'on_time']
max_retries = int(os.environ.get("SHUFFLE_BENCHMARK_RETRIES", "0"))
veclen = 1
rep = 1
had_failure = False


def logbatches_for_point(logsz):
    if is_semi_benchmark and logsz == 22:
        return [logbatch for logbatch in __all_logbatch if logbatch != 10]
    return __all_logbatch

for protocol in all_protocol:
    for target in all_targets:
        if target == 'on_time' and protocol in (
                'my_shuffle', 'my_shuffle_strong', 'semi_my_shuffle'):
            continue
        for n_party in all_n_party:
            filename = get_filename(protocol, target, n_party)
            res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, cur_logsz, cur_logbatch, cur_off_comm, cur_off_round, cur_off_time, cur_on_comm, cur_on_round, cur_on_time = load_result(filename)
            for ind, logsz in enumerate(all_logsz):
                print('Testing ' + protocol + ' with ' + str(n_party) + ' parties and logsz = ' + str(logsz) + ' of ' + str(all_logsz))


                if res_off_comm[ind] != -1 and logsz != cur_logsz:
                    print('Already tested. Skip.')
                    continue

                # Initial record
                current_logbatch = cur_logbatch if cur_logsz == logsz else -1
                has_valid_result = res_off_comm[ind] != -1
                if has_valid_result:
                    best_off_comm = res_off_comm[ind]
                    best_off_round = res_off_round[ind]
                    best_off_time = res_off_time[ind]
                    best_on_comm = res_on_comm[ind]
                    best_on_round = res_on_round[ind]
                    best_on_time = res_on_time[ind]
                    best_total_time = best_on_time + best_off_time
                else:
                    best_off_comm = -1
                    best_off_round = -1
                    best_off_time = 1e9
                    best_on_comm  = -1
                    best_on_round = -1
                    best_on_time  = 1e9
                    best_total_time = 1e9
                all_logbatch = logbatches_for_point(logsz)

                for logbatch in all_logbatch:
                    print('Testing logbatch = ' + str(logbatch))
                    if logbatch <= current_logbatch:
                        print('Already tested. Skipped.')
                        continue

                    continue_test = True
                    proc = []
                    attempts = 0

                    while continue_test:
                        continue_test = False
                        attempts += 1
                        time.sleep(5) # Wait for port release
                        
                        # Run the protocol
                        current_time = time.time()
                        try:
                            proc = run_protocol(protocol, n_party, logsz, veclen, logbatch, port_base, rep)
                            elapse = time.time() - current_time
                            append_party0_logs(protocol, target, n_party, logsz, logbatch, attempts)
                            # print(proc)
                            off_comm, off_round, off_time, on_comm, on_round, on_time = sparse_output(proc)

                            if off_comm == -1:
                                print("WARNING: No valid output, logsz = ", logsz, ", logbatch = ", logbatch)
                                append_raw_result(
                                    protocol,
                                    target,
                                    n_party,
                                    logsz,
                                    logbatch,
                                    attempts,
                                    "invalid_output",
                                    elapse,
                                    reason="party 0 output did not contain valid benchmark metrics",
                                )
                                if attempts <= max_retries:
                                    continue_test = True
                                else:
                                    had_failure = True
                                if has_valid_result:
                                    save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, logsz, logbatch, best_off_comm, best_off_round, best_off_time, best_on_comm, best_on_round, best_on_time)
                                else:
                                    save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, logsz, logbatch)
                                continue
                            improved = ((target == 'total_time' and off_time + on_time < best_total_time)
                                        or (target == 'on_time' and on_time < best_on_time))
                            append_raw_result(
                                protocol,
                                target,
                                n_party,
                                logsz,
                                logbatch,
                                attempts,
                                "ok",
                                elapse,
                                off_comm,
                                off_round,
                                off_time,
                                on_comm,
                                on_round,
                                on_time,
                            )
                            if off_comm != -1 and improved:
                                has_valid_result = True
                                best_total_time = off_time + on_time
                                best_off_comm = off_comm
                                best_off_round = off_round
                                best_off_time = off_time
                                best_on_comm = on_comm
                                best_on_round = on_round
                                best_on_time = on_time
                                res_off_comm[ind] = best_off_comm
                                res_off_round[ind] = best_off_round
                                res_off_time[ind] = best_off_time
                                res_on_comm[ind] = best_on_comm
                                res_on_round[ind] = best_on_round
                                res_on_time[ind] = best_on_time
                            save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, logsz, logbatch, best_off_comm, best_off_round, best_off_time, best_on_comm, best_on_round, best_on_time)
                        except Exception as e:
                            elapse = time.time() - current_time
                            print(e)
                            print("Error. Restarting.")
                            os.system("pkill my_shuffle_main")
                            append_raw_result(
                                protocol,
                                target,
                                n_party,
                                logsz,
                                logbatch,
                                attempts,
                                "error",
                                elapse,
                                reason=str(e),
                            )
                            if attempts <= max_retries:
                                continue_test = True # Recover from exception
                            else:
                                had_failure = True
                                if has_valid_result:
                                    save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, logsz, logbatch, best_off_comm, best_off_round, best_off_time, best_on_comm, best_on_round, best_on_time)
                                else:
                                    save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time, logsz, logbatch)

                if not has_valid_result:
                    res_off_comm[ind] = -1
                    res_off_round[ind] = -1
                    res_off_time[ind] = -1
                    res_on_comm[ind] = -1
                    res_on_round[ind] = -1
                    res_on_time[ind] = -1
                save_result(filename, res_off_comm, res_off_round, res_off_time, res_on_comm, res_on_round, res_on_time)

if had_failure:
    sys.exit(1)
