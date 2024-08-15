#include "mpc_communicator.h"

mpc_comm::mpc_comm(int _n_party, int _my_number)
     : n_party(_n_party), my_number(_my_number), out_buff(n_party), in_buff(n_party),
         N(my_number, n_party, "localhost", 9999),
         P(N),
         setup(P, prime_length)
{
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
