#pragma once

#include <iostream>

#include "mpc_communicator.h"
#include "Song_shuffle.h"
#include "my_shuffle.h"

static const int veclen = 1;
static const std::vector<int> all_logsz = {2, 3, 4, 5, 6, 7}, all_log_batch = {3, 4, 5};

static const int rep = 5;

class record {
public:
    double off_time, on_time;
    size_t off_comm, on_comm;
};

class all_record {
public:
    all_record(std::string name);

    std::string name;
    std::vector<record> records;

    void save() const;
};


void benchmark_Song_shuffle(gjcShuffle::mpc_comm& com);
void benchmark_my_shuffle(gjcShuffle::mpc_comm& com);
