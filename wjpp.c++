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

void segmentate(std::vector<std::string> files, double segment_length);

int open_input(const std::string file);
void close_input();

int open_output(const std::string file);
void close_output();

int encode_frame(int idx, AVFrame* frame);
int decode_packet(int idx, AVPacket* pkt, AVFrame* frame);

// FFmpeg API global objects
AVFormatContext* inctx;
AVFormatContext* outctx;
AVCodecContext* inavctx[16];
AVCodecContext* outavctx[16];

char errbuf[1024];

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

    segmentate(files, 1000);
}


void segmentate(std::vector<std::string> files, double segment_length) {
    // Registry
    av_register_all();
    avcodec_register_all();

    int ret;
    
    for (auto const &video : files) {
        size_t fileno(0);

        // Open input and output, prepare encoding and decoding
        open_input(video);
        open_output(std::to_string(fileno++)+"_"+video);

        std::cout << "Opened input and output." << std::endl;

        AVFrame* frame = av_frame_alloc();
        AVPacket in_pkt;

        size_t current(0);
        size_t previous(0);
        size_t nframes(0);

        for (auto n = 0; n < inctx->nb_streams; n ++) {
            if (inctx->streams[n]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                nframes = inctx->streams[n]->nb_frames;
                break;
            }
        }
        
        double time_curr(0);
        double time_prev(0);
        
        av_init_packet(&in_pkt);
        while (av_read_frame(inctx, &in_pkt) >= 0) {
            print_percent(current++, previous, nframes);

            // Decode packets until the end of the stream is reached
            if (inavctx[in_pkt.stream_index] != NULL) {
                time_curr = av_frame_get_best_effort_timestamp(frame);
                if ((ret = decode_packet(in_pkt.stream_index, &in_pkt, frame)) < 0) {
                    av_strerror(ret, errbuf, 1024);
                    error(__LINE__ - 2, __FILE__);
                    return;
                }

                // If the elapsed time is larger than the specified length, write the current output file and open the next one
                if (time_curr - time_prev > segment_length) {
                    time_prev = time_curr;
                    close_output();
                    open_output(std::to_string(fileno++)+"_"+video);
                }
            }
            else {
                // Write the final packet out
                if ((ret = av_interleaved_write_frame(outctx, &in_pkt)) < 0) {
                    av_strerror(ret, errbuf, 1024);
                    error(__LINE__ - 2, __FILE__);
                    return;
                }
            }
        }

        for (auto n = 0; n < inctx->nb_streams; n ++) {
            if (inavctx[n]) {
                // Flush decoder
                int frame_finished;
                do {
                    in_pkt.data = NULL;
                    in_pkt.size = 0;
                    if ((ret = decode_packet(n, &in_pkt, frame)) < 0) {
                        av_strerror(ret, errbuf, 1024);
                        error(__LINE__ - 2, __FILE__);
                        return;
                    }
                } while (frame_finished);

                // Flush encoder
                int got_output;
                do {
                    if ((ret = encode_frame(n, NULL)) < 0) {
                        av_strerror(ret, errbuf, 1024);
                        error(__LINE__ - 2, __FILE__);
                        return;
                    }
                } while (got_output);
            }
        }

        av_packet_unref(&in_pkt);

        close_input();
        close_output();

        return;
    }
}

int open_input(const std::string file) {
    int ret;

    if ((ret = avformat_open_input(&inctx, file.c_str(), NULL, NULL))) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    if ((ret = avformat_find_stream_info(inctx, NULL)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    return 0;
}

void close_input() {
    for (auto n = 0; n < inctx->nb_streams; n++) {
        avcodec_close(inavctx[n]);
        avcodec_free_context(&inavctx[n]);
    }
    avformat_close_input(&inctx);
}

int open_output(const std::string file) {
    int ret;

    outctx = avformat_alloc_context();
    outctx->oformat = av_guess_format(NULL, file.c_str(), NULL);
    if ((ret = avio_open2(&outctx->pb, file.c_str(), AVIO_FLAG_WRITE, NULL, NULL)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    // Select video stream from input
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

            // Output some information about the input file
            std::cout << "Input file meta: " << std::endl;
            std::cout << "    Pixel format: " << av_get_pix_fmt_name(inavctx[n]->pix_fmt) << std::endl;
            std::cout << "    Size:         " << inavctx[n]->width << " x " << inavctx[n]->height << std::endl;
            std::cout << "    Time base:    {" << inavctx[n]->time_base.num << ", " << inavctx[n]->time_base.den << "}" << std::endl;
            std::cout << "    Gop size:     " << inavctx[n]->gop_size << std::endl;
            std::cout << "    Bit rate:     " << inavctx[n]->bit_rate << std::endl;

            // Set up video encoder
            AVCodec* encoder = avcodec_find_encoder_by_name("rawvideo");
            AVStream* outstream = avformat_new_stream(outctx, encoder); 

            avcodec_parameters_from_context(outstream->codecpar, inavctx[n]);
            outavctx[n] = avcodec_alloc_context3(encoder);

            // Copy output parameters from output stream to output context
            avcodec_parameters_to_context(outavctx[n], outstream->codecpar);
            outavctx[n]->time_base = inavctx[n]->time_base; // Since AVCodecParameters struct has no time_base member

            if ((ret = avcodec_open2(outavctx[n], encoder, NULL)) < 0) {
                av_strerror(ret, errbuf, 1024);
                error(__LINE__ - 2, __FILE__);
                return ret;
            }
        }
        else if (inc->codec_type == AVMEDIA_TYPE_AUDIO) {
            avformat_new_stream(outctx, inc->codec);
            inavctx[n] = outavctx[n] = NULL;
        }
    }

    if ((ret = avformat_write_header(outctx, NULL)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    return 0;
}

void close_output() {
    // Write output
    av_write_trailer(outctx);

    // Close all the streams
    for (int n = 0; n < outctx->nb_streams; n ++) {
        if (outctx->streams[n]->codec) {
            avcodec_close(outctx->streams[n]->codec);
        }
    }

    // Free the memory
    avformat_free_context(outctx);
}

int encode_frame(int idx, AVFrame* frame) {
    AVPacket out_pkt;
    int ret;

    if ((ret = avcodec_send_frame(outavctx[idx], frame)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    av_init_packet(&out_pkt);
    while (ret >= 0) {
        ret = avcodec_receive_packet(outavctx[idx], &out_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
        else if (ret < 0) {
            av_strerror(ret, errbuf, 1024);
            error(__LINE__ - 2, __FILE__);
            return ret;
        }

        out_pkt.stream_index = idx;
        if ((ret = av_interleaved_write_frame(outctx, &out_pkt)) < 0) {
            av_strerror(ret, errbuf, 1024);
            error(__LINE__ - 2, __FILE__);
            return ret;
        }
    }

    // Free packet
    av_packet_unref(&out_pkt);

    return 0;
}

int decode_packet(int idx, AVPacket* pkt, AVFrame* frame) {
    int ret;

    if ((ret = avcodec_send_packet(inavctx[idx], pkt)) < 0) {
        av_strerror(ret, errbuf, 1024);
        error(__LINE__ - 2, __FILE__);
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(inavctx[idx], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return 0;
        else if (ret < 0) {
            av_strerror(ret, errbuf, 1024);
            error(__LINE__ - 2, __FILE__);
            return ret;
        }

        if ((ret = encode_frame(idx, frame)) < 0) {
            av_strerror(ret, errbuf, 1024);
            error(__LINE__ - 2, __FILE__);
            return ret;
        }
    }

}


