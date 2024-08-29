#include "mpc_communicator.h"

namespace gjcShuffle {

    mpc_comm::mpc_comm(int _n_party, int _my_number)
        : n_party(_n_party), my_number(_my_number), sessions(n_party), ios(),
            N(my_number, n_party, "localhost", 9999),
            P(N),
            setup(P, prime_length),
            osuPrg(osuCrypto::sysRandomSeed()),
            prg(),
            shared_mask(n_party),
            alpha(),
            random_resource(),
            expand_random_size(0),
            cnt_private_output(n_party),
            otSendChannel(n_party, nullptr),
            otRecvChannel(n_party, nullptr)
    {
        prg.InitSeed();
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

        ClearType alphai = ShareType::get_mac_key();
        input_init();
        input_append_all(alphai);
        input_exchange();
        for (int i(0); i != n_party; ++i) {
            alpha += input_consume(i);
        }
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

    void mpc_comm::rand_bytes(octet * dest, size_t size)
    {
        osuPrg.get(dest, size);
    }

    void mpc_comm::rand_blocks(block_wrapper *dest, size_t num)
    {
        osuPrg.get(dest, num);
    }

    ClearType mpc_comm::rand_int()
    {
        ClearType res;
        res.randomize(prg);
        return res;
    }

    void mpc_comm::rand_int(ClearType &dest)
    {
        dest.randomize(prg);
    }

    void mpc_comm::rand_int(std::vector<ClearType> &dest)
    {
        for (auto &i : dest) i.randomize(prg);
    }

    void mpc_comm::rand_int(vectors<ClearType> &dest)
    {
        for (auto &i : dest) i.randomize(prg);
    }

    void mpc_comm::input_init()
    {
        input->reset_all(P);
    }

    void mpc_comm::input_append_all(const ClearType &val)
    {
        input->add_from_all(val);
    }

    void mpc_comm::input_append(int party, const ClearType &val)
    {
        if (party == my_number) input->add_mine(val);
        else input->add_other(party);
    }

    void mpc_comm::input_append_all(const vectors<ClearType>& val)
    {
        for (const ClearType& v : val)
            input->add_from_all(v);
    }

    void mpc_comm::input_append(int party, const vectors<ClearType> &val)
    {
        if (party == my_number) {
            for (auto& v : val) input->add_mine(v);
        } else {
            for (auto& v : val) input->add_other(party);
        }
    }

    void mpc_comm::input_exchange()
    {
        input->exchange();
    }

    void mpc_comm::input_consume(int party, ShareType& val)
    {
        val = input->finalize(party);
    }

    void mpc_comm::input_consume(int party, vectors<ShareType>& val)
    {
        for (auto& v : val) v = input->finalize(party);
    }

    ShareType mpc_comm::input_consume(int party)
    {
        return input->finalize(party);
    }

    void mpc_comm::mul_init()
    {
        protocol->init_mul();
    }

    void mpc_comm::mul_exchange()
    {
        protocol->exchange();
    }

    void mpc_comm::mul_consume(ShareType &val)
    {
       val = protocol->finalize_mul();
    }
     
    void mpc_comm::mul_consume(vectors<ShareType>& val)
    {
        for (auto& v : val) v = protocol->finalize_mul();
    }

    ShareType mpc_comm::mul_consume()
    {
        return protocol->finalize_mul();
    }

    void mpc_comm::mul_append(const vectors<ShareType> &v1, const vectors<ShareType> &v2)
    {
        for (size_t i(0); i != v1.size(); ++i) {
            protocol->prepare_mul(v1.at(i), v2.at(i));
        }
    }

    void mpc_comm::mul_append(const ShareType &v1, const ShareType &v2)
    {
        protocol->prepare_mul(v1, v2);
    }

    void mpc_comm::output_immediately(const ShareType& val, ClearType& res)
    {
        output_init();
        output_append(val);
        output_exchange();
        output_consume(res);
    }

    void mpc_comm::output_immediately(const vectors<ShareType>& val, vectors<ClearType>& res)
    {
        output_init();
        output_append(val);
        output_exchange();
        res.resize(val.num, val.len);
        output_consume(res);
    }

    void mpc_comm::output_immediately(const vector<ShareType>& val, std::vector<ClearType>& res)
    {
        output_init();
        output_append(val);
        output_exchange();
        res.resize(val.size());
        output_consume(res);
    }

    void mpc_comm::output_init()
    {
        output->init_open(P);
    }

    void mpc_comm::output_append(const ShareType &val)
    {
        output->prepare_open(val);
    }

    void mpc_comm::output_append(const vectors<ShareType> &val)
    {
        for (auto& v : val) output->prepare_open(v);
    }

    void mpc_comm::output_append(const std::vector<ShareType>& val)
    {
        for (auto& v : val) output->prepare_open(v);
    }

    void mpc_comm::output_exchange()
    {
        output->exchange(P);
    }

    void mpc_comm::output_consume(ClearType &val)
    {
        val = output->finalize_open();
    }

    void mpc_comm::output_consume(vectors<ClearType> & val)
    {
        for (auto& v : val) v = output->finalize_open();
    }

    void mpc_comm::output_consume(std::vector<ClearType> &val)
    {
        for (auto& v : val) v = output->finalize_open();
    }

    ClearType mpc_comm::output_consume()
    {
        return  output->finalize_open();
    }

    void mpc_comm::prepare_more_random_lazy(size_t num)
    {
        expand_random_size += num;
    }

    void mpc_comm::prepare_more_random_now(size_t num)
    {
        static size_t default_expand(DEFAULT_EXPAND_SIZE);
        size_t expand = expand_random_size + num;
        expand_random_size = 0;
        if (expand == 0) { // Called under situation random_resourece.empty() && expand_random_size == 0
            expand = default_expand;
            default_expand <<= 1;
        }
        // Generate random numbers
        input_init();
        for (size_t i(0); i != expand; ++i) {
            auto t = rand_int();
            input_append_all(t);
        }
        input_exchange();
        for (size_t i(0); i != expand; ++i) {
            ShareType sum = ShareType::constant(0, my_number, ShareType::get_mac_key());
            for (int j(0); j != n_party; ++j) {
                sum += input_consume(j);
            }
            random_resource.push_back(sum);
        }
    }

    ShareType mpc_comm::get_random()
    {
        if (random_resource.empty()) {
            prepare_more_random_now();
        }
        ShareType res = random_resource.front();
        random_resource.pop_front();
        return res;
    }

    void mpc_comm::prepare_output_mask(size_t expand)
    {
        input_init();
        for (size_t i(0); i != expand; ++i) {
            ClearType tmp = rand_int();
            input->add_from_all(tmp);
            clear_mask.push_back(tmp);
        }
        input_exchange();
        for (int i(0); i != n_party; ++i) {
            for (size_t j(0); j != expand; ++j) {
                shared_mask[i].push_back(input_consume(i));
            }
        }
    }

    void mpc_comm::prepare_more_private_output_lazy(int party, size_t num)
    {
        cnt_private_output[party] += num;
    }

    void mpc_comm::prepare_more_private_output_now(size_t num)
    {
        size_t expand = 0;
        for (int party(0); party != n_party; ++party) {
            if (cnt_private_output[party] + num > expand) {
                expand = cnt_private_output[party] + num;
            }
        }
        prepare_output_mask(expand);
    }

    void mpc_comm::private_output_init()
    {
        prepare_more_private_output_now();
        output->init_open(P);
    }

    void mpc_comm::private_output_append(int party, const ShareType &val)
    {
        output_append(val + shared_mask[party].front());
        shared_mask[party].pop_front();
    }

    void mpc_comm::private_output_append(int party, const vectors<ShareType> &val)
    {
        for (auto& v : val) {
            output_append(v + shared_mask[party].front());
            shared_mask[party].pop_front();
        }
    }

    void mpc_comm::private_output_exchange()
    {
        bool needExpand(false);
        
        output_exchange();
    }

    void mpc_comm::private_output_consume(int party, ClearType &val)
    {
        val = output_consume();
        if (my_number == party) {
            val -= clear_mask.front();
            clear_mask.pop_front();
        }
    }

    void mpc_comm::private_output_consume(int party, vectors<ClearType> &val)
    {
        for (ClearType& v : val) {
            v = output_consume();
            if (my_number == party) {
                v -= clear_mask.front();
                clear_mask.pop_front();
            }
        }
    }

    ClearType mpc_comm::private_output_consume(int party)
    {
        ClearType res = output_consume();
        if (my_number == party) {
            res -= clear_mask.front();
            clear_mask.pop_front();
        }
        return res;
    }

    void mpc_comm::output_check()
    {
        output->Check(P);
    }

    void mpc_comm::send(int recver, const void * data, size_t size)
    {
        octetStream o;
        o.append(reinterpret_cast<octet *>(const_cast<void *>(data)), size);
        P.send_to(recver, o);
    }

    void mpc_comm::recv(int sender, void *data, size_t size)
    {
        octetStream o;
        P.receive_player(sender, o);
        o.consume(reinterpret_cast<octet *>(data), size);
    }

    void mpc_comm::send_base_cor_ot(int recver, osuCrypto::span<std::array<osuCrypto::block, 2>> sendKey,
                                osuCrypto::Channel *channel)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        using namespace osuCrypto;
        Channel *sendChannel;
        if (!channel) {
            sendChannel = new Channel;
            *sendChannel = sessions[recver].addChannel();
        } else {
            sendChannel = channel;
        }


        my_ote::send_base_cor_ot(sendKey, osuPrg, sendChannel);


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
        if (choices.size() != static_cast<size_t>(recvKey.size())) {
            cerr << "mpc_comm::base_ot_recv : choices.size() != recvKey.size(), " << choices.size() << " != " << recvKey.size() << endl;
            throw std::runtime_error("mpc_comm::base_ot_recv : choices.size() != recvKey.size()");
        }
        using namespace osuCrypto;
        Channel *recvChannel;
        if (!channel) {
            recvChannel = new Channel;
            *recvChannel = sessions[sender].addChannel();
        } else {
            recvChannel = channel;
        }
        

        my_ote::recv_base_cor_ot(choices, recvKey, osuPrg, recvChannel);


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

        std::vector<Channel*>& allSendChannel = otSendChannel;
        static std::vector<KosOtExtSender*> allExtOT;
        if (allExtOT.empty()) {
            allExtOT.resize(n_party);
        }
        if (allSendChannel[recver] == nullptr) {
            allSendChannel[recver] = new Channel;
            *allSendChannel[recver] = sessions[recver].addChannel();
            allExtOT[recver] = new KosOtExtSender;


            Channel& sendChannel = *allSendChannel[recver];
            KosOtExtSender& extOT = *allExtOT[recver];

            // Perform base ot
            size_t cnt_base_ot = extOT.baseOtCount();
            osuCrypto::BitVector base_choices(cnt_base_ot);
            std::vector<block> baseRecv(cnt_base_ot);
            recv_base_cor_ot(recver, base_choices, baseRecv, &sendChannel);

            allExtOT[recver]->setBaseOts(baseRecv, base_choices, osuPrg, sendChannel);
        }
        Channel& sendChannel = *allSendChannel[recver];
        KosOtExtSender& extOT = *allExtOT[recver];

        std::vector<std::array<block, 2>> sendBlockMsg(num_ot);

        // Perform extened ot
        memcpy(sendBlockMsg.data(), sendKey.data(), 2 * num_ot * sizeof(block));
        extOT.send(sendBlockMsg, osuPrg, sendChannel);
        memcpy(sendKey.data(), sendBlockMsg.data(), 2 * num_ot * sizeof(block));
    }

