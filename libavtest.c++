#include <stdio.h>
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

char errbuf[1024];

static AVFormatContext *inctx, *outctx;
#define MAX_STREAMS 16
static AVCodecContext *inavctx[MAX_STREAMS];
static AVCodecContext *outavctx[MAX_STREAMS];

static int openInputFile(const char *file) {
    int ret;

    inctx = NULL;
    ret = avformat_open_input(& inctx, file, NULL, NULL);
    if (ret != 0)
        return ret;
    ret = avformat_find_stream_info(inctx, NULL);
    if (ret < 0)
        return ret;

    return 0;
}

static void closeInputFile(void) {
    int n;

    for (n = 0; n < inctx->nb_streams; n++)
        if (inavctx[n]) {
            avcodec_close(inavctx[n]);
            avcodec_free_context(&inavctx[n]);
        }

    avformat_close_input(&inctx);
}

static int openOutputFile(const char *file) {
    int ret, n;

    outctx = avformat_alloc_context();
    outctx->oformat = av_guess_format(NULL, file, NULL);
    if ((ret = avio_open2(&outctx->pb, file, AVIO_FLAG_WRITE, NULL, NULL)) < 0)
        return ret;

    for (n = 0; n < inctx->nb_streams; n++) {
        AVStream *inst = inctx->streams[n];
        AVCodecContext *inc = inst->codec;

        if (inc->codec_type == AVMEDIA_TYPE_VIDEO) {
            // video decoder
            inavctx[n] = avcodec_alloc_context3(inc->codec);
            avcodec_copy_context(inavctx[n], inc);
            if ((ret = avcodec_open2(inavctx[n], avcodec_find_decoder(inc->codec_id), NULL)) < 0)
                return ret;

            // video encoder
            AVCodec *encoder = avcodec_find_encoder_by_name("rawvideo");
            AVStream *outst = avformat_new_stream(outctx, encoder);
            outst->codec->width = inavctx[n]->width;
            outst->codec->height = inavctx[n]->height;
            outst->codec->pix_fmt = inavctx[n]->pix_fmt;
            outst->time_base = inavctx[n]->time_base;
            
            outavctx[n] = avcodec_alloc_context3(encoder);
            avcodec_copy_context(outavctx[n], outst->codec);
            if ((ret = avcodec_open2(outavctx[n], encoder, NULL)) < 0)
                return ret;
        } else if (inc->codec_type == AVMEDIA_TYPE_AUDIO) {
            avformat_new_stream(outctx, inc->codec);
            inavctx[n] = outavctx[n] = NULL;
        } else {
            fprintf(stderr, "Don't know what to do with stream %d\n", n);
            return -1;
        }
    }

    if ((ret = avformat_write_header(outctx, NULL)) < 0)
        return ret;

    return 0;
}

static void closeOutputFile(void) {
    int n;

    av_write_trailer(outctx);
    for (n = 0; n < outctx->nb_streams; n++)
        if (outctx->streams[n]->codec)
            avcodec_close(outctx->streams[n]->codec);
    avformat_free_context(outctx);
}

static int encodeFrame(int stream_index, AVFrame *frame, int *gotOutput) {
    AVPacket outPacket;
    int ret;

    av_init_packet(&outPacket);
    if ((ret = avcodec_encode_video2(outavctx[stream_index], &outPacket, frame, gotOutput)) < 0) {
        fprintf(stderr, "Failed to encode frame\n");
        return ret;
    }
    if (*gotOutput) {
        outPacket.stream_index = stream_index;
        if ((ret = av_interleaved_write_frame(outctx, &outPacket)) < 0) {
            fprintf(stderr, "Failed to write packet\n");
            return ret;
        }
    }
    av_free_packet(&outPacket);

    return 0;
}

static int decodePacket(int stream_index, AVPacket *pkt, AVFrame *frame, int *frameFinished) {
    int ret;

    if ((ret = avcodec_decode_video2(inavctx[stream_index], frame,
                                     frameFinished, pkt)) < 0) {
        fprintf(stderr, "Failed to decode frame\n");
        return ret;
    }
    if (*frameFinished){
        int hasOutput;

        frame->pts = frame->pkt_pts;
        return encodeFrame(stream_index, frame, &hasOutput);
    } else {
        return 0;
    }
}

int main(int argc, char *argv[]) {
    char *input = argv[1];
    char *output = argv[2];
    int ret, n;

    printf("Converting %s to %s\n", input, output);
    av_register_all();
    if ((ret = openInputFile(input)) < 0) {
        fprintf(stderr, "Failed to open input file %s\n", input);
        return ret;
    }
    if ((ret = openOutputFile(output)) < 0) {
        fprintf(stderr, "Failed to open output file %s\n", input);
        return ret;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket inPacket;

    av_init_packet(&inPacket);
    while (av_read_frame(inctx, &inPacket) >= 0) {
        if (inavctx[inPacket.stream_index] != NULL) {
            int frameFinished;
            if ((ret = decodePacket(inPacket.stream_index, &inPacket, frame, &frameFinished)) < 0) {
                return ret;
            }
        } else {
            if ((ret = av_interleaved_write_frame(outctx, &inPacket)) < 0) {
                fprintf(stderr, "Failed to write packet\n");
                return ret;
            }
        }
    }

    for (n = 0; n < inctx->nb_streams; n++) {
        if (inavctx[n]) {
            // flush decoder
            int frameFinished;
            do {
                inPacket.data = NULL;
                inPacket.size = 0;
                if ((ret = decodePacket(n, &inPacket, frame, &frameFinished)) < 0)
                    return ret;
            } while (frameFinished);

            // flush encoder
            int gotOutput;
            do {
                if ((ret = encodeFrame(n, NULL, &gotOutput)) < 0)
                    return ret;
            } while (gotOutput);
        }
    }
    av_free_packet(&inPacket);

    closeInputFile();
    closeOutputFile();

    return 0;
}
