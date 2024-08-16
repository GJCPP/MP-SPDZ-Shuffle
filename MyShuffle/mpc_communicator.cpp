#include "mpc_communicator.h"

namespace gjcShuffle {

    mpc_comm::mpc_comm(int _n_party, int _my_number)
        : n_party(_n_party), my_number(_my_number), out_buff(n_party), in_buff(n_party), sessions(n_party), ios(),
            N(my_number, n_party, "localhost", 9999),
            P(N),
            setup(P, prime_length),
            prng(osuCrypto::sysRandomSeed())
    {
        int session_port_base = N.get_portnum_base() + 4 * n_party;

        for (int i(0); i != n_party; ++i) {
            if (i < my_number) {
                // This party acts as server
                sessions[i].start(ios, "localhost", session_port_base + my_number, osuCrypto::SessionMode::Server);
            }
            if (i > my_number) {
                sessions[i].start(ios, N.get_name(i), session_port_base + i, osuCrypto::SessionMode::Client);
            }
        }
        // setup = new ProtocolSetup<ShareType>(P, prime_length);
    }

    void mpc_comm::init(Input<ShareType> *_input, SPDZ<ShareType> *_protocol, MAC_Check_<ShareType> *_output)
    {
        input = _input;
        protocol = _protocol;
        output = _output;
    }

    CryptoPlayer &mpc_comm::get_P()
    {
    return P;
    }

    ProtocolSetup<ShareType> &mpc_comm::get_setup()
    {
        return setup;
    }

    int mpc_comm::get_port(int party) const
    {
        if (party == -1) party = my_number;
        if (party < 0 || party >= n_party) {
            cerr << "mpc_comm::get_port : Invalid party number: " << party << endl;
            throw std::runtime_error("mpc_comm::get_port : Invalid party number.");
        }
        return N.ports[party];
    }

    int mpc_comm::get_my_number() const
    {
        return my_number;
    }

    int mpc_comm::get_n_party() const
    {
        return n_party;
    }

    void mpc_comm::send(int recver, const void * data, size_t size)
    {
        octetStream o;
        o.store_bytes(reinterpret_cast<octet *>(const_cast<void *>(data)), size);
        P.send_to(recver, o);
    }

    void mpc_comm::recv(int sender, void *data, size_t size)
    {
        octetStream o;
        P.receive_player(sender, o);
        o.get_bytes(reinterpret_cast<octet *>(data), size);
    }

