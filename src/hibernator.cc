#include "common.h"
#include <ctime>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <glibmm/spawn.h>
extern "C"{
#include <linux/rtc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
}

#define DEFAULT_RTC			"rtc0"
#define DEFAULT_RTC_DEVICE		"/dev/" DEFAULT_RTC
#define SYS_WAKEUP_PATH			"/sys/class/rtc/" DEFAULT_RTC "/device/power/wakeup"
#define SYS_POWER_STATE_PATH		"/sys/power/state"

void error_plain(std::string preString){
    std::cerr << "Error: " << preString << std::endl;
}

void error_errno(std::string preString){
    std::cerr << "Error: " << preString << ": " << strerror(errno) << std::endl;
}

bool setup_alarm(int fd, time_t *wakeup)
{
    struct tm		*tm;
    struct rtc_wkalrm	wake = { 0 };

    tm = gmtime(wakeup);
    wake.time.tm_sec = tm->tm_sec;
    wake.time.tm_min = tm->tm_min;
    wake.time.tm_hour = tm->tm_hour;
    wake.time.tm_mday = tm->tm_mday;
    wake.time.tm_mon = tm->tm_mon;
    wake.time.tm_year = tm->tm_year;
    /* wday, yday, and isdst fields are unused */
    wake.time.tm_wday = -1;
    wake.time.tm_yday = -1;
    wake.time.tm_isdst = -1;
    wake.enabled = 1;

    if(ioctl(fd, RTC_WKALM_SET, &wake) < 0) {
	error_errno("Set rtc wake alarm failed");
	return false;
    }
    return true;
}

bool suspend_system()
{
    std::ofstream ofs(SYS_POWER_STATE_PATH);

    if (!ofs) {
	error_errno("Cannot open " SYS_POWER_STATE_PATH);
	return false;
    }

    ofs << "disk\n";
    ofs.flush();
    /* this executes after wake from suspend */
    ofs.close();
    // Drop privileges
    // TODO: be more thorough
    uid_t newuid = getuid(), olduid = geteuid();
    if(setreuid(newuid, newuid) == -1){
	error_plain("Could not drop privileges.");
	return 1;
    }
    std::cout << "Hiii y'alll!!!" << std::endl;
    // launch ringer
    std::vector<std::string> args{afCommon::get_executable_dir() + ringer_exec};
    Glib::spawn_async("",args);
    std::cout << "Execed" << std::endl;
    return true;
}

int main(int argc, char *argv[]){
    if(argc < 2){
	error_plain("Expecting two arguments.");
	return 1;
    }
    if(access(DEFAULT_RTC_DEVICE, F_OK)){
	error_plain("Could not find RTC device");
	return 1;
    }
    std::ifstream ifs(SYS_POWER_STATE_PATH);
    if(!ifs){
	error_errno("Could not determine hibernate capability");
	return 1;
    }
    std::string tempString;
    std::getline(ifs, tempString);
    ifs.close();
    if(!ifs){
	error_errno("Error closing power state file");
	return 1;
    }

    if(tempString.find("disk") == std::string::npos){
	error_plain("Sorry, system does not support hibernation");
	return 1;
    }

    ifs.open(SYS_WAKEUP_PATH);
    if(!ifs){
	error_errno("Sorry, wakeups not enabled");
	return 1;
    }
    std::getline(ifs, tempString);
    if(tempString.compare("enabled") != 0){
	error_plain("Sorry, wakeups not enabled");
	return 1;
    }
    time_t wakeup = std::stoi(argv[1]);
    int fd = open(DEFAULT_RTC_DEVICE, O_RDONLY | O_CLOEXEC);
    if(fd < 0){
	error_errno("Could not open RTC device.");
	return 1;
    }
    if(!setup_alarm(fd, &wakeup)){
	return 1;
    }
    sync();
    if(!suspend_system()) return 1;
    return 0;
}
