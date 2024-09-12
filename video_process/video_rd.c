#include "video_rd.h"


void init_video(VideoContext *vc, const char *filename, int width, int height, int fps) {
    // Allocate format context and set output format to MP4
    avformat_alloc_output_context2(&vc->format_context, NULL, NULL, filename);
    if (!vc->format_context) {
        fprintf(stderr, "Could not allocate output format context\n");
        return;
    }

    // Find and create a video stream with H.264 codec
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "H.264 codec not found\n");
        return;
    }

    vc->video_stream = avformat_new_stream(vc->format_context, codec);
    if (!vc->video_stream) {
        fprintf(stderr, "Could not create video stream\n");
        return;
    }

    // Set codec parameters
    vc->codec_context = avcodec_alloc_context3(codec);
    vc->codec_context->width = width;
    vc->codec_context->height = height;
    vc->codec_context->time_base = (AVRational){1, fps};
    vc->codec_context->framerate = (AVRational){fps, 1};
    vc->codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

    // Set stream parameters and open codec
    avcodec_parameters_from_context(vc->video_stream->codecpar, vc->codec_context);
    if (avcodec_open2(vc->codec_context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    // Open output file
    if (!(vc->format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&vc->format_context->pb, filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", filename);
            return;
        }
    }

    // Write file header
    avformat_write_header(vc->format_context, NULL);

    // Allocate frame buffer
    vc->frame = av_frame_alloc();
    vc->frame->format = vc->codec_context->pix_fmt;
    vc->frame->width = width;
    vc->frame->height = height;
    av_frame_get_buffer(vc->frame, 32);

    // Set up conversion context to convert from YUYV to YUV420P
    vc->sws_context = sws_getContext(width, height, AV_PIX_FMT_YUYV422,
                                     width, height, AV_PIX_FMT_YUV420P,
                                     SWS_BILINEAR, NULL, NULL, NULL);

    vc->frame_index = 0;
}

void write_frame(VideoContext *vc, const uint8_t *yuyv_data) {
    // Convert YUYV to YUV420P
    const uint8_t *src_data[1] = { yuyv_data };
    int src_linesize[1] = { vc->codec_context->width * 2 };
    sws_scale(vc->sws_context, src_data, src_linesize, 0, vc->codec_context->height, vc->frame->data, vc->frame->linesize);

    vc->frame->pts = vc->frame_index++;

    // Encode the frame
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    int ret = avcodec_send_frame(vc->codec_context, vc->frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(vc->codec_context, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            return;
        }

        // Write encoded packet
        av_interleaved_write_frame(vc->format_context, &pkt);
        av_packet_unref(&pkt);
    }
}

void close_video(VideoContext *vc) {
    // Write trailer and clean up
    av_write_trailer(vc->format_context);
    avcodec_free_context(&vc->codec_context);
    av_frame_free(&vc->frame);
    sws_freeContext(vc->sws_context);
    if (!(vc->format_context->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&vc->format_context->pb);
    }
    avformat_free_context(vc->format_context);
}
