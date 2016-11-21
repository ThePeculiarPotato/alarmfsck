#include "aflaunch.h"
#include "common.h"
#include <system_error>
#include <iostream>
#include <fstream>
#include <memory>
#include <regex>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
}
#include <cryptopp/files.h>
#include <cryptopp/gzip.h>

const size_t maxSize = 200;

std::string format_time_point(const std::tm& timePoint_tm, const std::string& format){
    char *strBuf = new char[maxSize];
    std::strftime(strBuf, maxSize, format.c_str(), &timePoint_tm);
    std::string formattedTime(strBuf);
    delete[] strBuf;
    return formattedTime;
}

std::string format_time_point(const AlarmFsckLauncher::af_time_point& timePoint, const std::string& format){
    return format_time_point(time_point_to_tm(timePoint), format);
}

std::tm time_point_to_tm(const AlarmFsckLauncher::af_time_point& timePoint){
    std::time_t timePoint_t = std::chrono::system_clock::to_time_t(timePoint);
    return *(std::localtime(&timePoint_t));
}

void AlarmFsckLauncher::check_hostage_file(){
    try {
	fileChooser.import_file(hostageFilePath);
	hostageCheckBox.set_active();
	hostageCheckBox.toggled();
    } catch (std::exception) {
	// no error, just skip it
	std::cout << "No appropriate hostages file found.";
    }
}

void AlarmFsckLauncher::write_or_update_hostage_list_file(){
    // Write hostage file, don't update it if it wasn't modified after importing it
    if(!fileChooser.is_updated() && access(hostageFilePath.c_str(), F_OK) == 0){
	struct stat * statBuf = new struct stat;
	if(stat(hostageFilePath.c_str(), statBuf) != 0){
	    throw AfSystemException("Could not open existing " + hostage_file);
	    return;
	}
	if(statBuf->st_mtime > (std::chrono::system_clock::to_time_t(timeStart))){
	    write_hostage_list_file();
	}
    } else {
	write_hostage_list_file();
    }
}

void AlarmFsckLauncher::write_hostage_list_file(){
    progressBar.set_text("Writing hostage list to disk.");
    std::ofstream ofs{hostageFilePath};
    if(!ofs){
	throw AfSystemException("Could not open " + hostage_file);
    }
    const auto& top_paths = fileChooser.get_top_paths();
    // loop over path_map and write out top paths
    for(std::string path : top_paths){
	ofs << path << std::endl;
    }
    ofs.close();
}

void AlarmFsckLauncher::add_path_to_archive(TAR * tarStrucPtr, const std::string& filename){
    const char *fpath = filename.c_str();
    if(tar_append_file(tarStrucPtr, fpath, fpath) == -1){
	throw AfSystemException("Error adding " + filename + " to archive.");
    }
}

void AlarmFsckLauncher::write_hostage_archive(){
    double currentSize = 0, totalSize = fileChooser.get_total_size();
    TAR *tarStrucPtr = new TAR;
    // open - the TAR_GNU option is necessary for files with long names
    if(tar_open(&tarStrucPtr, archivePath.c_str(), NULL, O_WRONLY | O_CREAT | O_TRUNC, 0755, TAR_GNU) == -1){
	throw AfSystemException("Error opening " + hostage_archive);
    }
    fileChooser.for_each_file([&](Gtk::TreeModel::iterator row){
	if (row->children().size()) return false;
	add_path_to_archive(tarStrucPtr,(*row)[fileChooser.get_column_record().nameCol]);
	currentSize += (*row)[fileChooser.get_column_record().sizeCol];
	progressBar.set_fraction(currentSize/totalSize);
	return false;
    });
    if(tar_append_eof(tarStrucPtr) == -1){
	tar_close(tarStrucPtr);
	throw AfSystemException("Error finalizing " + hostage_archive);
    }
    tar_close(tarStrucPtr);
}

void AlarmFsckLauncher::erase_original_hostages(){
    // files in the filesystem
    fileChooser.for_each_file([&](Gtk::TreeModel::iterator row){
	if (row->children().size()) return false;
	afCommon::erase_file((*row)[fileChooser.get_column_record().nameCol]);
	return false;
    });
    // uncompressed archive
    afCommon::erase_file(archivePath);
}

void AlarmFsckLauncher::write_compressed_hostage_archive(){
    CryptoPP::FileSource fs(archivePath.c_str(), true,
	    new CryptoPP::Gzip(
		new CryptoPP::FileSink(compressedPath.c_str())
		)
	    );
}

bool AlarmFsckLauncher::check_time_entry(){
    Gtk::TreeModel::iterator iter = inAtComboBox.get_active();
    switch((*iter)[inAtColumnRecord.idCol]){
	case 1:
	    {
		return check_interval_entry();
		break;
	    }
	case 2:
	    {
		return check_datetime_entry();
		break;
	    }
    }
    return true;
}

