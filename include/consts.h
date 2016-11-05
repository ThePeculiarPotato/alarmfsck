#ifndef ORG_WALRUS_ALARMFUCK_CONSTS
#define ORG_WALRUS_ALARMFUCK_CONSTS

#include <string>

const int padding = 10;
const int suggested_hours = 8;
const std::string data_dir= "data/";
const std::string bin_dir= "bin/";
const std::string hostage_file= "hostages.af";
const std::string hostage_archive= "hostages.tar";
const std::string hostage_compressed= "hostages.tar.gz";
const std::string hib_exec= "hibernator";
const std::string audio_file= "rullGenocide.oga";
const std::string file_delim= "/";

std::string get_executable_dir();
std::string cpp_realpath(const std::string&);

#endif // ORG_WALRUS_ALARMFUCK_CONSTS