    void mpc_comm::send_base_cor_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> sendKey,
                                osuCrypto::Channel *channel)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        using namespace osuCrypto;
        size_t num_ot(sendKey.size());
        Channel *sendChannel;
        if (!channel) {
            sendChannel = new Channel;
            *sendChannel = sessions[recver].addChannel();
        } else
            sendChannel = channel;

        AsmSimplestOT baseOT;
        baseOT.send(sendKey, prng, *sendChannel);
        if (!channel) delete sendChannel;
    }

    void mpc_comm::send_base_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendKey)
    {
        using namespace osuCrypto;
        size_t num_ot(sendKey.size());
        std::vector<std::array<block, 2>> sendBlockMsg(num_ot);

        memcpy(sendBlockMsg.data(), sendKey.data(), 2 * num_ot * sizeof(block));
        send_base_cor_ot(recver, sendBlockMsg);
        memcpy(sendKey.data(), sendBlockMsg.data(), 2 * num_ot * sizeof(block));
    }

    void mpc_comm::recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recvKey
                                , osuCrypto::Channel *channel)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        if (choices.size() != recvKey.size()) {
            cerr << "mpc_comm::recv_base_ot : choices.size() != recvKey.size(), " << choices.size() << " != " << recvKey.size() << endl;
            throw std::runtime_error("mpc_comm::recv_base_ot : choices.size() != recvKey.size()");
        }
        using namespace osuCrypto;
        size_t num_ot(recvKey.size());
        Channel *recvChannel;
        if (!channel) {
            recvChannel = new Channel;
            *recvChannel = sessions[sender].addChannel();
        } else
            recvChannel = channel;
        AsmSimplestOT baseOT;

        baseOT.receive(choices, recvKey, prng, *recvChannel);
        if (!channel) delete recvChannel;
    }

    void mpc_comm::recv_base_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvKey)
    {
        using namespace osuCrypto;
        size_t num_ot(recvKey.size());
        std::vector<block> recvBlockMsg(num_ot);

        recv_base_cor_ot(sender, choices, recvBlockMsg);

        memcpy(recvKey.data(), recvBlockMsg.data(), num_ot * sizeof(block));
    }

    void mpc_comm::send_ext_cor_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendKey)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        using namespace osuCrypto;
        size_t num_ot(sendKey.size());

        Channel sendChannel = sessions[recver].addChannel();

        std::vector<std::array<block, 2>> sendBlockMsg(num_ot);

        // IknpOtExtSender extOT;
        // KosOtExtSender extOT;
        SoftSpokenOT::TwoOneMaliciousSender extOT(10);

        // Perform base ot
        size_t cnt_base_ot = extOT.baseOtCount();
        osuCrypto::BitVector base_choices(cnt_base_ot);
        std::vector<block> baseRecv(cnt_base_ot);
        recv_base_cor_ot(recver, base_choices, baseRecv, &sendChannel);

        extOT.setBaseOts(baseRecv, base_choices, prng, sendChannel);

        // Perform extened ot
        memcpy(sendBlockMsg.data(), sendKey.data(), 2 * num_ot * sizeof(block));
        extOT.send(sendBlockMsg, prng, sendChannel);
        memcpy(sendKey.data(), sendBlockMsg.data(), 2 * num_ot * sizeof(block));
    }

    void mpc_comm::recv_ext_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvKey)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        if (choices.size() != recvKey.size()) {
            cerr << "mpc_comm::recv_base_ot : choices.size() != recvKey.size(), " << choices.size() << " != " << recvKey.size() << endl;
            throw std::runtime_error("mpc_comm::recv_base_ot : choices.size() != recvKey.size()");
        }
        using namespace osuCrypto;
        size_t num_ot(recvKey.size());
        Channel recvChannel = sessions[sender].addChannel();
        // IknpOtExtReceiver extOT;
        //KosOtExtReceiver extOT;
        SoftSpokenOT::TwoOneMaliciousReceiver extOT(10);
        

        // Perform base ot
        size_t cnt_base_ot = extOT.baseOtCount();
        std::vector<std::array<block, 2>> baseSend(cnt_base_ot);
        prng.get((unsigned char *)baseSend.data(), cnt_base_ot * 2 * sizeof(block));
        send_base_cor_ot(sender, baseSend, &recvChannel);
        extOT.setBaseOts(baseSend, prng, recvChannel);

        // Perform extened ot
        std::vector<block> recvBlockMsg(num_ot);
        extOT.receive(choices, recvBlockMsg, prng, recvChannel);
        memcpy(recvKey.data(), recvBlockMsg.data(), num_ot * sizeof(block));
    }

    void mpc_comm::send_ext_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg)
    {
        size_t numOT = sendMsg.size();
        std::vector<std::array<block_wrapper, 2>> sendKey(numOT);
        send_ext_cor_ot(recver, sendKey);
        for (size_t i = 0; i < numOT; i++) {
            sendKey[i][0] ^= sendMsg[i][0];
            sendKey[i][1] ^= sendMsg[i][1];
        }
        send(recver, sendKey.data(), numOT * 2 * sizeof(block_wrapper));
    }

    void mpc_comm::recv_ext_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg)
    {
        size_t numOT = recvMsg.size();
        std::vector<block_wrapper> recvKey(numOT);
        std::vector<std::array<block_wrapper, 2>> recvMaskMsg(numOT);
        recv_ext_cor_ot(sender, choices, recvKey);
        recv(sender, recvMaskMsg.data(), numOT * 2 * sizeof(block_wrapper));
        for (size_t i = 0; i < numOT; i++) {
            recvMsg[i] = recvMaskMsg[i][choices[i]] ^ recvKey[i];
        }
    }

    mpc_comm::~mpc_comm(void)
    {
        for (int i(0); i != n_party; ++i) {
            if (i != my_number) sessions[i].stop();
        }
    }

}