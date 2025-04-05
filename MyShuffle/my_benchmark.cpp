#include "my_benchmark.h"
#include "my_timer.h"


all_record::all_record(std::string _filename)
    : filename(_filename)
{
    records.resize(default_all_logsz.size());
}

void all_record::save() const
{
    std::ofstream ofs(filename);
    ofs << "logsz, ";
    for (const record& r : records) {
        ofs << r.logsz << ", ";
    } ofs << std::endl;
    ofs << "offline comm, ";
    for (const record& r : records) {
        if (r.off_comm != size_t(-1)) ofs << r.off_comm << ", ";
        else ofs << "-1, ";
    } ofs << std::endl;
    ofs << "offline time, ";
    for (const record& r : records) {
        ofs << r.off_time << ", ";
    } ofs << std::endl;
    ofs << "online comm, ";
    for (const record& r : records) {
        if (r.on_comm != size_t(-1)) ofs << r.on_comm << ", ";
        else ofs << "-1, ";
    } ofs << std::endl;
    ofs << "online time, ";
    for (const record& r : records) {
        ofs << r.on_time << ", ";
    } ofs << std::endl;
    ofs.close();
}

double comm_time(size_t comm)
{
    const size_t rate = 40 * 1024 * 1024; // 40MB/s
    return comm / rate;
}

std::string get_filename(std::string protocol, std::string target, int n_party) {
    return protocol + "_" + target + "_n_" + std::to_string(n_party) + ".csv";
}

std::vector<std::string> split(const std::string &s, std::string delim) {
    std::vector<std::string> result;
    std::string item;
    for (char c : s) {
        if (delim.find(c) != std::string::npos) {
            result.push_back(item);
            item.clear();
        } else {
            item += c;
        }
    }
    if (!item.empty()) {
        result.push_back(item);
    }
    return result;
}

std::string remove(const std::string &s, std::string delim) {
    std::string result;
    for (char c : s) {
        if (delim.find(c) == std::string::npos) {
            result += c;
        }
    }
    return result;
}

void load_record(std::string filename, all_record &rec) {
    std::ifstream ifs(filename);
    rec.records.clear();
    if (!ifs.good()) {
        for (int logsz : default_all_logsz) {
            rec.records.push_back({});
            rec.records.back().logsz = logsz;
            rec.records.back().off_comm = -1;
            rec.records.back().off_time = -1;
            rec.records.back().on_comm = -1;
            rec.records.back().on_time = -1;
        }
        return ;
    }
    std::vector<std::vector<std::string>> tokens;
    std::string line;
    while (std::getline(ifs, line)) {
        tokens.push_back(split(remove(line, " \n"), ","));
    }
    for (size_t i(0); i != tokens[0].size(); ++i) {
        if (i == 0) continue;
        rec.records.push_back({});
        rec.records[i - 1].logsz = std::stoi(tokens[0][i]);
        rec.records[i - 1].off_comm = std::stoll(tokens[1][i]);
        rec.records[i - 1].off_time = std::stod(tokens[2][i]);
        rec.records[i - 1].on_comm = std::stoll(tokens[3][i]);
        rec.records[i - 1].on_time = std::stod(tokens[4][i]);
    }
    ifs.close();
}

void execute_Song_shuffle(myShuffle::mpc_comm &com,
                            int logsz, int veclen, int logbatch, int rep,
                            size_t& off_comm, double& off_time,
                            size_t& on_comm, double& on_time)
{
    using namespace song2023;
    static vectors<ShareType> val; val.resize(1 << logsz, veclen);
    size_t val_sz= val.size();
    for (size_t k(0); k != val_sz; ++k) {
        val.at(k) = ShareType::constant(k, com.get_my_number(), ShareType::get_mac_key());
    }
    com.reset_total_comm();
    static std::vector<song2023::shuffle_session *> plans; plans.clear();

    // Offline Phase
    com.set_offline();

    myShuffle::timer off_time_timer, on_time_timer;
    off_time_timer.tick();
    for (int rank(0); rank != rep; ++rank) {
        plans.push_back(song2023::book_shuffle_session<ShareType>(com, 
                        logsz, veclen, logbatch, permutation(1 << logsz, true)));
    }
    song2023::process_all_orders(com);

    // Record
    off_time_timer.tock();
    off_time = off_time_timer.duration();
    off_comm = com.count_total_comm();
    off_time += comm_time(off_comm);
    com.reset_total_comm();
    
    com.output_check();
    // Online Phase
    com.set_online();
    on_time_timer.tick();
    for (auto plan : plans) {
        plan->perform(com, val);
    }

    // Record
    on_time_timer.tock();
    on_time = on_time_timer.duration();
    on_comm = com.count_total_comm();
    on_time += comm_time(on_comm);
    com.reset_total_comm();

    for (auto plan : plans) {
        delete plan;
    }

    // Average
    off_comm = myShuffle::insecure_static_sum(com, off_comm) / com.get_n_party();
    off_time = myShuffle::insecure_static_sum(com, off_time) / com.get_n_party();
    on_comm = myShuffle::insecure_static_sum(com, on_comm) / com.get_n_party();
    on_time = myShuffle::insecure_static_sum(com, on_time) / com.get_n_party();
}

void execute_my_shuffle(myShuffle::mpc_comm &com,
                            int logsz, int veclen, int logbatch, int rep,
                            size_t& off_comm, double& off_time,
                            size_t& on_comm, double& on_time)
{
    using namespace myShuffle;
    static vectors<ShareType> val; val.resize(1 << logsz, veclen);
    size_t val_sz= val.size();
    for (size_t k(0); k != val_sz; ++k) {
        val.at(k) = ShareType::constant(k, com.get_my_number(), ShareType::get_mac_key());
    }
    com.reset_total_comm();
    static std::vector<myShuffle::shuffle_session *> plans; plans.clear();

    // Offline Phase
    com.set_offline();
    myShuffle::timer off_time_timer, on_time_timer;
    off_time_timer.tick();
    for (int rank(0); rank != rep; ++rank) {
        plans.push_back(myShuffle::book_shuffle_session<ShareType>(com, 
                        logsz, veclen, logbatch, permutation(1 << logsz, true)));
    }
    myShuffle::process_all_orders(com);

    // Record
    off_time_timer.tock();
    off_time = off_time_timer.duration();
    off_comm = com.count_total_comm();
    off_time += comm_time(off_comm);
    com.reset_total_comm();

    // std::cout << "Offline Phase Done." << std::endl;

    // Online Phase
    com.set_online();
    on_time_timer.tick();
    for (auto plan : plans) {
        plan->perform(com, val);
    }

    // Record
    on_time_timer.tock();
    on_time = on_time_timer.duration();
    on_comm = com.count_total_comm();
    on_time += comm_time(on_comm);
    com.reset_total_comm();

    for (auto plan : plans) {
        delete plan;
    }

    // Average
    off_comm = myShuffle::insecure_static_sum(com, off_comm) / com.get_n_party();
    off_time = myShuffle::insecure_static_sum(com, off_time) / com.get_n_party();
    on_comm = myShuffle::insecure_static_sum(com, on_comm) / com.get_n_party();
    on_time = myShuffle::insecure_static_sum(com, on_time) / com.get_n_party();
}
