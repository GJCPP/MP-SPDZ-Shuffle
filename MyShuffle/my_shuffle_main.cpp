/*
 * paper-example.cpp
 *
 * Working example similar to Figure 2 in https://eprint.iacr.org/2020/521
 *
 */

#define NO_MIXED_CIRCUITS

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

#include "double_length_prg.h"
#include "mpc_communicator.h"
#include "math_gadget.h"
#include "Benes_network.h"
#include "OPV.h"

#include "local/include/cryptoTools/Network/IOService.h"

void run(char** argv, int prime_length);

int main(int argc, char** argv)
{
    // need player number and number of players
    if (argc < 3)
    {
        cerr << "Usage: " << argv[0]
                << " <my number: 0/1/...> <total number of players> [protocol [threshold]]"
                << endl;
        exit(1);
    }

    string protocol = "MASCOT";
    if (argc > 3)
        protocol = argv[3];

    if (protocol == "MASCOT")
        run(argv, prime_length);
    else
    {
        cerr << "Unknown protocol: " << protocol << endl;
        exit(1);
    }
    return 0;
}

void run(char** argv, int prime_length)
{
    // set up networking on localhost
    int my_number = atoi(argv[1]);
    int n_parties = atoi(argv[2]);
    mpc_comm com(n_parties, my_number);

    // set of protocols (input, multiplication, output)
    ProtocolSet<ShareType> set(com.get_P(), com.get_setup());
    com.init(&set.input, &set.protocol, &set.output);
    
    const int numOTs = 10000;
    using namespace osuCrypto;
    osuCrypto::IOService ios;
    if (my_number == 0) {
        BaseOT baseOT;
    }
    if (my_number == 1) {
        ;
    }
}
