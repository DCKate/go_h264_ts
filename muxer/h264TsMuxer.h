#ifndef _H264_TS_MUXER_H
#define _H264_TS_MUXER_H
#include <stdio.h>
#include <libavformat/avformat.h>

typedef struct TsManager {
    AVFormatContext *ofmt_ctx;
    AVFormatContext *ifmt_ctx;
    AVBSFContext *h264bsf_ctx;
    AVDictionary *opts;
    int videoindex_out;

    int64_t last_frame_pts;
    int64_t last_pts;
    int fps;
    int frame_h;
    int frame_w;
    int bitrate;
    int wait_key_frame;
}TsManager;

void DeleteTsManager(TsManager *tmr);
int NewTsManagerInstsnce(TsManager* mgr,const char* filename,int fps,int in_w,int in_h,int bitrate);
int HandleReceiveFrameData(TsManager *tmr, const uint8_t *buffer,int fsize,int64_t frame_pts);

#endif