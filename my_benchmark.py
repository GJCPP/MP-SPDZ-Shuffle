import signal
import subprocess
from tqdm import tqdm
import os
import time
import sys
from contextlib import contextmanager
import threading

class TimeoutException(Exception): pass

@contextmanager
def time_limit(seconds):
    def signal_handler(signum, frame):
        raise TimeoutException("Timed out!")
    signal.signal(signal.SIGALRM, signal_handler)
    signal.alarm(seconds)
    try:
        yield
    finally:
        signal.alarm(0)

def run_command(cmd):
    os.system(cmd)

def run_protocol(protocol : str, n_party: int, logsz: int, veclen: int, logbatch: int, port_base: int, rep: int):
    proc = []
    with open('stdout', 'w+') as f:
        f.write('-1 -1 -1 -1')
    for party in range(n_party):
        redir = ' > stdout ' if party == 0 else ''
        command = "./my_shuffle_main.x " + redir + protocol + ' ' + str(party) + ' ' + str(n_party) + ' ' + str(logsz) + ' ' + str(veclen) + ' ' + str(logbatch) + ' ' + str(port_base) + ' ' + str(rep)
        proc.append(threading.Thread(target=run_command, args=[command]))
        proc[-1].start()
    return proc

def sparse_output(proc: list):
    with open('stdout', 'r') as f:
        line = f.readline().split()
        print(line)
        off_comm = int(line[0])
        off_time = float(line[1])
        on_comm = int(line[2])
        on_time = float(line[3])
        return off_comm, off_time, on_comm, on_time

def save_result(filename : str,
                res_off_comm : list,
                res_off_time : list,
                res_on_comm : list,
                res_on_time : list):
    with open(filename, "w+") as f:
        f.write('logsz, ')
        for i in all_logsz:
            f.write(str(i) + ", ")
        f.write("\n")
        f.write("off_comm, ")
        for i in res_off_comm:
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
        f.write("on_time, ")
        for i in res_on_time:
            f.write(str(i) + ", ")
        f.write("\n")

def load_result(filename : str):
    global all_logsz
    res_off_comm = [-1 for i in all_logsz]
    res_off_time = [-1 for i in all_logsz]
    res_on_comm = [-1 for i in all_logsz]
    res_on_time = [-1 for i in all_logsz]
    if not os.path.isfile(filename):
        save_result(filename, res_off_comm, res_off_time, res_on_comm, res_on_time)
    with open(filename, "r") as f:
        for ind, line in enumerate(f.readlines()):
            if ind == 0:
                saved_all_logsz = [int(i) for i in line.strip(',\n ').split(',')[1:]]
                all_logsz = saved_all_logsz
            line = line.strip(',\n ').split(',')
            if ind == 1:
                res_off_comm = [int(i) for i in line[1:]]
                while len(res_off_comm) < len(all_logsz):
                    res_off_comm.append(-1)
            elif ind == 2:
                res_off_time = [float(i) for i in line[1:]]
                while len(res_off_time) < len(all_logsz):
                    res_off_time.append(-1)
            elif ind == 3:
                res_on_comm = [int(i) for i in line[1:]]
                while len(res_on_comm) < len(all_logsz):
                    res_on_comm.append(-1)
            elif ind == 4:
                res_on_time = [float(i) for i in line[1:]]
                while len(res_on_time) < len(all_logsz):
                    res_on_time.append(-1)
    return res_off_comm, res_off_time, res_on_comm, res_on_time

def get_filename(protocol : str, target: str, n_party: int):
    return protocol + "_" + target + "_n_" + str(n_party) + ".csv"


all_n_party = [i for i in range(3, 24, 3)]
all_logsz = [i for i in range(6, 14, 2)]
__all_logbatch = [i for i in range(4, 8, 1)]
all_targets = ['on_time', 'total_time' ]
max_time = 9999999
all_protocol = ['my_shuffle', 'Song_shuffle']
port_base = 10000
if len(sys.argv) > 1:
    all_protocol = [sys.argv[1]]
    port_base = int(sys.argv[2])
    if len(sys.argv) > 3:
        all_n_party = [int(sys.argv[3])]
veclen = 1
rep = 1

for protocol in all_protocol:
    for target in all_targets:
        if target == 'on_time' and protocol == 'my_shuffle':
            continue
        for n_party in all_n_party:
            res_off_comm, res_off_time, res_on_comm, res_on_time = load_result(get_filename(protocol, target, n_party))
            for ind, logsz in enumerate(all_logsz):
                print('Testing ' + protocol + ' with ' + str(n_party) + ' parties and logsz = ' + str(logsz) + ' of ' + str(all_logsz))
                if res_off_comm[ind] != -1:
                    print('Already tested. Skip.')
                    continue
                all_time_out  = True
                best_off_comm = 10000000000000
                best_off_time = 1e9
                best_on_comm  = 10000000000000
                best_on_time  = 1e9
                best_total_time = 1e9
                wait_time = max_time
                all_logbatch = __all_logbatch
                if target == 'on_time':
                    if logsz == 6:
                        all_logbatch = [4, 6]
                    elif logsz == 8:
                        all_logbatch = [4, 8]
                    elif logsz == 10:
                        all_logbatch = [4, 5]
                    elif logsz == 12:
                        all_logbatch = [4, 6]
                    elif logsz == 14:
                        all_logbatch = [4, 7]
                for logbatch in tqdm(all_logbatch):
                    off_time, off_comm = 0.0, 0
                    on_time, on_comm = 0.0, 0
                    time_out = False
                    proc = []
                    elapse = max_time
                    try:
                        current_time = time.time()
                        with time_limit(wait_time):
                            proc = run_protocol(protocol, n_party, logsz, veclen, logbatch, port_base, rep)
                            for p in proc:
                                p.join()
                        elapse = time.time() - current_time
                    except TimeoutException as e:
                        time_out = True
                    if time_out:
                        print("Time out")
                        os.system("pkill my_shuffle_main")
                        time.sleep(5) # Wait for port release
                        continue
                    all_time_out = False

                    off_comm, off_time, on_comm, on_time = sparse_output(proc)

                    if off_comm == -1:
                        print("WARNING: No valid output, logsz = ", logsz, ", logbatch = ", logbatch)
                        continue
                    if off_comm != -1 and (target == 'total_time' and off_time + on_time < best_total_time) or (target == 'on_time' and on_time < best_on_time):
                        best_total_time = off_time + on_time
                        best_off_comm = off_comm
                        best_off_time = off_time
                        best_on_comm = on_comm
                        best_on_time = on_time
                        res_off_comm[ind] = best_off_comm
                        res_off_time[ind] = best_off_time
                        res_on_comm[ind] = best_on_comm
                        res_on_time[ind] = best_on_time
                    if target == 'total_time' and elapse + 60 < wait_time:
                        wait_time = int(elapse + 60)
                save_result(get_filename(protocol, target, n_party), res_off_comm, res_off_time, res_on_comm, res_on_time)

