#include "pathhashlist.h"
#include "affilechooser.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

const char FILE_DELIM = '/';

PathHashList* PathHashList::currentObject = NULL;

// TODO: this can be made smarter with glibc extensions and reusal of subdirectory size information

// path has to exist, otherwise you'll get an exception
void PathHashList::remove_path(std::string path){
    totalSize -= pathHashList[path].totalSize;
    fileChooserWindow.erase(pathHashList[path].rowIter);
    pathHashList.erase(path);
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