    void mpc_comm::recv_ext_cor_ot(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvKey)
    {
        assert(sizeof(osuCrypto::block) == sizeof(block_wrapper));
        if (choices.size() != static_cast<size_t>(recvKey.size())) {
            cerr << "mpc_comm::base_ot_recv : choices.size() != recvKey.size(), " << choices.size() << " != " << recvKey.size() << endl;
            throw std::runtime_error("mpc_comm::base_ot_recv : choices.size() != recvKey.size()");
        }
        using namespace osuCrypto;
        size_t num_ot(recvKey.size());

        
        std::vector<Channel*>& allRecvChannel = otRecvChannel;
        static std::vector<KosOtExtReceiver*> allExtOT;
        if (allExtOT.empty()) {
            allExtOT.resize(n_party);
        }
        if (allRecvChannel[sender] == nullptr) {
            allRecvChannel[sender] = new Channel;
            *allRecvChannel[sender] = sessions[sender].addChannel();
            allExtOT[sender] = new KosOtExtReceiver;
            

            Channel& recvChannel = *allRecvChannel[sender];
            KosOtExtReceiver& extOT = *allExtOT[sender];

            // Perform base ot
            size_t cnt_base_ot = extOT.baseOtCount();
            std::vector<std::array<block, 2>> baseSend(cnt_base_ot);
            osuPrg.get((unsigned char *)baseSend.data(), cnt_base_ot * 2 * sizeof(block));
            send_base_cor_ot(sender, baseSend, &recvChannel);
            extOT.setBaseOts(baseSend, osuPrg, recvChannel);
        }

        Channel& recvChannel = *allRecvChannel[sender];
        KosOtExtReceiver& extOT = *allExtOT[sender];

        // Perform extened ot
        std::vector<block> recvBlockMsg(num_ot);
        extOT.receive(choices, recvBlockMsg, osuPrg, recvChannel);
        memcpy(recvKey.data(), recvBlockMsg.data(), num_ot * sizeof(block));
    }

