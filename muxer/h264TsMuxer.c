#include "h264TsMuxer.h"

// < 0 = error
// 0 = I-Frame
// 1 = P-Frame
// 2 = B-Frame
// 3 = S-Frame
int get_vop_type( const void *p, int len )
{   
    if ( !p || 6 >= len )
        return -1;

    unsigned char *b = (unsigned char*)p;

    // Verify NAL marker
    if ( b[ 0 ] || b[ 1 ] || 0x01 != b[ 2 ] )
    {   b++;
        if ( b[ 0 ] || b[ 1 ] || 0x01 != b[ 2 ] )
            return -1;
    } // end if

    b += 3;

    // Verify VOP id
    if ( 0xb6 == *b )
    {   b++;
        return ( *b & 0xc0 ) >> 6;
    } // end if

    switch( *b )
    {   case 0x65 : return 0;
        case 0x61 : return 1;
        case 0x01 : return 2;
    } // end switch

    return -1;
}

int init_video_codec_contex(AVCodecContext **video_ctx, enum AVCodecID cid, AVDictionary **opts, int fps,int in_w,int in_h,int bt){
    // AVCodec* pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodec* pCodec = avcodec_find_encoder(cid);
    if(pCodec==NULL){
        printf("Unsupport codec %d\n",cid);
        return -1;
    }

    (*video_ctx) = avcodec_alloc_context3(pCodec);
    (*video_ctx)->codec_id = cid;  
    (*video_ctx)->codec_type = AVMEDIA_TYPE_VIDEO; 
    // tmr->video_ctx->codec_id = tmr->ofmt_ctx->oformat->video_codec; 
    (*video_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;  
    (*video_ctx)->width = in_w;    
    (*video_ctx)->height = in_h;  
    (*video_ctx)->bit_rate = bt;
    // (*video_ctx)->bit_rate = 400000;    
    (*video_ctx)->gop_size = fps;  
  
    (*video_ctx)->time_base = (AVRational){1,fps};
    //video_enc_ctx->time_base.num = 1;    
    //video_enc_ctx->time_base.den = 25;    
  
    //H264  
    //pCodecCtx->me_range = 16;  
    //pCodecCtx->max_qdiff = 4;  
    //pCodecCtx->qcompress = 0.6;  
    // (*video_ctx)->qmin = 10;  
    // (*video_ctx)->qmax = 51;  
    // (*video_ctx)->gop_size = 12;
  
    //Optional Param  
    (*video_ctx)->max_b_frames=0;  

    /* Init the decoders, with or without reference counting */
    av_dict_set(opts, "refcounted_frames", "0", 0);
    if (avcodec_open2((*video_ctx), pCodec,opts) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return -1;
    }
    
    return 0;

}

// int decode_setting(){
//     const char *filename = "a.mp4";
//     AVFormatContext fmt_ctx = nullptr;
//     avformat_open_input(&fmt_ctx, filename, nullptr, nullptr); // open a streaming file
//     avformat_find_stream_info(fmt_ctx, nullptr);//read file and the stream info
//     for(size_t i = 0; i < fmt_ctx->nb_streams; ++i)
//     {
//         AVStream *stream = fmt_ctx->streams[i];
//         AVCodec *codec =  avcodec_find_decoder(stream->codecpar->codec_id);
//         AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);//free by avcodec_free_context

//         //AVCodecParameters copt to AVCodecContext
//         avcodec_parameters_to_context(codec_ctx, stream->codecpar);

//         av_codec_set_pkt_timebase(codec_ctx, stream->time_base);
//         avcodec_open2(codec_ctx, codec, nullptr);
//     }
// }


int encode_setting(const char* filename,TsManager *tmr){
    AVCodecContext *in_video_ctx;
    AVCodecContext *out_video_ctx;
    // ---- input data stream
    tmr->ifmt_ctx = avformat_alloc_context();
    if (!tmr->ifmt_ctx) {
        printf("Call avformat_alloc_context function failed!\n");
        return -1;
    }

    //new AVStream as fmt_ctx，free by avformat_free_context
    tmr->in_stream = avformat_new_stream(tmr->ifmt_ctx, NULL);
    init_video_codec_contex(&in_video_ctx,AV_CODEC_ID_H264,
        &tmr->opts,tmr->fps,tmr->frame_w,tmr->frame_h,tmr->bitrate);
   
    //copy AVCodecContext to AVCodecParameters
    avcodec_parameters_from_context(tmr->in_stream->codecpar, in_video_ctx);
    av_stream_set_r_frame_rate(tmr->in_stream, in_video_ctx->time_base);
    tmr->in_stream->time_base = in_video_ctx->time_base;
    
    // ---- output data stream
    avformat_alloc_output_context2(&tmr->ofmt_ctx, NULL, NULL, filename);//need free by avformat_free_context
    if (!tmr->ofmt_ctx) {
        printf("Could not deduce output format from file extension\n");
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    
    tmr->ofmt_ctx->oformat = av_guess_format(NULL, filename, NULL);
    if (!tmr->ofmt_ctx->oformat) {
        printf("Call av_guess_format function failed!\n");
        return -1;
    }

    //new AVStream as fmt_ctx，free by avformat_free_context
    tmr->out_stream = avformat_new_stream(tmr->ofmt_ctx, NULL);
    init_video_codec_contex(&out_video_ctx,tmr->ofmt_ctx->oformat->video_codec,
        &tmr->opts,tmr->fps,tmr->frame_w,tmr->frame_h,tmr->bitrate);
    
    //copy AVCodecContext to AVCodecParameters
    avcodec_parameters_from_context(tmr->out_stream->codecpar, out_video_ctx);
    av_stream_set_r_frame_rate(tmr->out_stream, out_video_ctx->time_base);
    tmr->out_stream->time_base = out_video_ctx->time_base;
    
    if ((tmr->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        out_video_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    avcodec_free_context(&in_video_ctx);
    avcodec_free_context(&out_video_ctx);
    return 0;
}

int open_file(const char* filename,TsManager *tmr){
    int ret = 0;
    printf("========== Output Information ==========\n");
    av_dump_format(tmr->ofmt_ctx, 0, filename, 1);
    printf("======================================\n");

    if ((ret = avio_open(&tmr->ofmt_ctx->pb, filename, AVIO_FLAG_WRITE)) < 0) {
        printf("Could not open : %s\n",av_err2str(ret));
        return ret;
    }

    //Write file header
    if ((ret = avformat_write_header(tmr->ofmt_ctx, &tmr->opts) )< 0){
        printf("Error occurred when opening output file %s\n",av_err2str(ret));
        return ret;
    }
    return ret;
}

int generate_AVPacket(TsManager *tmr, AVPacket *pkt, const uint8_t *buf,int fsize){
    int ret = av_new_packet(pkt, fsize);
    if (ret < 0){
        printf("Error GenerateAVPacket%s\n",av_err2str(ret));
        return -1;
    }
    pkt->flags |= ( 0 >= get_vop_type( buf, fsize ) ) ? AV_PKT_FLAG_KEY : 0;   
    if ( tmr->wait_key_frame ){
        if ( 0 == ( pkt->flags & AV_PKT_FLAG_KEY ) ){
            av_packet_unref(pkt);
            return -1;
        }
        else{
            tmr->wait_key_frame = 0;
        }
    }
    memcpy(pkt->data, buf, fsize);
    return 0;
}

static int set_bitstream_filter(TsManager *tmr){
    int ret = 0;

    const AVBitStreamFilter *bsf= av_bsf_get_by_name("h264_mp4toannexb");
    if (bsf == NULL) {
        printf("call av_bsf_get_by_name, fail\n");
        return -1;
    } 

    if ((ret = av_bsf_alloc(bsf, &tmr->bsf_ctx)) < 0){
        printf("call av_bsf_alloc, ret:%d\n", ret);
        return ret;
    }

    if ((ret = avcodec_parameters_copy(tmr->bsf_ctx->par_in, tmr->in_stream->codecpar)) < 0){
        printf("call avcodec_parameters_copy, ret:%d\n", ret);
        av_bsf_free(&tmr->bsf_ctx);
        return ret;
    }
    tmr->bsf_ctx->time_base_in = tmr->in_stream->time_base;
    if((ret = av_bsf_init(tmr->bsf_ctx)) < 0) {
        printf("call av_bsf_init, ret:%d\n", ret);
        av_bsf_free(&tmr->bsf_ctx);
        return ret;
    }

    if ((ret = avcodec_parameters_copy(tmr->out_stream->codecpar, tmr->bsf_ctx->par_out)) < 0){
        printf("call avcodec_parameters_copy, ret:%d\n", ret);
        av_bsf_free(&tmr->bsf_ctx);
        return ret;
    }
    tmr->out_stream->time_base = tmr->bsf_ctx->time_base_out;
    return ret;
}

int process_packet(TsManager *tmr,AVPacket *pkt,int64_t frame_pts,int stream_index){
    int ret = 0;
    tmr->out_stream->codecpar->codec_tag = 0;
    if (frame_pts<0){   
        //Write PTS/DTS
        // Duration between 2 frames (s)
        double calc_duration = 1 * av_q2d(tmr->in_stream->r_frame_rate);
        //Parameters
        pkt->duration = AV_TIME_BASE * calc_duration;
        pkt->pts = tmr->last_pts + pkt->duration * av_q2d(tmr->in_stream->time_base);
    }else{
        pkt->duration = frame_pts-tmr->last_pts;
        pkt->pts = frame_pts;
        pkt->dts = pkt->pts;
    }
    tmr->last_pts = pkt->pts;
    pkt->dts = pkt->pts;

    pkt->stream_index = stream_index;
    printf("Write 1 Packet. size:%5d\tpts:%lld\t dts:%lld\n", pkt->size, pkt->pts, pkt->dts);
    //Write
    if((ret = av_bsf_send_packet(tmr->bsf_ctx, pkt)) < 0)
    {
        printf("call av_bsf_send_packet, %s\n", av_err2str(ret));
        return ret;
    }
    while(av_bsf_receive_packet(tmr->bsf_ctx, pkt) == 0){
        if ( (ret = av_interleaved_write_frame(tmr->ofmt_ctx, pkt) ) < 0)
        {
            printf("Error muxing packet %s %p\n", av_err2str(ret),pkt);
            return ret;
        }
    }
    return ret;
    // av_packet_unref(pkt);
}

void DeleteTsManager(TsManager *tmr){
    av_write_trailer(tmr->ofmt_ctx);
    av_bsf_free(&tmr->bsf_ctx);
    /* close output */
    if (tmr->ofmt_ctx && !(tmr->ofmt_ctx->oformat->flags & AVFMT_NOFILE)){
        avio_close(tmr->ofmt_ctx->pb);
    }
    avformat_free_context(tmr->ofmt_ctx);
    avformat_free_context(tmr->ifmt_ctx);
}

// int GlobalInit(){
//     av_register_all();
//     return 0;
// } 

int NewTsManagerInstsnce(TsManager* mgr,const char* filename,int fps,int in_w,int in_h,int bitrate){
    int ok = 0;
    mgr->wait_key_frame=1;
    mgr->last_pts = 0;
    mgr->fps = fps;
    mgr->frame_h = in_w;
    mgr->frame_w = in_h;
    mgr->bitrate = bitrate;
    if ((ok = encode_setting(filename,mgr)) < 0){
        return ok;
    }
    if ((ok = set_bitstream_filter(mgr)) < 0){
        return ok;
    }
    ok = open_file(filename,mgr);
    return ok;
}

int HandleReceiveFrameData(TsManager *tmr, const uint8_t *buffer,int fsize){
    AVPacket pkt;
    int ok = 0;
    if ((ok = generate_AVPacket(tmr,&pkt,buffer,fsize)) < 0){
        return ok;
    }
    ok = process_packet(tmr,&pkt,-1,0);
    av_packet_unref(&pkt);
    return ok;
}

// int main(int argc, char const *argv[])
// {
//     av_register_all();
//     TsManager mgr;
//     mgr.wait_key_frame=1;
//     mgr.fps = 15;
//     mgr.frame_h = 360;
//     mgr.frame_w = 640;
//     encode_setting("test.ts",&mgr);
//     set_bitstream_filter(&mgr);
//     open_file("test.ts",&mgr);
//     const int BUFFERSIZE = 81920;    
//     uint8_t buffer[BUFFERSIZE]={0};
//     char name [100]={0};
//     for (int ii=1;ii!=2768;ii++){
//         snprintf ( name, 100, "/Users/kate_hung/Documents/hlsdemo/BackhandShotsAllEnglandOpenLow/frame%d.h264", ii);
//         printf("%s\n",name);
//         FILE * filp = fopen(name, "rb"); 
//         int bytes_read = fread(buffer, sizeof(uint8_t), BUFFERSIZE, filp);
//         printf("read %d %p\n",bytes_read,buffer);
//         AVPacket pkt;
//         int ok = generate_AVPacket(&mgr,&pkt,buffer,bytes_read);
//         if (ok==0){
//             process_packet(&mgr,&pkt,-1,0);
//             av_packet_unref(&pkt);
//         }
//         fclose(filp);
//     }
//     DeleteTsManager(&mgr);
//     return 0;
// }
// gcc -L /usr/local/lib/ h264TsMuxer.c -o tsmuxer -lavformat -lavcodec -lavutil -lm -lpthread -lswresample -lx264 -lbz2 -lz