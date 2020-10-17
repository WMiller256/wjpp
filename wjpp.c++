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

#include <experimental/filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// libav 
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

// Boost
#include <boost/program_options.hpp>

#include "colors.h"

namespace po = boost::program_options;
namespace fs = std::experimental::filesystem;

void Segment(std::vector<std::string> files);

int main(int argn, char** argv) {
    std::vector<std::string> files;

   	po::options_description description("Usage");
	try {
		description.add_options()
			("videos,v", po::value<std::vector<std::string> >()->multitoken(), "The videos on which to perform the preprocessing. (Required)")
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

	if (vm.count("videos")) files = vm["videos"].as<std::vector<std::string> >();
	else {
		std::cout << description << std::endl;
		exit(2);
	}

    Segment(files);
}


void Segment(std::vector<std::string> files) {
    // Allocation and registry
    frame = av_frame_alloc();
    decframe = av_frame_alloc();
    av_register_all();
    avcodec_register_all();
    
    for (auto const &video : files) {
        if ((ret = avformat_open_input(&informat_ctx, video.c_str(), NULL, NULL)) < 0) {
        	// If opening fails print error message and exit
        	char errbuf[1024];
        	av_strerror(ret, errbuf, 1024);
            std::cout << "Could not open file "+yellow << video << res+": " << errbuf << std::endl;
            exit(-1);
        }

        // Read the meta data 
        ret = avformat_find_stream_info(informat_ctx, 0);
        if (ret < 0) {
            std::cout << red << "Failed to read input file information. " << res << std::endl;
            exit(-1);
        }

        for(int ii = 0; ii < informat_ctx->nb_streams; ii ++) {
            if (informat_ctx->streams[ii]->codec->codec_type == AVMEDIA_TYPE_VIDEO && video_stream < 0) {
                video_stream = ii;
            }
        }
        if (video_stream == -1) {
            std::cout << "Could not find stream index." << std::endl;
            exit(-1);
        }

        instream = informat_ctx->streams[video_stream];
        in_codec = avcodec_find_decoder(instream->codec->codec_id);
        if (in_codec == NULL) {
            std::cout << "Could not find codec: " << avcodec_get_name(instream->codec->codec_id) << std::endl;
            exit(1);
        }
        else std::cout << "Detected codec: " << avcodec_get_name(instream->codec->codec_id) << std::endl;
        std::cout << "File format:  " << informat_ctx->iformat->name << std::endl;
        std::cout << "Pixel format: " << av_get_pix_fmt_name(instream->codec->pix_fmt) << std::endl;
        inav_ctx = instream->codec;

        // Open the input codec
        avcodec_open2(inav_ctx, in_codec, NULL);

        // Allocate destination frame buffer
        std::vector<uint8_t> framebuf(avpicture_get_size(inav_ctx->pix_fmt, inav_ctx->width, inav_ctx->height));
        avpicture_fill(reinterpret_cast<AVPicture*>(frame), framebuf.data(), AV_PIX_FMT_BGR24, instream->codec->width, instream->codec->height);
                       
        SwsContext* swsctx = sws_getContext(inav_ctx->width, inav_ctx->height, instream->codec->pix_fmt, inav_ctx->width, 
                                            inav_ctx->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
        size_t nframes = instream->nb_frames;
        frames.reserve(nframes);    // Minimize memcpy
        
        size_t current(0);
        size_t previous(0);

        // Packet initialization
        AVPacket pkt;
        av_init_packet(&pkt);

        // Parsing loop
        while (ret == 0) {

            // Print progress
            print_percent(current++, previous, nframes);

            ret = av_read_frame(informat_ctx, &pkt);
            avcodec_decode_video2(instream->codec, decframe, &valid_frame, &pkt);

            // Ignore invalid frames
            if (!valid_frame) continue;

            // Frame extraction
            if (ret == 0) {                                              
                sws_scale(swsctx, decframe->data, decframe->linesize, 0, decframe->height, frame->data, frame->linesize);

                // Convert the decoded frame into a cv::Mat
                cv::Mat _frame(decframe->height, decframe->width, CV_8UC3, framebuf.data());
                frames.push_back(_frame.clone());   // Have to use .clone() otherwise each element in [frames] will reference the same object
            }
        }
        print_percent(nframes-1, nframes);

        // Free memory and close streams
        av_frame_free(&decframe);
        av_frame_free(&frame);
        sws_freeContext(swsctx);
        avcodec_close(inav_ctx);
        avformat_close_input(&informat_ctx);
    }
}
