#include "aflaunch.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

#define FILE_DELIM '/'

PathHashList* PathHashList::currentObject = NULL;

bool PathHashList::check_and_add_path(std::string path)
{
	// check absoluteness and that it's not the whole system
	std::string prefixString = "Skipped " + path + ": ";
	if(path.front() != FILE_DELIM){
		fileChooserWindow.notify(prefixString + "not an absolute path.\n");
		return false;
	}
	if(path == "/"){
		fileChooserWindow.notify(prefixString + "please do not gamble away the whole system.\n");
		return false;
	}

	// check if the path and any of its parent directories are included yet
	size_t dashPos = std::string::npos;
	do{
		if( pathHashList.count(path.substr(0,dashPos)) ){
			fileChooserWindow.notify(prefixString + "already listed.\n");
			return false;
		}
	} while( (dashPos = path.find_last_of(FILE_DELIM, dashPos - 1)) != 0);

	// file selector sometimes returns directories ... check for file type manually
	if( stat(path.c_str(), statBuf) != 0 ){
		fileChooserWindow.notify("Error processing " + path + ": " +
				strerror(errno) + "\n");
		return false;
	}
	int permissionMask;
	char const *type;
	switch(statBuf->st_mode & S_IFMT)
	{
		// ordinary file
		case S_IFREG:
			permissionMask = W_OK;
			type = "File";
			break;
	        
		// folder
		case S_IFDIR:
			permissionMask = W_OK | X_OK;
			type = "Folder";
			break;
	        
		default:
			fileChooserWindow.notify(prefixString + "incompatible file type.\n");
			return false;
	        
	}

	if( access(path.c_str(), permissionMask) ){
		fileChooserWindow.notify(prefixString + "no write permission.\n");
		return false;
	}
	// update hash lists and fileView
	if( (permissionMask & X_OK) == 0 ){// ordinary file
		PathHashEntry entry(NULL, statBuf->st_size, fileChooserWindow.insert_entry(type, statBuf->st_size, path));
		std::pair<std::string,PathHashEntry> somePair(path, entry);
		pathHashList.insert(somePair);
		totalSize += statBuf->st_size;
	} else {// directory ... do the file tree walk
		currentTopEntry = new PathHashEntry(new std::unordered_set<std::string>, 0);
		ftw( path.c_str(), &PathHashList::traversal_func, 15 );

		currentTopEntry->rowIter = fileChooserWindow.insert_entry(type, currentTopEntry->totalSize, path);

		std::pair<std::string,PathHashEntry> somePair(path, *currentTopEntry);
		pathHashList.insert(somePair);
	}
	return true;
}

// TODO: this can be made smarter with glibc extensions and reusal of subdirectory size information
int PathHashList::traversal_func(const char *fpath, const struct stat *sb, int nopen){
	// first check permissions
	switch(sb->st_mode & S_IFMT)
	{
		// ordinary file
		case S_IFREG:
			if( access(fpath, W_OK) ) return 0;
			break;
		// folder
		case S_IFDIR:
			if( access(fpath, W_OK | X_OK) ) return 0;
			break;
	        
		default:
			return 0;
	        
	}
	// check if subpath is present already (and delete it if it is)
	if(currentObject->pathHashList.count(fpath)){
		currentObject->remove_path(fpath);
	}
	// add it
	if((sb->st_mode & S_IFMT) == S_IFREG){
		currentObject->currentTopEntry->subfilesPointer->insert(fpath);
		currentObject->currentTopEntry->totalSize += sb->st_size;
		currentObject->totalSize += sb->st_size;
	}
	return 0;
}

// path has to exist, otherwise you'll get an exception
void PathHashList::remove_path(std::string path){
	totalSize -= pathHashList[path].totalSize;
	fileChooserWindow.erase(pathHashList[path].rowIter);
	pathHashList.erase(path);
}

void PathHashList::populate_path_hash_view(std::vector<std::string> fileList){
	int valid = 0, invalid = 0;
	for(auto it = fileList.begin(); it != fileList.end(); it++){
		if(check_and_add_path(*it)) valid++;
		else invalid++;
	}
	fileChooserWindow.notify(std::to_string(valid) + " paths added, "
			+ std::to_string(invalid) + " skipped.\n");
}

bool PathHashList::import_file(const std::string& fileName){
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

PathList& PathList::operator=(const PathList& source){
	pathHashList = source.pathHashList;
	totalSize = source.totalSize;
	return *this;
}
