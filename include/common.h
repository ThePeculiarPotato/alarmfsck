#ifndef ORG_WALRUS_ALARMFSCK_COMMON_H
#define ORG_WALRUS_ALARMFSCK_COMMON_H

#include <cerrno>
#include <system_error>
#include <vector>

const int padding = 10;
const int suggested_hours = 8;
const std::string package_name = "alarmfsck";
const std::string data_dir= "share/";
const std::string bin_dir= "bin/";
const std::string hostage_file= "hostages.af";
const std::string hostage_archive= "hostages.tar";
const std::string hostage_compressed= "hostages.tar.gz";
const std::string hostage_encrypted = "hostages.enc";
const std::string hib_exec= "hibernator";
const std::string ringer_exec= "alarmfsck";
const std::string audio_file= "genocide.oga";
const char file_delim= '/';

class AfSystemException : public std::system_error
{
private:
    const std::string message;
public:
    AfSystemException(std::string message, int code): std::system_error(code,
	    std::system_category()), message(message) {};
    AfSystemException(std::string message): AfSystemException(message, errno) {};
    const std::string& get_message() const {return message;};
};

class AfUserException : public std::logic_error
{
public:
    AfUserException(std::string message): logic_error(message){};
};

namespace afCommon {
    void erase_file(const std::string&);
    std::string get_executable_dir();
    std::string cpp_realpath(const std::string&);
    std::string humanize_byte_count(const off_t&);
}

#endif // ORG_WALRUS_ALARMFSCK_COMMON_H
