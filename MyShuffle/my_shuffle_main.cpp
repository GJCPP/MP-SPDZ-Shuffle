#define NO_MIXED_CIRCUITS

#include <chrono>
#include <cstdlib>
#include <exception>

#include "Math/gfp.hpp"
#include "Machines/SPDZ.hpp"
#include "Machines/SPDZ2k.hpp"
#include "Machines/MalRep.hpp"
#include "Machines/ShamirMachine.hpp"
#include "Machines/Semi2k.hpp"
#include "Protocols/CowGearShare.h"
#include "Protocols/CowGearPrep.hpp"
#include "Protocols/ProtocolSet.h"
#include "Tools/random.h"
#include <libOTe/libOTe/TwoChooseOne/IknpOtExtSender.h>
#include <libOTe/libOTe/TwoChooseOne/IknpOtExtReceiver.h>
#include <libOTe/libOTe/Base/SimplestOT.h>

#include "double_length_prg.h"
#include "mpc_communicator.h"
#include "math_gadget.h"
#include "Benes_network.h"
#include "OPV.h"
#include "Chase_shuffle.h"
#include "Song_shuffle.h"
#include "test_shuffle.h"
#include "unit_test.h"
#include "my_benchmark.h"
#include "mpc_gadget.h"

#include <cryptoTools/Network/IOService.h>

int python_interface(int argc, char **argv);

int main(int argc, char** argv)
{
    try {
        return python_interface(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception" << std::endl;
        return 1;
    }
}


int python_interface(int argc, char **argv) {
    if (argc != 9) {
        std::cerr << "argc = " << argc << ", expect 9." << std::endl;
        std::cerr << "Usage: " << argv[0] << " <protocol> <me> <n_party> <logsz> <veclen> <logbatch> <port base> <repeat>" << std::endl;
        std::cerr << "\tprotocol: my_shuffle, my_shuffle_strong, Song_shuffle, Chase_shuffle, semi_my_shuffle, test_my_shuffle, test_my_shuffle_strong, test_semi_my_shuffle" << std::endl;
        return 2;
    }
    // set up networking on localhost
    int protocol;
    if (strcmp(argv[1], "my_shuffle") == 0) {
        protocol = 0;
    } else if (strcmp(argv[1], "my_shuffle_strong") == 0) {
        protocol = 8;
    } else if (strcmp(argv[1], "Song_shuffle") == 0) {
        protocol = 1;
    } else if (strcmp(argv[1], "semi_my_shuffle") == 0) {
        protocol = 2;
    } else if (strcmp(argv[1], "test_semi_my_shuffle") == 0) {
        protocol = 3;
    } else if (strcmp(argv[1], "Chase_shuffle") == 0) {
        protocol = 4;
    } else if (strcmp(argv[1], "test_my_shuffle") == 0) {
        protocol = 6;
    } else if (strcmp(argv[1], "test_my_shuffle_strong") == 0) {
        protocol = 7;
    } else {
        std::cerr << "Unknown protocol: " << argv[1] << std::endl;
        return 2;
    }
    int me = atoi(argv[2]);
    int n = atoi(argv[3]);
    int logsz = atoi(argv[4]);
    int veclen = atoi(argv[5]);
    int logbatch = atoi(argv[6]);
    int port_base = atoi(argv[7]);
    int rep = atoi(argv[8]);
    
    myShuffle::mpc_comm com(n, me, port_base);

    // set of protocols
    ProtocolSet<ShareType> set(com.get_P(), com.get_setup());
    
    com.init(&set.preprocessing, &set.input, &set.protocol, &set.output);

    if (protocol == 3) {
        return test_semi_my_shuffle(com, logsz, veclen, logbatch) ? 0 : 1;
    } else if (protocol == 6 || protocol == 7) {
        return test_my_shuffle(com, protocol == 7) ? 0 : 1;
    }

    size_t off_comm, off_round, on_comm, on_round;
    double off_time, on_time;
    if (protocol == 0) {
        execute_my_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_round, off_time, on_comm, on_round, on_time);
    } else if (protocol == 8) {
        execute_my_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_round, off_time, on_comm, on_round, on_time, true);
    } else if (protocol == 1) {
        execute_Song_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_round, off_time, on_comm, on_round, on_time);
    } else if (protocol == 2) {
        execute_semi_my_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_round, off_time, on_comm, on_round, on_time);
    } else {
        execute_Chase_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_round, off_time, on_comm, on_round, on_time);
    }
    std::cout << off_comm / rep << " " << off_round / rep << " " << off_time / rep << " "
            << on_comm / rep << " " << on_round / rep << " " << on_time / rep << std::endl;
    // std::cerr << off_comm / rep << " " << off_time / rep << " " << on_comm / rep << " " << on_time / rep << std::endl;
    return 0;
}
