#define NO_MIXED_CIRCUITS

#include <chrono>

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
#include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtSender.h"
#include "deps/libOTe/libOTe/TwoChooseOne/IknpOtExtReceiver.h"
#include "deps/libOTe/libOTe/Base/SimplestOT.h"

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

#include "local/include/cryptoTools/Network/IOService.h"


void run_benchmark(int argc, char **argv);

void python_interface(int argc, char **argv);

int main(int argc, char** argv)
{
    //run_benchmark(argc, argv);
    python_interface(argc, argv);
    return 0;
}


void run_benchmark(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <me> <n_party>" << std::endl;
        return;
    }
    // set up networking on localhost
    int protocol;
    int me = atoi(argv[1]);
    int n = atoi(argv[2]);

    gjcShuffle::mpc_comm com(n, me);


    // set of protocols
    ProtocolSet<ShareType> set(com.get_P(), com.get_setup());
    
    com.init(&set.preprocessing, &set.input, &set.protocol, &set.output);




    benchmark_my_shuffle(com);
    benchmark_Song_shuffle(com);
    
    throw;
}


void python_interface(int argc, char **argv) {
    if (argc != 9) {
        std::cerr << "argc = " << argc << ", expect 9." << std::endl;
        std::cerr << "Usage: " << argv[0] << " <protocol> <me> <n_party> <logsz> <veclen> <logbatch> <port base> <repeat>" << std::endl;
        std::cerr << "\tprotocol: my_shuffle, Song_shuffle" << std::endl;
        return;
    }
    // set up networking on localhost
    int protocol;
    if (strcmp(argv[1], "my_shuffle") == 0) {
        protocol = 0;
    } else if (strcmp(argv[1], "Song_shuffle") == 0) {
        protocol = 1;
    } else {
        std::cerr << "Unknown protocol: " << argv[1] << std::endl;
        return;
    }
    int me = atoi(argv[2]);
    int n = atoi(argv[3]);
    int logsz = atoi(argv[4]);
    int veclen = atoi(argv[5]);
    int logbatch = atoi(argv[6]);
    int port_base = atoi(argv[7]);
    int rep = atoi(argv[8]);

    gjcShuffle::mpc_comm com(n, me, port_base);

    // set of protocols
    ProtocolSet<ShareType> set(com.get_P(), com.get_setup());
    
    com.init(&set.preprocessing, &set.input, &set.protocol, &set.output);

    size_t off_comm, on_comm;
    double off_time, on_time;
    if (protocol == 0) {
        execute_my_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_time, on_comm, on_time);
    } else {
        execute_Song_shuffle(com, logsz, veclen, logbatch, rep, off_comm, off_time, on_comm, on_time);
    }
    std::cout << off_comm / rep << " " << off_time / rep << " " << on_comm / rep << " " << on_time / rep << std::endl;
    throw;
    /*
     Force exit.
     I do not know why normal exit costs very very long time (to clean up / deconstruction?).
     To save test time, I force exit here. Nevertheless, the time record in execute_xxx_shuffle is accurate.
    */
}
