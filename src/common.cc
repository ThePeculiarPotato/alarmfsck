#include "common.h"
#include <vector>
extern "C" {
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <sys/types.h>
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

const int kB_remainder_mask = 1024 - 1;
const int kB_shift = 10;
const std::vector<std::string> binary_units = {" B", " kB", " MB", " GB"};

std::string humanize_byte_count(const off_t& byte_count){
    /* converts a raw byte count into the units of B, kB, MB, etc.. The
    prefixes k, M, G, etc., are binary, i.e., 1 kB = 2^10 B = 1024 B. The
    following format has been decided upon: the amount is expressed in terms of
    the largest possible binary prefix such that the integer part is non-zero.
    Given the expected use of the utility, the available prefixes go up to G.
    If the resulting integer part has one or two digits, the entire amount is
    rounded to one decimal place. If the rounded fractional part equals zero,
    the ".0" suffix is not shown. This is also not shown if the integer part
    has three or more digits. */
    // this attempts to do some fun bitwise computations
    off_t currValue_t = byte_count, nextQuotient;
    int prevRemainder = 0;
    int i;
    for(i = 0; i < 3 && (nextQuotient = currValue_t >> kB_shift) != 0; ++i){
	prevRemainder = currValue_t & kB_remainder_mask;
	currValue_t = nextQuotient;
    }
    int currValue = (int) currValue_t;
    int twoDecimalDigits = (100 * prevRemainder) / 1024;
    if(currValue / 100 != 0) {
	if (twoDecimalDigits < 50)
	    return std::to_string(currValue) + binary_units[i];
	return std::to_string(currValue + 1) + binary_units[i];
    }
    if(twoDecimalDigits < 5) return std::to_string(currValue) + binary_units[i];
    if(twoDecimalDigits >= 95) return std::to_string(currValue + 1) + binary_units[i];
    return std::to_string(currValue) + "." + std::to_string((twoDecimalDigits + 5) / 10) + binary_units[i];
}

}
