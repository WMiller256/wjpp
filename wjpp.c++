/*
 * wjpp.c++
 *
 * William Miller
 * Sep 19, 2020
 *
 *
 * Main implementation file for  WinJupos PreProcessor (WJPP)
 *
 *
 */

#include <chrono>
#include <ctime>
#include <experimental/filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// libav 
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

// Boost
#include <boost/program_options.hpp>

#include "colors.h"
#include "iocustom.h"

namespace po = boost::program_options;
namespace fs = std::experimental::filesystem;

const std::vector<std::string> targets = {"sun", "mercury", "venus", "earth", "moon", "mars", "saturn", "jupiter", "uranus", "neptune"};
const std::string wjformat = "%Y-%m-%d-%H%M_%S";

std::vector<std::time_t> parse_dates(std::vector<std::string> files, const std::string &dtformat, bool utc);
void segmentate(std::vector<std::string> files, std::vector<std::time_t> starttimes, double duration);

double get_duration(std::string file);
std::string outname(const std::time_t starttime, const std::string file, const std::string obs="UI20");

char errbuf[1024];

int main(int argn, char** argv) {
    std::vector<std::string> files;
    double duration;
    std::string dtformat;
    bool utc;

   	po::options_description description("Usage");
	try {
		description.add_options()
			("inputs,i", po::value<std::vector<std::string> >()->multitoken(), "The videos on which to perform the preprocessing. (Required)")
			("duration,d", po::value<double>(&duration)->required(), "The desired segment duration in seconds. (Required)")
			("dtformat,f", po::value<std::string>(&dtformat)->default_value("%Y%m%d_%H%M%S"), "The date-time format in the filename, i.e. "
			                                                               "%Y-%m-%d_%H:%M:%S for YYYY-mm-dd_HH:MM:SS. Assumed to be the same"
			                                                               " for ALL input files. (Optional, defaults to Firecapture style)")
			("utc", po::value<bool>(&utc)->default_value(true), "Whether to consider input timestamps as UTC or not. (Optional, defaults to true)")
		;
	}
	catch (...) {
		std::cout << "Error in boost program options initialization" << std::endl;
		exit(0);
	}

  	po::variables_map vm;
    try {
    	po::store(po::command_line_parser(argn, argv).options(description).run(), vm);
    	po::notify(vm);
    }
    catch (...) {
        std::cout << description << std::endl;
        exit(1);
    }

	if (vm.count("inputs")) files = vm["inputs"].as<std::vector<std::string> >();
	else {
		std::cout << description << std::endl;
		exit(2);
	}

    std::vector<std::time_t> starttimes = parse_dates(files, dtformat, utc);
    segmentate(files, starttimes, duration);
}

void segmentate(std::vector<std::string> files, std::vector<std::time_t> starttimes, double duration) {    
    for (unsigned int ii = 0; ii < files.size(); ii ++) {        
        if (!fs::exists(fs::path(files[ii]))) {
            std::cout << "File "+yellow+files[ii]+res+" does not exist.\n" << std::flush;
            return;
        }

        std::cout << yellow+files[ii]+res << std::endl;
        
        float dur = get_duration(files[ii]);
        float start = 0;
        std::string command;

        while (dur >= 0) {
            float t = duration < dur ? duration : dur;
            command += "ffmpeg -y -ss "+std::to_string(start)+" -i "+files[ii]+" -c copy -t "+std::to_string(t)+" "+
                outname(starttimes[ii] + (size_t)start, files[ii])+"; ";
            dur -= duration;
            start += duration;
        }
        int ret = std::system(command.c_str());
    }
}

std::vector<std::time_t> parse_dates(std::vector<std::string> files, const std::string &dtformat, bool utc) {
    std::vector<std::time_t> dates;
    for (auto const &_f : files) {
        std::string f = fs::path(_f).filename().string();
        size_t pos;
        if ((pos = f.find_first_of("0123456789")) != std::string::npos) {
            std::string sub = f.substr(pos, f.find_first_not_of("0123456789-_") - pos + 1);
            std::istringstream ss(sub.substr(0, sub.find_last_of("0123456789") + 1));
            std::tm t = {};
            ss >> std::get_time(&t, dtformat.c_str());
            if (ss.fail()) {
                std::string err("Failed to extract datetime from "+yellow+f+res+" with format "+cyan+dtformat+res+".");
                std::copy(std::begin(err), std::end(err), std::begin(errbuf));
                error(__LINE__ - 4, __FILE__);
                exit(-1);
            }
            else dates.push_back(utc ? timegm(&t) : mktime(&t));
        }
    }

    return dates;
}

// Contexts 
// Why these have to be globals is entirely beyond me, 
// but evidently they do: seg fault if they are localized
AVFormatContext* inctx;
AVCodecContext* inavctx[16];
double get_duration(std::string file) {

    // Registry
    av_register_all();
    avcodec_register_all();

    double time_base_dec;

    int ret;
    float dur = 0;

    if ((ret = avformat_open_input(&inctx, file.c_str(), NULL, NULL))) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        exit(ret);
    }

    if ((ret = avformat_find_stream_info(inctx, NULL)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        exit(ret);
    }

    for (auto n = 0; n < inctx->nb_streams; n ++) {
        AVStream* instream = inctx->streams[n];
        AVCodecContext* inc = instream->codec;
        if (inc->codec_type == AVMEDIA_TYPE_VIDEO) {
            // Set up video decoder
            inavctx[n] = avcodec_alloc_context3(inc->codec);
            avcodec_copy_context(inavctx[n], inc);
            inavctx[n]->time_base = instream->codec->time_base;

            if ((ret = avcodec_open2(inavctx[n], avcodec_find_decoder(inc->codec_id), NULL)) < 0) {
                av_strerror(ret, errbuf, 1024);
                error(__LINE__ - 2, __FILE__);
                return ret;
            }

            dur = (double)inavctx[n]->time_base.num / (double)inavctx[n]->time_base.den * instream->nb_frames;

        }
    }

    // Close input
    for (auto n = 0; n < inctx->nb_streams; n++) {
        avcodec_close(inavctx[n]);
        avcodec_free_context(&inavctx[n]);
    }
    avformat_close_input(&inctx);
    
    return dur;
}

std::string outname(const std::time_t starttime, const std::string file, const std::string obs) {
    char _outname[19];
    std::strftime(_outname, sizeof(_outname), wjformat.c_str(), std::gmtime(&starttime));

    // Round integer seconds to fractional minutes for WinJUPOS compatibility. Worst line of code I've ever written. I hate it.
    _outname[16] = char((int)(int((char)_outname[16] - '0') * 10 + int((char)_outname[17]) - '0') / 6) + '0';
    _outname[17] = '\0';

    size_t idx = file.find_first_not_of("0123456789_-");
    fs::path outpath = fs::path(file.substr(idx, file.length() - idx));
    outpath.replace_filename(std::string(_outname)+"-"+obs+"-"+outpath.filename().string());
    return outpath.string();
}
