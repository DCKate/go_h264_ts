#include <stdio.h>
#include <assert.h>
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 1
/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter
#define USE_AACBSF 0
static struct
{
    AVPacketList *ahead;
    AVPacketList *atail;
    AVPacketList *vhead;
    AVPacketList *vtail;
} packet_list;

static int ff_packet_list_put(AVPacketList **packet_buffer,
                              AVPacketList **packet_buffer_end,
                              AVPacket *pkt)
{
    AVPacketList *tmp = av_mallocz(sizeof(AVPacketList));
    int ret;
    if (!tmp)
        return AVERROR(ENOMEM);
    tmp->pkt = *pkt;
    if (*packet_buffer)
        (*packet_buffer_end)->next = tmp;
    else
        *packet_buffer = tmp;
    *packet_buffer_end = tmp;
    return 0;
}

static int ff_packet_list_get(AVPacketList **pkt_buffer,
                              AVPacketList **pkt_buffer_end,
                              AVPacket *pkt)
{
    AVPacketList *tmp;
    assert(*pkt_buffer);
    tmp = *pkt_buffer;
    *pkt = tmp->pkt;
    *pkt_buffer = tmp->next;
    if (!tmp->next)
        *pkt_buffer_end = NULL;
    av_freep(&tmp);
    return 0;
}

static int bsf_init(AVBSFContext **bsf_ctx, const char *bsf_name, AVStream *in_stream, AVStream *out_stream) {
    const AVBitStreamFilter *filter;
    AVBSFContext *ctx;
    int ret;
    filter = av_bsf_get_by_name(bsf_name);
    assert(filter);
    if((ret = av_bsf_alloc(filter, &ctx)) < 0) {
        fprintf(stderr, "call av_bsf_alloc, ret:%d\n", ret);
        return ret;
    }
    avcodec_parameters_copy(ctx->par_in, in_stream->codecpar);
    ctx->time_base_in = in_stream->time_base;
    if((ret = av_bsf_init(ctx)) < 0) {
        fprintf(stderr, "call av_bsf_init, ret:%d\n", ret);
        av_bsf_free(&ctx);
        return ret;
    }
    avcodec_parameters_copy(out_stream->codecpar, ctx->par_out);
    out_stream->time_base = ctx->time_base_out;
    *bsf_ctx = ctx;
    return 0;
}