    void mpc_comm::ext_ot_send(int recver, osuCrypto::span<std::array<block_wrapper, 2>> sendMsg)
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

    void mpc_comm::ext_ot_send(int recver, osuCrypto::span<block_wrapper> msg0, osuCrypto::span<block_wrapper> msg1)
    {
        assert(msg0.size() == msg1.size());
        size_t numOT = msg0.size();
        std::vector<std::array<block_wrapper, 2>> sendKey(numOT);
        send_ext_cor_ot(recver, sendKey);
        for (size_t i = 0; i < numOT; i++) {
            sendKey[i][0] ^= msg0[i];
            sendKey[i][1] ^= msg1[i];
        }
        send(recver, sendKey.data(), numOT * 2 * sizeof(block_wrapper));
    }

    void mpc_comm::ext_ot_recv(int sender, osuCrypto::BitVector choices, osuCrypto::span<block_wrapper> recvMsg)
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

    /*
    * MAC check for the shuffle protocol by Song et al.
    */
    void mpc_comm::mac_check(const vectors<ShareType> &val)
    {
        size_t num(val.size());
        ShareType shared_r(get_random());
        std::vector<ShareType> shared_c(num);
        std::vector<ClearType> clear_c(num);
        for (size_t i(0); i != num; ++i) {
            shared_c[i] = get_random();
        }
        output_immediately(shared_c, clear_c);
        ShareType shared_t(shared_r);
        ClearType clear_t;
        for (size_t i(0); i != num; ++i) {
            shared_t += clear_c[i] * val.at(i);
        } // <t> = <r> + sum([c_i] * <val_i>)
        output_immediately(shared_t, clear_t);
        ClearType shared_s = shared_t.get_mac() - clear_t * ShareType::get_mac_key();
        std::vector<ClearType> all_s(n_party);
        
        
        commit_and_open(shared_s, all_s);


        ClearType sum(0);
        for (int i(0); i != n_party; ++i) {
            sum += all_s[i];
        }
        if (std::accumulate(all_s.begin(), all_s.end(), ClearType(0)).is_zero() == false) {
            std::cerr << "mpc_comm::mac_check : MAC check failed." << std::endl;
            throw std::runtime_error("mpc_comm::mac_check : MAC check failed.");
        }
    }

    size_t mpc_comm::count_total_comm() const
    {
        size_t ret = 0;
        for (auto channel : otSendChannel) {
            if (channel) ret += channel->getTotalDataSent();
        }
        ret += P.total_comm().sent;
        return ret;
    }

    void mpc_comm::reset_total_comm()
    {
        for (auto channel : otSendChannel) {
            if (channel) channel->resetStats();
        }
        P.reset_stats();
    }

    mpc_comm::~mpc_comm()
    {
        for (int i(0); i != n_party; ++i) {
            sessions[i].stop();
        }
        ios.stop();
        for (auto channel : otSendChannel) {
            if (channel) delete channel;
        }
        for (auto channel : otRecvChannel) {
            if (channel) delete channel;
        }
    }
}