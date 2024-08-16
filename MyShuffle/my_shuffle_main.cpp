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
#include "deps/libOTe/libOTe/Base/SimplestOT.h"

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
    std::cout << "Main Exit." << std::endl;
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
    const int numOTs = 10;
    if (my_number == 0) {
        for (int cnt(0); cnt != 10; ++cnt) {
            std::vector<std::array<block_wrapper, 2>> send_msg(numOTs);
            for (int i = 0; i < numOTs; i++) {
                send_msg[i][0] = makeBlockWrapper(0, 0);
                send_msg[i][1] = makeBlockWrapper(0xffffffffffffffff, 0xffffffffffffffff);
            }
            com.send_ext_ot(1, send_msg);
            com.send(1, send_msg.data(), numOTs * 2 * sizeof(block_wrapper));
        }
    }
    if (my_number == 1) {
        bool failed = false;
        osuCrypto::PRNG prg(osuCrypto::block(1235123,3456123));
        for (int cnt(0); cnt != 10 && !failed; ++cnt) {
            std::vector<std::array<block_wrapper, 2>> send_msg(numOTs);
            std::vector<block_wrapper> recv_msg(numOTs);
            osuCrypto::BitVector choices(numOTs);
            choices.randomize(prg);
            com.recv_ext_ot(0, choices, recv_msg);
            com.recv(0, send_msg.data(), numOTs * 2 * sizeof(block_wrapper));
            for (int i = 0; i < numOTs; i++) {
                    std::cout << "Send " << i << ": " << send_msg[i][0] << " " << send_msg[i][1] << std::endl;
                std::cout << "Recv " << i << ": " << choices[i] << " " << recv_msg[i];
                if (recv_msg[i] != send_msg[i][choices[i]]) {
                    std::cout << ", Error!";
                    failed = true;
                    break;
                }
                std::cout << std::endl;
            }
        }
        if (failed) {
            std::cout << "OT failed." << std::endl;
        } else {
            std::cout << "OT success." << std::endl;
        }
    }
}
