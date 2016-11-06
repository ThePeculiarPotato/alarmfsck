#ifndef ORG_WALRUS_ALARMFUCK_COMMON_H
#define ORG_WALRUS_ALARMFUCK_COMMON_H

#include <cerrno>
#include <system_error>

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

#endif // ORG_WALRUS_ALARMFUCK_COMMON_H
