#include <iostream>

#include <emp-tool/emp-tool.h>
#include <emp-ot/emp-ot.h>

#include "global.h"
#include "field_int.h"
#include "OPV.h"
#include "mpc_communicator.h"
#include "Chase_shuffle.h"
#include "test_shuffle.h"


int main(int, char **argv) {
    mpc_comm com(atoi(argv[1]));
    std::cout << "Initializing mpc_comm, # party = " << com.n << "..." << std::endl;
    com.init();
    test_Song_shuffle(com);
    return 0;
}
