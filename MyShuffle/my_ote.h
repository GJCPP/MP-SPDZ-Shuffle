#pragma once

#include <iostream>

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Common/BitVector.h>

#include "libOTe/libOTe/Base/SimplestOT.h"
#include <libOTe/libOTe/TwoChooseOne/KosOtExtSender.h>
#include <libOTe/libOTe/TwoChooseOne/KosOtExtReceiver.h>


#include "vectors.h"
#include "block_wrapper.h"

/*
    A wrapper for base OT.
*/
namespace my_ote {
    void recv_base_cor_ot(
                osuCrypto::BitVector choices,
                osuCrypto::span<osuCrypto::block> recvKey,
                osuCrypto::PRNG& osuPrg,
                osuCrypto::Channel *channel);

    void send_base_cor_ot(
                osuCrypto::span<std::array<osuCrypto::block, 2>> sendKey,
                osuCrypto::PRNG& osuPrg,
                osuCrypto::Channel *channel);
}