bool AlarmFsckLauncher::check_interval_entry(){
    double interval;
    try{ interval = stod(timeIntervalEntry.get_text().raw()); }
    catch(std::exception& e){
	progressBar.set_text("Invalid time interval.");
	return false;
    }
    if(interval <= 0){
	progressBar.set_text("Wakeup time must be in the future.");
	return false;
    }
    int modifier;
    Gtk::TreeModel::iterator iter2 = timeUnitComboBox.get_active();
    switch((*iter2)[timeUnitColumnRecord.idCol]){
	case 1:
	    modifier = 1;
	    break;
	case 2:
	    modifier = 60;
	    break;
	case 3:
	    modifier = 3600;
	    break;
    }
    timeWakeup = std::chrono::system_clock::now() +
	std::chrono::duration<int>((int) (interval * modifier));
    return true;
}

bool AlarmFsckLauncher::check_datetime_entry(){
    using namespace std::chrono;
    af_time_point timeNow = system_clock::now();
    // this will be filled with the human-readable date&time data entered by
    // the user
    std::tm wakeup_tm;

    // check entered time format
    std::regex timeRegex("^(\\d{2}):(\\d{2}):(\\d{2})$");
    std::smatch timeMatch;
    const std::string timeEntryText = timeEntry.get_text();
    if(!std::regex_match(timeEntryText, timeMatch, timeRegex)){
	progressBar.set_text("Invalid time format.");
	return false;
    }

    // check entered date format, allowing for the special values "today" and
    // "tomorrow". Also populate the date portion of wakeup_tm.
    std::regex dateRegex("^(\\d{2})/(\\d{2})/(\\d{4})$");
    std::smatch dateMatch;
    const std::string dateEntryText = dateEntry.get_text();
    if(!std::regex_match(dateEntryText, dateMatch, dateRegex)){
	// TODO: trim the strings
	if(dateEntryText == "today"){
	    wakeup_tm = time_point_to_tm(timeNow);
	} else if (dateEntryText == "tomorrow"){
	    wakeup_tm = time_point_to_tm(timeNow + hours(24));
	} else {
	    progressBar.set_text("Invalid date format.");
	    return false;
	}
    } else {
	// the subtractions are on account of std::tm's definition
	wakeup_tm.tm_year = std::stoi(dateMatch[3]) - 1900;
	wakeup_tm.tm_mon = std::stoi(dateMatch[2]) - 1;
	wakeup_tm.tm_mday  = std::stoi(dateMatch[1]);
    }

    // populate the time portion of wakeup_tm
    wakeup_tm.tm_hour = std::stoi(timeMatch[1]);
    wakeup_tm.tm_min = std::stoi(timeMatch[2]);
    wakeup_tm.tm_sec = std::stod(timeMatch[3]);

    // convert to seconds since epoch, do sanity checks
    std::time_t wakeup_t = std::mktime(&wakeup_tm);
    if(wakeup_t == -1){
	progressBar.set_text("Not a real point in time.");
	return false;
    }
    af_time_point potentialWakeup = system_clock::from_time_t(wakeup_t);
    if(potentialWakeup <= timeNow){
	progressBar.set_text("Wakeup time must be in the future.");
	return false;
    }
    timeWakeup = potentialWakeup;
    // std::mktime accepts and converts dates like 32nd August, so must update
    // text entries again
    timeEntry.set_text(format_time_point(timeWakeup, "%T"));
    dateEntry.set_text(format_time_point(timeWakeup, "%d/%m/%Y"));
    return true;
}

const std::string DEFAULT_RTC		= "rtc0";
const std::string DEFAULT_RTC_DEVICE	= "/dev/" + DEFAULT_RTC;
const std::string SYS_WAKEUP_PATH	= "/sys/class/rtc/" + DEFAULT_RTC + "/device/power/wakeup";
const std::string SYS_POWER_STATE_PATH	= "/sys/power/state";

void AlarmFsckLauncher::perform_rtc_check(){
    if(access(DEFAULT_RTC_DEVICE.c_str(), F_OK)){
	throw AfUserException("Could not find RTC device");
    }
    std::ifstream ifs(SYS_POWER_STATE_PATH);
    if(!ifs){
	throw AfSystemException("Could not determine hibernate capability");
    }
    std::string tempString;
    std::getline(ifs, tempString);
    ifs.close();
    if(!ifs){
	throw AfSystemException("Error closing power state file");
    }

    if(tempString.find("disk") == std::string::npos){
	throw AfUserException("Sorry, system does not support hibernation");
    }

    ifs.open(SYS_WAKEUP_PATH);
    if(!ifs){
	throw AfSystemException("Sorry, wakeups not enabled");
    }
    std::getline(ifs, tempString);
    if(tempString.compare("enabled") != 0){
	throw AfUserException("Sorry, wakeups not enabled");
    }
}