int main(int argc, char *argv[])
{
    //Input AVFormatContext and Output AVFormatContext
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex = -1, videoindex_out = -1;
    int audioindex = -1, audioindex_out = -1;
    int frameindex = 0;
    int64_t cur_pts_v = 0, cur_pts_a = 0;
    const char *in_filename, *out_filename;
    AVBSFContext *bsf_ctx, *video_bsf_ctx = NULL, *audio_bsf_ctx = NULL;

    if(argc < 3) {
        fprintf(stderr, "usage:%s in_file out_file\n", argv[0]);
        return -1;
    }
    in_filename = argv[1];
    out_filename = argv[2];

    if ((ret = avformat_open_input(&ifmt_ctx, argv[1], 0, 0)) < 0)
    {
        printf("Could not open input file. %s\n",argv[1]);
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0)
    {
        printf("Failed to retrieve input stream information");
        goto end;
    }
    printf("===========Input Information==========\n");
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    printf("======================================\n");
    //Output
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx)
    {
        printf("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        //Create output AVStream according to input AVStream
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            AVStream *in_stream = ifmt_ctx->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            #if USE_H264BSF
                bsf_init(&video_bsf_ctx, "h264_mp4toannexb", in_stream, out_stream);
            #else
                avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            #endif
            videoindex = i;
            if (!out_stream)
            {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            videoindex_out = out_stream->index;
            out_stream->codecpar->codec_tag = 0;
        }
        //Create output AVStream according to input AVStream
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            AVStream *in_stream = ifmt_ctx->streams[i];
            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            #if USE_AACBSF
                bsf_init(&audio_bsf_ctx, "aac_adtstoasc", in_stream, out_stream);
            #else
                avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            #endif
            audioindex = i;
            if (!out_stream)
            {
                printf("Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                goto end;
            }
            audioindex_out = out_stream->index;
            out_stream->codecpar->codec_tag = 0;
        }
    }
    printf("==========Output Information==========\n");
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    printf("======================================\n");
    //Open output file
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0)
        {
            printf("Could not open output file '%s'", out_filename);
            goto end;
        }
    }
    //Write file header
    if (avformat_write_header(ofmt_ctx, NULL) < 0)
    {
        printf("Error occurred when opening output file\n");
        goto end;
    }

    memset(&packet_list, 0, sizeof(packet_list));
    while (1)
    {
        int stream_index = 0;
        AVStream *in_stream, *out_stream;
        //Get an AVPacket
        pkt.stream_index = -1;
        stream_index = -1;
        if (av_compare_ts(cur_pts_v, ifmt_ctx->streams[videoindex]->time_base, cur_pts_a, ifmt_ctx->streams[audioindex]->time_base) <= 0)
        {
            if (packet_list.vhead)
            {
                stream_index = videoindex_out;
                ff_packet_list_get(&packet_list.vhead, &packet_list.vtail, &pkt);
            }
            else
            {
                while (av_read_frame(ifmt_ctx, &pkt) >= 0)
                {
                    if (pkt.stream_index == videoindex)
                    {
                        stream_index = videoindex_out;
                        break;
                    }
                    else
                        ff_packet_list_put(&packet_list.ahead, &packet_list.atail, &pkt);
                }
            }
        }
        else
        {
            if (packet_list.ahead)
            {
                stream_index = audioindex_out;
                ff_packet_list_get(&packet_list.ahead, &packet_list.atail, &pkt);
            }
            else
            {
                while (av_read_frame(ifmt_ctx, &pkt) >= 0)
                {
                    if (pkt.stream_index == audioindex)
                    {
                        stream_index = audioindex_out;
                        break;
                    }
                    else
                        ff_packet_list_put(&packet_list.vhead, &packet_list.vtail, &pkt);
                }
            }
        }
        if (stream_index != -1)
        {
            in_stream = ifmt_ctx->streams[pkt.stream_index];
            out_stream = ofmt_ctx->streams[stream_index];
            //FIX：No PTS (Example: Raw H.264)
            //Simple Write PTS
            if (pkt.pts == AV_NOPTS_VALUE)
            {
                printf("pts == AV_NOPTS_VALUE\n");
                //Write PTS
                AVRational time_base1 = in_stream->time_base;
                //Duration between 2 frames (us)
                int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                //Parameters
                pkt.pts = (double)(frameindex * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                pkt.dts = pkt.pts;
                pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                frameindex++;
            }
            if (pkt.stream_index == videoindex) {
                bsf_ctx = video_bsf_ctx;
                cur_pts_v = pkt.pts;
            }
            else{
                bsf_ctx = audio_bsf_ctx;
                cur_pts_a = pkt.pts;
            }
        }
        else
            break;

        //Convert PTS/DTS
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = stream_index;
        // printf("Write 1 Packet. size:%5d\tpts:%lld\t dts:%lld\n", pkt.size, pkt.pts, pkt.dts);
        //Write
        if((video_bsf_ctx != NULL && bsf_ctx == video_bsf_ctx) || (audio_bsf_ctx != NULL && bsf_ctx == audio_bsf_ctx)){
            if((ret = av_bsf_send_packet(bsf_ctx, &pkt)) < 0)
            {
                fprintf(stderr, "call av_bsf_send_packet, ret:%d\n", ret);
                break;
            }
            while(av_bsf_receive_packet(bsf_ctx, &pkt) == 0)
            {
                if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0)
                {
                    printf("Error muxing packet\n");
                    break;
                }
            }
        }
        else
        {
            if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0)
            {
                printf("Error muxing packet\n");
                break;
            }
        }
        av_packet_unref(&pkt);
    }
    //Write file trailer
    av_write_trailer(ofmt_ctx);
    av_bsf_free(&video_bsf_ctx);
    av_bsf_free(&audio_bsf_ctx);
end:
    avformat_close_input(&ifmt_ctx);
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF)
    {
        printf("Error occurred.\n");
        return -1;
    }
    return 0;
}