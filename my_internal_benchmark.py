import subprocess
from tqdm import tqdm
import time
import os
import threading

def run_command(cmd):
    os.system(cmd)

def run_protocol(n_party: int):
    proc = []
    FNULL = open(os.devnull, 'w')
    for party in range(n_party):
        proc.append(threading.Thread(target=run_command, args=(["./my_shuffle_main.x " + str(party) + ' ' + str(n_party)])))
        proc[-1].start()
    return proc

for num_party in [3, 6, 9, 12, 15, 18, 21]:
    proc = run_protocol(num_party)
    for p in proc:
        p.join()


