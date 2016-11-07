#include "affilechooser.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

const char FILE_DELIM = '/';

void AlarmFuckFileChooser::check_path_prerequisites(const std::string& filePath){
    // check absoluteness and that it's not the whole system
    if(filePath.front() != FILE_DELIM){
	throw AfUserException("not an absolute path");
    }
    if(filePath == "/"){
	//TODO: also check whether parent directories of the alarmfuck executables are included in path
	throw AfUserException("please do not gamble away the whole system");
    }

    // check if the path is included already
    if( hashMap.count(filePath) ){
	throw AfUserException("already listed");
    }
}

std::pair<bool, off_t> AlarmFuckFileChooser::is_accessible_directory(const std::string& filePath){
    int permissionMask;
    bool isDir = false;
    std::unique_ptr<struct stat> statBuf{new struct stat};
    if(stat(filePath.c_str(), statBuf.get()) != 0)
	throw AfSystemException("Error processing " + filePath);
    switch(statBuf->st_mode & S_IFMT)
    {
	// ordinary file
	case S_IFREG:
	    permissionMask = W_OK;
	    break;

	    // folder
	case S_IFDIR:
	    permissionMask = W_OK | X_OK;
	    isDir = true;
	    break;

	default:
	    throw AfUserException("incompatible file type");
    }

    if( access(filePath.c_str(), permissionMask) != 0 ){
	throw AfUserException("no write permission.");
    }
    return std::pair<bool,size_t>(isDir, statBuf->st_size);
}


bool AlarmFuckFileChooser::check_and_add_path(const std::string& filePath)
{
    bool isDir = false;
    off_t size = 0;
    try{
	check_path_prerequisites(filePath);
	auto resPair = is_accessible_directory(filePath);
	isDir = resPair.first;
	size = resPair.second;
    } catch (AfUserException& exc) {
	notify("Skipped " + filePath + ": " + exc.what() + ".\n");
	return false;
    } catch (AfSystemException& exc) {
	notify(exc.get_message() + ": " + exc.what() + ".\n");
	return false;
    }

    // update hash lists and fileView
    if(!isDir) insert_checked_entry(filePath, size);
    else // directory ... do the file tree walk
	nftw(filePath.c_str(), &AlarmFuckFileChooser::traversal_func, 15, FTW_ACTIONRETVAL);
    return true;
}

std::string parent_directory(const std::string& filepath){
    // TODO: check paths are not terminated with slashes
    return filepath.substr(0, filepath.rfind(FILE_DELIM));
}

int AlarmFuckFileChooser::traversal_func(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
    // first check permissions
    switch(typeflag)
    {
	// ordinary file
	case FTW_F:
	    if(access(fpath, W_OK)) return FTW_CONTINUE;
	    break;
	// folder
	case FTW_D:
	    if(access(fpath, W_OK | X_OK)) return FTW_SKIP_SUBTREE;
	    break;
	default:
	    return FTW_SKIP_SUBTREE;
    }
    AlarmFuckFileChooser * const currentObject = get_set_first_obj_ptr(nullptr);
    auto& hashMap = currentObject->hashMap;
    // base directory action
    if(ftwbuf->level == 0){
	if(hashMap.count(std::string(fpath)))
	    // something went wrong. exit
	    return FTW_STOP;
	currentObject->insert_checked_entry(fpath, sb->st_size);
	return FTW_CONTINUE;
    }
    // lower subdirectories
    // check if subpath is present already (and delete it if it is)
    // TODO: this is terribly wasteful, think about subclassing TreeModel
    try {
	currentObject->filenameTreeStore->erase(hashMap.at(fpath));
	hashMap.erase(fpath);
    } catch (std::out_of_range) {}
    currentObject->insert_checked_entry(fpath, sb->st_size, true);
    return FTW_CONTINUE;
}

void AlarmFuckFileChooser::insert_checked_entry(const std::string& filePath, off_t size, const bool is_child){
    if(is_child){
	Gtk::TreeModel::iterator& parentRow = hashMap[parent_directory(filePath)];
	const Gtk::TreeModel::iterator& row = filenameTreeStore->append(parentRow->children());
	populate_row(row, filePath, size);
    } else {
	const Gtk::TreeModel::iterator& row = filenameTreeStore->append();
	populate_row(row, filePath, size);
    }
}

void AlarmFuckFileChooser::populate_row(const Gtk::TreeModel::iterator& row, const std::string& filePath, off_t size){
    (*row)[fileViewColumnRecord.sizeCol] = size;
    (*row)[fileViewColumnRecord.nameCol] = filePath;
    // TODO: what is the validity of these iterators later
    // I think it's fine, gtk website seems to say so
    std::pair<std::string,Gtk::TreeStore::iterator> somePair{filePath, row};
    hashMap.insert(somePair);
    totalSize += size;
}

bool AlarmFuckFileChooser::import_file(const std::string& fileName){
    // TODO
    if(access(fileName.c_str(), R_OK) == -1) return false;
    std::ifstream ifs(fileName);
    if(!ifs){
	std::cout << "Could not open " << fileName << ": " << strerror(errno) << std::endl;
	return false;
    }
    // count no. of lines
    int noOfLines = 0;
    while(!ifs.eof()){
	if(ifs.get() == '\n') noOfLines++;
	// TODO: figure out why EOF adds an extra newline
    }
    ifs.clear(std::ios_base::eofbit);
    std::cout << "Reading " << noOfLines << " lines from " << fileName << std::endl;
    // make fileList vector
    std::vector<std::string> fileList;
    std::string tempString;
    fileList.reserve(noOfLines);
    // go back to beginning, read everything
    ifs.seekg(0);
    for(auto i=0; i<noOfLines; i++){
	std::getline(ifs, tempString);
	std::cout << "Read: " << tempString << std::endl;
	fileList.push_back(tempString);
    }
    // populate hashList and fileView
    ifs.close();
    populate_path_hash_view(fileList);
    return true;
}

