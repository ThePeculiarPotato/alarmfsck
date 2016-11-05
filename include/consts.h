#ifndef ORG_WALRUS_ALARMFUCK_CONSTS
#define ORG_WALRUS_ALARMFUCK_CONSTS

#define PADDING 10
#define DATA_DIR "data/"
#define BIN_DIR "bin/"
#define HOSTAGE_FILE "hostages.af"
#define HOSTAGE_ARCHIVE "hostages.tar"
#define HOSTAGE_COMPRESSED "hostages.tar.gz"
#define HIB_EXEC "hibernator"
#define AUDIO_FILE "rullGenocide.oga"
#define FILE_DELIM '/'
#define SUGGESTED_HOURS 8

std::string get_executable_dir();
std::string cpp_realpath(const std::string&);

#endif // ORG_WALRUS_ALARMFUCK_CONSTS
