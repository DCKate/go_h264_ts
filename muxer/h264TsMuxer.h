#ifndef _H264_TS_MUXER_H
#define _H264_TS_MUXER_H
#include <stdio.h>
#include <libavformat/avformat.h>

typedef struct TsManager {
    AVStream *out_stream;
    AVStream *in_stream;
    AVFormatContext *ofmt_ctx;
    AVFormatContext *ifmt_ctx;
    AVBSFContext *bsf_ctx;
    AVDictionary *opts;
    int last_pts;
    int fps;
    int frame_h;
    int frame_w;
    int bitrate;
    int wait_key_frame;
}TsManager;

void DeleteTsManager(TsManager *tmr);
int NewTsManagerInstsnce(TsManager* mgr,const char* filename,int fps,int in_w,int in_h,int bitrate);
int HandleReceiveFrameData(TsManager *tmr, const uint8_t *buffer,int fsize);

#endif