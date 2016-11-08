#include "common.h"
extern "C" {
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
}

namespace afCommon{

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

void erase_file(const std::string& filename){
    // TODO: for now this just rm's a file, later add an option to shred it
    if(std::remove(filename.c_str()) == -1){
	throw AfSystemException("Error removing " + filename);
    }
}

}
