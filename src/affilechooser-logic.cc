#include "affilechooser.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

const char FILE_DELIM = '/';

void AlarmFsckFileChooser::check_path_prerequisites(const std::string& filePath){
    // TODO: include file name in messages, remove from higher calling functions
    // check absoluteness and that it's not the whole system
    if(filePath.front() != FILE_DELIM){
	throw AfUserException("not an absolute path");
    }
    if(filePath == "/"){
	//TODO: also check whether parent directories of the alarmfsck executables are included in path
	throw AfUserException("please do not gamble away the whole system");
    }

    // check if the path is included already
    if( hashMap.count(filePath) ){
	throw AfUserException("already listed");
    }
}

std::pair<bool, off_t> AlarmFsckFileChooser::is_accessible_directory(const std::string& filePath){
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


bool AlarmFsckFileChooser::check_and_add_path(const std::string& filePath)
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
	nftw(filePath.c_str(), &AlarmFsckFileChooser::traversal_func, 15, FTW_ACTIONRETVAL);
    return true;
}

std::string parent_directory(const std::string& filepath){
    // TODO: check paths are not terminated with slashes
    return filepath.substr(0, filepath.rfind(FILE_DELIM));
}

int AlarmFsckFileChooser::traversal_func(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
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
    AlarmFsckFileChooser * const currentObject = get_set_first_obj_ptr(nullptr);
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
    // if subpath already present, move its entire subtree to the new location.
    // Skip calling this handler on the corresponding subdirectories.
    if(hashMap.count(fpath)){
	currentObject->relocate_subtree(fpath);
	return FTW_SKIP_SUBTREE;
    } else {
	currentObject->insert_checked_entry(fpath, sb->st_size, true);
	return FTW_CONTINUE;
    }
}

void AlarmFsckFileChooser::relocate_subtree(const std::string& filePath){
    // TODO: somehow enforce calling this only when filePath's parent is in the TreeStore?
    Gtk::TreeModel::iterator source = hashMap[filePath];
    const Gtk::TreeModel::iterator& dest = filenameTreeStore->append(hashMap[parent_directory(filePath)]->children());
    move_subtree(source, dest);
    filenameTreeStore->erase(source);
}

void AlarmFsckFileChooser::insert_checked_entry(const std::string& filePath, off_t size, const bool is_child){
    if(is_child){
	Gtk::TreeModel::iterator& parentRow = hashMap[parent_directory(filePath)];
	const Gtk::TreeModel::iterator& row = filenameTreeStore->append(parentRow->children());
	populate_row(row, filePath, size);
    } else {
	const Gtk::TreeModel::iterator& row = filenameTreeStore->append();
	populate_row(row, filePath, size);
    }
}

void AlarmFsckFileChooser::populate_row(const Gtk::TreeModel::iterator& row, const std::string& filePath, off_t size){
    // TODO: what is the validity of these iterators later
    // I think it's fine, gtk website seems to say so
    (*row)[fileViewColumnRecord.sizeCol] = size;
    (*row)[fileViewColumnRecord.nameCol] = filePath;
    //std::pair<std::string,Gtk::TreeStore::iterator> somePair{filePath, row};
    //hashMap.insert(somePair);
    hashMap[filePath] = row;
    totalSize += size;
}

void AlarmFsckFileChooser::import_file(const std::string& fileName){
    std::ifstream ifs(fileName);
    if(!ifs)
	throw AfSystemException("Error importing file " + fileName + ": ");
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
    for(int i=0; i<noOfLines; i++){
	std::getline(ifs, tempString);
	std::cout << "Read: " << tempString << std::endl;
	fileList.push_back(tempString);
    }
    // populate hashList and fileView
    ifs.close();
    populate_file_view(fileList);
}

void AlarmFsckFileChooser::move_subtree(const Gtk::TreeStore::iterator& source, const Gtk::TreeStore::iterator& dest)
{
    auto& fvcr = fileViewColumnRecord;
    std::string pathName = source->get_value(fvcr.nameCol);
    // first copy the base nodes. Use get/set_value because the [] operator is
    // having trouble when appearing on both sides of an equation
    dest->set_value(fvcr.nameCol, pathName);
    dest->set_value(fvcr.sizeCol, source->get_value(fvcr.sizeCol));
    // and then recursively the children
    auto& sourceChildren = source->children();
    for(auto child = sourceChildren.begin(); child != sourceChildren.end(); child++)
	move_subtree(child, filenameTreeStore->append(dest->children()));
    hashMap[pathName] = dest;
    // could also erase source from the tree but it is equivalent, and more
    // satisfying, to erase the top-level source after the recursive invocation
    // of this method is finished. Don't forget to do that.
}

void AlarmFsckFileChooser::erase_subtree(const Gtk::TreeStore::iterator& top)
{
    auto& children = top->children();
    for(auto child = children.begin(); child != children.end(); child++)
	erase_subtree(child);
    totalSize -= ((*top)[fileViewColumnRecord.sizeCol]);
    hashMap.erase((*top)[fileViewColumnRecord.nameCol]);
    // don't forget to erase top iterator after exiting
}

std::vector<std::string> AlarmFsckFileChooser::get_top_paths() const{
    std::vector<std::string> retList;
    const auto& topLevel = filenameTreeStore->children();
    for(auto node = topLevel.begin(); node != topLevel.end(); node++)
	retList.push_back((*node)[fileViewColumnRecord.nameCol]);
    return retList;
}

// DEBUG functions
void AlarmFsckFileChooser::print_subtree(int level, const Gtk::TreeModel::iterator& row){
    const auto& children = row->children();
    // indentation
    for(int count = level; count > 0; count--)
	std::cout << "\t";
    // print file name
    std::cout << row->get_value(fileViewColumnRecord.nameCol) << std::endl;
    //subdirs
    for(auto child = children.begin(); child != children.end(); child++)
	print_subtree(level + 1, child);
}

void AlarmFsckFileChooser::print_tree(){
    //static int occurrence = 1;
    //std::cout << "print_tree invocation No. " << occurrence << ":\n";
    const auto& children = filenameTreeStore->children();
    for(auto child = children.begin(); child != children.end(); child++)
	print_subtree(0, child);
    std::cout << std::endl;
}
