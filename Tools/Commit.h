#ifndef _Commit
#define _Commit

/* Define the hash based commitment scheme */

#include "Tools/octetStream.h"
#include "Networking/Player.h"

/*
 * Commit using comm = hash(send_player || message || r)
 * where r is SEED_SIZE random bytes
 */
void Commit(octetStream& comm, octetStream& open, const octetStream& message, int send_player);

bool Open(octetStream& message, const octetStream& comm, const octetStream& open, int send_player);

// same as above using less memory
class Commitment
{
    int send_player;

public:
    // open only contains the randomness
    octetStream comm, open;

    Commitment(int send_player) : send_player(send_player) {}
    void commit(const octetStream& message);
    void commit(const octetStream& message, const octetStream& open);
    void check(const octetStream& message, const octetStream& comm,
            const octetStream& open);
};

class AllCommitments
{
    const Player& P;
    vector<octetStream> comms, opens;

public:
    vector<octetStream> messages;

    AllCommitments(const Player& P) : P(P), comms(P.num_players()), opens(P.num_players()) {}
    // no checks yet
    void commit_and_open(const octetStream& message);
    void commit(const octetStream& message);
    
    template <typename T>
    void commit_and_open(const T& message, std::vector<T>& open_msg) {
        octetStream os;
        os.store(message);
        commit(os);
        open(os);
        open_msg.resize(P.num_players());
        for (int i = 0; i < P.num_players(); i++) {
            messages[i].get(open_msg[i]);
        }
    }
    template <typename T>
    void commit(const T& message) {
        octetStream os;
        os.append(message);
        commit(os);
    }

    // only open randomness
    void open();
    // open and check everything
    void open(const octetStream& message);
    void check(int player, const octetStream& message);
    void check_relative(int diff, const octetStream& message);
};

#endif

