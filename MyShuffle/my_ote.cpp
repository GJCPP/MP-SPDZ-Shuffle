#include "my_ote.h"

namespace my_ote {
    void recv_base_cor_ot(osuCrypto::BitVector choices,
                        osuCrypto::span<osuCrypto::block> recvKey,
                        osuCrypto::PRNG& osuPrg,
                        osuCrypto::Channel *channel)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        if (choices.size() != static_cast<size_t>(recvKey.size())) {
            std::cerr << "mpc_comm::base_ot_recv : choices.size() != recvKey.size(), " << choices.size() << " != " << recvKey.size() << endl;
            throw std::runtime_error("mpc_comm::base_ot_recv : choices.size() != recvKey.size()");
        }
        using namespace osuCrypto;
        AsmSimplestOT baseOT;

        baseOT.receive(choices, recvKey, osuPrg, *channel);
    }

    void send_base_cor_ot(osuCrypto::span<std::array<osuCrypto::block, 2>> sendKey,
                        osuCrypto::PRNG& osuPrg,
                        osuCrypto::Channel *channel)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        using namespace osuCrypto;
        AsmSimplestOT baseOT;
        baseOT.send(sendKey, osuPrg, *channel);
    }
}
