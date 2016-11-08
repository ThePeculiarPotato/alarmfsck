#include "aflaunch.h"
#include "common.h"
#include <system_error>
#include <iostream>
#include <fstream>
#include <memory>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
}

std::string get_executable_dir(){
    char exePath[PATH_MAX];
    ssize_t exePathSize = readlink("/proc/self/exe", exePath, PATH_MAX);
    if(exePathSize == -1) throw AfSystemException("Cannot determine executable path");
    exePath[exePathSize] = '\0';
    return std::string(dirname(exePath)) + file_delim;
}

std::string cpp_realpath(const std::string& raw_path){
    char *pathCand = realpath(raw_path.c_str(), NULL);
    if(pathCand == NULL) throw AfSystemException("Cannot canonicalize path");
    return std::string(pathCand) + file_delim;
}

void AlarmFuckLauncher::check_hostage_file(){
    try {
	fileChooser.import_file(fullHostageFilePath);
	hostageCheckBox.set_active();
	hostageCheckBox.toggled();
    } catch (std::exception) {
	//TODO handle
    }
}

void AlarmFuckLauncher::write_or_update_hostage_list_file(){
    // Write hostage file, don't update it if it wasn't modified after importing it
    if(!fileChooser.is_updated() && access(fullHostageFilePath.c_str(), F_OK) == 0){
	struct stat * statBuf = new struct stat;
	if(stat(fullHostageFilePath.c_str(), statBuf) != 0){
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

void AlarmFuckLauncher::write_hostage_list_file(){
    progressBar.set_text("Writing hostage list to disk.");
    std::ofstream ofs{fullHostageFilePath};
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

void AlarmFuckLauncher::add_path_to_archive(TAR * tarStrucPtr, const std::string& filename){
    const char *fpath = filename.c_str();
    if(tar_append_file(tarStrucPtr, fpath, fpath) == -1){
	throw AfSystemException("Error adding " + filename + " to archive.");
    }
}

void AlarmFuckLauncher::erase_file(const std::string& filename){
    // TODO: for now this just rm's a file, later add an option to shred it
    if(std::remove(filename.c_str()) == -1){
	throw AfSystemException("Error removing " + filename);
    }
}

void AlarmFuckLauncher::write_hostage_archive(){
    double currentSize = 0, totalSize = fileChooser.get_total_size();
    TAR *tarStrucPtr = new TAR;
    // TODO: if any of these archives exists before it has to be shredded
    // open - the TAR_GNU option is necessary for files with long names
    std::string fullHostageArchivePath = baseDir + data_dir + hostage_archive;
    if(tar_open(&tarStrucPtr, fullHostageArchivePath.c_str(), NULL, O_WRONLY | O_CREAT | O_TRUNC, 0755, TAR_GNU) == -1){
	throw AfSystemException("Error opening " + hostage_archive);
    }
    fileChooser.for_each_file([&](Gtk::TreeModel::iterator row){
	if (row->children().size()) return false;
	add_path_to_archive(tarStrucPtr,(*row)[fileChooser.get_column_record().nameCol]);
	currentSize += (*row)[fileChooser.get_column_record().sizeCol];
	progressBar.set_fraction(currentSize/totalSize);
	return false;
    });
    //auto& path_map = userPathHashList.pathHashList;
    //// double loop over path_map
    //for(auto it = path_map.begin(); it != path_map.end(); it++){
    //    // ordinary file
    //    if(it->second.subfilesPointer == NULL){
    //        add_path_to_archive(tarGoodPtr,it->first);
    //    }
    //    // directory
    //    else{
    //        std::unordered_set<std::string>& subPaths = *(it->second.subfilesPointer);
    //        for(auto it2 = subPaths.begin(); it2 != subPaths.end(); it2++){
    //    	add_path_to_archive(tarGoodPtr,*it2);
    //        }
    //    }
    //    // update progress bar
    //    currentSize += it->second.totalSize;
    //    progressBar.set_fraction(currentSize/totalSize);
    //}
    // finalize archive
    if(tar_append_eof(tarStrucPtr) == -1){
	tar_close(tarStrucPtr);
	throw AfSystemException("Error finalizing " + hostage_archive);
	// progressBar.set_fraction(0.0);
    }
    // close and finish
    tar_close(tarStrucPtr);
}

void AlarmFuckLauncher::erase_original_hostages(){
    //std::unordered_map<std::string,PathHashEntry>& path_map = userPathHashList.pathHashList;
    // double loop over path_map
    fileChooser.for_each_file([&](Gtk::TreeModel::iterator row){
	if (row->children().size()) return false;
	erase_file((*row)[fileChooser.get_column_record().nameCol]);
	return false;
    });
    // TODO: general thingy
    //for(auto it = path_map.begin(); it != path_map.end(); it++){
    //    // directory
    //    if(it->second.subfilesPointer != NULL){
    //        std::unordered_set<std::string>& subPaths = *(it->second.subfilesPointer);
    //        for(auto it2 = subPaths.begin(); it2 != subPaths.end(); it2++){
    //    	erase_file(*it2);
    //        }
    //    }
    //    erase_file(it->first);
    //}
    //erase_file(baseDir + data_dir + hostage_archive);
}

void AlarmFuckLauncher::write_compressed_hostage_archive(){
    // TODO: add error checking
    namespace io = boost::iostreams;
    std::ofstream fileOut(baseDir + data_dir + hostage_compressed, std::ios_base::out | std::ios_base::binary);
    io::filtering_streambuf<io::output> out;
    out.push(io::gzip_compressor());
    out.push(fileOut);
    std::ifstream fileIn(baseDir + data_dir + hostage_archive, std::ios_base::in | std::ios_base::binary);
    io::copy(fileIn, out);
    out.pop();
    fileIn.close();
}

const std::string DEFAULT_RTC		= "rtc0";
const std::string DEFAULT_RTC_DEVICE	= "/dev/" + DEFAULT_RTC;
const std::string SYS_WAKEUP_PATH	= "/sys/class/rtc/" + DEFAULT_RTC + "/device/power/wakeup";
const std::string SYS_POWER_STATE_PATH	= "/sys/power/state";

bool AlarmFuckLauncher::perform_rtc_check(){
    //TODO:: throw simpler exceptions in this case
    if(access(DEFAULT_RTC_DEVICE.c_str(), F_OK)){
	error_to_user("Could not find RTC device");
	return false;
    }
    std::ifstream ifs(SYS_POWER_STATE_PATH);
    if(!ifs){
	error_to_user("Could not determine hibernate capability");
	return false;
    }
    std::string tempString;
    std::getline(ifs, tempString);
    ifs.close();
    if(!ifs){
	error_to_user("Error closing power state file");
	return false;
    }

    if(tempString.find("disk") == std::string::npos){
	error_to_user("Sorry, system does not support hibernation");
	return false;
    }

    ifs.open(SYS_WAKEUP_PATH);
    if(!ifs){
	error_to_user("Sorry, wakeups not enabled");
	return false;
    }
    std::getline(ifs, tempString);
    if(tempString.compare("enabled") != 0){
	error_to_user("Sorry, wakeups not enabled");
	return false;
    }
    return true;
}
