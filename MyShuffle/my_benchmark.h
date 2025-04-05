#pragma once

#include <iostream>

#include "mpc_gadget.h"

#include "Song_shuffle.h"
#include "my_shuffle.h"


static const int veclen = 1;
static const std::vector<int> default_all_logsz = {12}, all_log_batch = {4, 5, 6, 7};

static const int default_rep = 1;

class record {
public:
    int logsz;
    double off_time, on_time;
    size_t off_comm, on_comm;
};

class all_record {
public:
    all_record(std::string filename);

    std::string filename;
    std::vector<record> records;

    void save() const;
};


void benchmark_Song_shuffle(myShuffle::mpc_comm& com);
void benchmark_my_shuffle(myShuffle::mpc_comm& com);

void execute_my_shuffle(myShuffle::mpc_comm &com,
                            int logsz, int veclen, int logbatch, int rep,
                            size_t& off_comm, double& off_time,
                            size_t& on_comm, double& on_time);
void execute_Song_shuffle(myShuffle::mpc_comm &com,
                            int logsz, int veclen, int logbatch, int rep,
                            size_t& off_comm, double& off_time,
                            size_t& on_comm, double& on_time);

