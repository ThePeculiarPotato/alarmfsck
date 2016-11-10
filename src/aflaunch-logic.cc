#include "aflaunch.h"
#include "common.h"
#include <system_error>
#include <iostream>
#include <fstream>
#include <memory>
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
	if(statBuf->st_mtime > timeStart.to_unix()){
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
