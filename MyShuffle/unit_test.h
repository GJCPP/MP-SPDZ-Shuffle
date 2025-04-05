#pragma once

#include "mpc_communicator.h"
#include "OPV.h"

namespace myShuffle {
    bool test_com(mpc_comm& com);

    bool test_broadcast(mpc_comm &com);

    bool test_ote(mpc_comm &com);

    bool test_opv(mpc_comm &com);

    bool test_all(mpc_comm& com);
}
