#include "mpc_communicator.h"

mpc_comm::mpc_comm(int _n_party, int _my_number)
     : n_party(_n_party), my_number(_my_number), out_buff(n_party), in_buff(n_party), sessions(n_party), ios(),
         N(my_number, n_party, "localhost", 9999),
         P(N),
         setup(P, prime_length)
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

int mpc_comm::get_port(int party)
{
    if (party == -1) party = my_number;
    if (party < 0 || party >= n_party) {
        cerr << "mpc_comm::get_port : Invalid party number: " << party << endl;
        throw std::runtime_error("mpc_comm::get_port : Invalid party number.");
    }
    return N.ports[party];
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

void mpc_comm::send_base_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> sendMsg,
                            osuCrypto::Channel *channel)
{
    assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
    using namespace osuCrypto;
    size_t num_ot(sendMsg.size());
    Channel *sendChannel;
    if (!channel) {
        sendChannel = new Channel;
        *sendChannel = sessions[recver].addChannel();
    } else
        sendChannel = channel;

    osuCrypto::PRNG prng1(block(42532335, 334565));

    AsmSimplestOT baseOT;
    baseOT.send(sendMsg, prng1, *sendChannel);
    if (!channel) delete sendChannel;
}

void mpc_comm::send_base_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg)
{
    using namespace osuCrypto;
    size_t num_ot(sendMsg.size());
    std::vector<std::array<block, 2>> sendBlockMsg(num_ot);

    send_base_ot(recver, sendBlockMsg);

    memcpy(sendMsg.data(), sendBlockMsg.data(), 2 * num_ot * sizeof(block));
}

void mpc_comm::recv_base_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<osuCrypto::block> recvMsg
                            , osuCrypto::Channel *channel)
{
    assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
    if (choices.size() != recvMsg.size()) {
        cerr << "mpc_comm::recv_base_ot : choices.size() != recvMsg.size(), " << choices.size() << " != " << recvMsg.size() << endl;
        throw std::runtime_error("mpc_comm::recv_base_ot : choices.size() != recvMsg.size()");
    }
    using namespace osuCrypto;
    size_t num_ot(recvMsg.size());
    Channel *recvChannel;
    if (!channel) {
        recvChannel = new Channel;
        *recvChannel = sessions[sender].addChannel();
    } else
        recvChannel = channel;
    AsmSimplestOT baseOT;
    osuCrypto::PRNG prng0(block(4253465, 3434565));

    baseOT.receive(choices, recvMsg, prng0, *recvChannel);
    if (!channel) delete recvChannel;
}

void mpc_comm::recv_base_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg)
{
    using namespace osuCrypto;
    size_t num_ot(recvMsg.size());
    std::vector<block> recvBlockMsg(num_ot);

    recv_base_ot(sender, choices, recvBlockMsg);

    memcpy(recvMsg.data(), recvBlockMsg.data(), num_ot * sizeof(block));
}

void mpc_comm::send_ext_ot(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg)
{
    assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
    using namespace osuCrypto;
    size_t num_ot(sendMsg.size());

    Channel sendChannel = sessions[recver].addChannel();
    
    osuCrypto::PRNG prng1(block(42532335, 334565));

    std::vector<std::array<block, 2>> sendBlockMsg(num_ot);

    IknpOtExtSender extOT;

    // Perform base ot
    size_t cnt_base_ot = extOT.baseOtCount();
    osuCrypto::BitVector base_choices(cnt_base_ot);
    std::vector<block> baseRecv(cnt_base_ot);
    recv_base_ot(recver, base_choices, baseRecv, &sendChannel);

    extOT.setBaseOts(baseRecv, base_choices);

    // Perform extened ot
    extOT.send(sendBlockMsg, prng1, sendChannel);
    memcpy(sendMsg.data(), sendBlockMsg.data(), 2 * num_ot * sizeof(block));
}

void mpc_comm::recv_ext_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg)
{
    assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
    if (choices.size() != recvMsg.size()) {
        cerr << "mpc_comm::recv_base_ot : choices.size() != recvMsg.size(), " << choices.size() << " != " << recvMsg.size() << endl;
        throw std::runtime_error("mpc_comm::recv_base_ot : choices.size() != recvMsg.size()");
    }
    using namespace osuCrypto;
    size_t num_ot(recvMsg.size());
    Channel recvChannel = sessions[sender].addChannel();
    IknpOtExtReceiver extOT;
    osuCrypto::PRNG prng0(block(4253465, 3434565));


    // Perform base ot
    size_t cnt_base_ot = extOT.baseOtCount();
    std::vector<std::array<block, 2>> baseSend(cnt_base_ot);
    send_base_ot(sender, baseSend, &recvChannel);
    extOT.setBaseOts(baseSend);

    // Perform extened ot
    std::vector<block> recvBlockMsg(num_ot);
    extOT.receive(choices, recvBlockMsg, prng0, recvChannel);
    memcpy(recvMsg.data(), recvBlockMsg.data(), num_ot * sizeof(block));
}

mpc_comm::~mpc_comm(void)
{
    for (int i(0); i != n_party; ++i) {
        if (i != my_number) sessions[i].stop();
    }
}
