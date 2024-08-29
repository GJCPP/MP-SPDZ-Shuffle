#include "my_benchmark.h"

all_record::all_record(std::string _name)
    : name(_name)
{
    records.resize(all_logsz.size());
}

void all_record::save() const
{
    std::ofstream ofs(name + ".csv");
    for (int logsz : all_logsz) {
        ofs << logsz << ", ";
    } ofs << std::endl;
    ofs << "offline time" << std::endl;
    for (const record& r : records) {
        ofs << r.off_time << ", ";
    } ofs << std::endl;
    ofs << "offline comm" << std::endl;
    for (const record& r : records) {
        ofs << r.off_comm << ", ";
    } ofs << std::endl;
    ofs << "online time" << std::endl;
    for (const record& r : records) {
        ofs << r.on_time << ", ";
    } ofs << std::endl;
    ofs << "online comm" << std::endl;
    for (const record& r : records) {
        ofs << r.on_comm << ", ";
    } ofs << std::endl;
    ofs.close();
}

double comm_time(size_t comm)
{
    const size_t rate = 40 * 1024 * 1024; // 40MB/s
    return comm / rate;
}

void benchmark_Song_shuffle(gjcShuffle::mpc_comm &com)
{
    using namespace song2023;
    all_record rec("Song_shuffle_n" + std::to_string(com.get_n_party()));
    if (com.get_my_number() == 0) std::cout << "Benchmarking Song_shuffle." << std::endl;
    for (size_t i(0); i != all_logsz.size(); ++i) {
        int logsz = all_logsz[i];
        double best_on_time(1e9);
        vectors<ShareType> val(1 << logsz, veclen);
        for (size_t k(0); k != val.size(); ++k) {
            val.at(k) = ShareType::constant(k, com.get_my_number(), ShareType::get_mac_key());
        }
        for (size_t j(0); j != all_log_batch.size(); ++j) {
            if (com.get_my_number() == 0) std::cout << "logsz: " << logsz << ", log_batch: " << all_log_batch[j] << std::endl;
            double off_time, on_time;
            size_t off_comm, on_comm;
            int logbatch = all_log_batch[j];
            com.reset_total_comm();
            std::vector<song2023::shuffle_session *> plans;
            off_time = clock();
            for (int rank(0); rank != rep; ++rank) {
                plans.push_back(song2023::book_shuffle_session<ShareType>(com, 
                                logsz, veclen, logbatch, permutation(1 << logsz, true)));
            }
            song2023::process_all_orders(com);

            // Record
            off_time = (clock() - off_time) / CLOCKS_PER_SEC;
            off_comm = com.count_total_comm();
            off_time += comm_time(off_comm);
            com.reset_total_comm();

            on_time = clock();
            for (auto plan : plans) {
                plan->perform(com, val);
            }
            on_time = (clock() - on_time) / CLOCKS_PER_SEC;
            on_comm = com.count_total_comm();
            on_time += comm_time(on_comm);
            com.reset_total_comm();
            if (on_time < best_on_time) {
                best_on_time = on_time;
                rec.records[i].off_time = off_time / rep;
                rec.records[i].on_time = on_time / rep;
                rec.records[i].off_comm = off_comm / rep;
                rec.records[i].on_comm = on_comm / rep;
            }
            if (com.get_my_number() == 0) rec.save();
        }
    }
    if (com.get_my_number() == 0) std::cout << "Done." << std::endl;
}


void benchmark_my_shuffle(gjcShuffle::mpc_comm &com)
{
    using namespace gjcShuffle;
    all_record rec("my_shuffle_n" + std::to_string(com.get_n_party()));
    if (com.get_my_number() == 0) std::cout << "Benchmarking my_shuffle." << std::endl;
    for (size_t i(0); i != all_logsz.size(); ++i) {
        int logsz = all_logsz[i];
        double best_on_time(1e9);
        vectors<ShareType> val(1 << logsz, veclen);
        for (size_t k(0); k != val.size(); ++k) {
            val.at(k) = ShareType::constant(k, com.get_my_number(), ShareType::get_mac_key());
        }        
        for (size_t j(0); j != all_log_batch.size(); ++j) {
            if (com.get_my_number() == 0) std::cout << "logsz: " << logsz << ", log_batch: " << all_log_batch[j] << std::endl;
            double off_time, on_time;
            size_t off_comm, on_comm;
            int logbatch = all_log_batch[j];
            com.reset_total_comm();
            std::vector<gjcShuffle::shuffle_session *> plans;
            off_time = clock();
            for (int rank(0); rank != rep; ++rank) {
                plans.push_back(gjcShuffle::book_shuffle_session<ShareType>(com, 
                                logsz, veclen, logbatch, permutation(1 << logsz, true)));
            }
            gjcShuffle::process_all_orders(com);

            // Record
            off_time = (clock() - off_time) / CLOCKS_PER_SEC;
            off_comm = com.count_total_comm();
            off_time += comm_time(off_comm);
            com.reset_total_comm();

            on_time = clock();
            for (auto plan : plans) {
                plan->perform(com, val);
            }
            on_time = (clock() - on_time) / CLOCKS_PER_SEC;
            on_comm = com.count_total_comm();
            on_time += comm_time(on_comm);
            com.reset_total_comm();
            if (on_time < best_on_time) {
                best_on_time = on_time;
                rec.records[i].off_time = off_time / rep;
                rec.records[i].on_time = on_time / rep;
                rec.records[i].off_comm = off_comm / rep;
                rec.records[i].on_comm = on_comm / rep;
            }
            if (com.get_my_number() == 0) rec.save();
        }
    }
    if (com.get_my_number() == 0) std::cout << "Done." << std::endl;
}