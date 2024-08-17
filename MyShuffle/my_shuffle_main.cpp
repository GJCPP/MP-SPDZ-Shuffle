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
    std::cout << "Main Exit." << std::endl;
    return 0;
}

void run(char** argv, int prime_length)
{
    // set up networking on localhost
    int me = atoi(argv[1]);
    int n = atoi(argv[2]);
    gjcShuffle::mpc_comm com(n, me);


    // set of protocols (input, multiplication, output)
    ProtocolSet<ShareType> set(com.get_P(), com.get_setup());
    com.init(&set.input, &set.protocol, &set.output);
    std::cout << "Player " << me << " of " << n << " started." << std::endl;
    
    CryptoPlayer& P = com.get_P();
    auto& input = set.input;
    auto& protocol = set.protocol;
    auto& output = set.output;

    // set up the protocol
    test_Song_shuffle(com);
}
