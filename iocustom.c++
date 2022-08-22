/* iocustom.c++
 *
 * William Miller
 * Oct 14 2020
 *
 * Custom Input/Output functions for controlling progress printing, datetime conversion and OpenCV typing
 *
 */

#include "iocustom.h"
#include <vector>

static std::vector<int> r = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 249, 244, 239, 234, 229, 224, 
										219, 214, 209, 204, 198, 193, 188, 183, 178, 173, 168, 163, 158, 153, 
										147, 142, 137, 132, 127, 122, 117, 112, 107, 102, 96, 91, 86, 81, 76, 
										71, 66, 61, 56, 51, 45, 40, 35, 30, 25, 20, 15, 10, 5, 0, 0};

static std::vector<int> g = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 51, 56, 61, 66, 71, 76, 81, 86, 
										91, 96, 102, 107, 112, 117, 122, 127, 132, 137, 142, 147, 153, 158, 
										163, 168, 173, 178, 183, 188, 193, 198, 204, 209, 214, 219, 224, 229, 
										234, 239, 244, 249, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
										255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

static std::vector<int> b = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
										0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
										0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
										0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
										0, 0, 0, 0, 0, 0, 0, 0, 0};

void print_percent(size_t current, size_t &previous, size_t total) {
    if ((double)(current - previous) / (double)total < 0.01 && current < total - 1) return;
    else previous = current;

	double percent = ((double)current / (double)(total - 1))*100.0;
	int idx = (unsigned int)percent - 1 >= 0 ? (unsigned int)percent : 0;
	if (total == 1 || current > total || percent > 100) return;

	std::string p = std::to_string(int(percent));
	std::cout << "\r\033[38;2;"+std::to_string(r[idx])+";"+std::to_string(g[idx])+";"+std::to_string(b[idx])+"m"+
	             std::string(3 - p.length(), '0')+p+"% "+res << std::flush;
	if (percent >= 100) {
		std::cout << res+"\n" << std::flush;
	}
}

void error(const int line, const char* file) {
    std::cout << std::flush;
    printf(std::string("\r"+red+white_back+" Error "+res+" \"%s\""+" in File "+yellow+"%s"+res+" on line "+bright+red+"%d"+res+"\n").c_str(), errbuf, file, line);
}

std::string datetime() {
	time_t rawtime;
	struct tm * timeinfo;
	char buffer[80];
	
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	
	strftime(buffer,80,"%m.%d.%Y-%H.%M.%S",timeinfo);
	return std::string(buffer);
}
