#include <stdio.h>
#include <stdlib.h>


#include "video_rd.h"

// 视频宽度和高度（根据摄像头的分辨率调整）
#define WIDTH 960
#define HEIGHT 540
#define FPS 20

// // FFmpeg 初始化
// void initialize_ffmpeg() {
//     // 注册所有编解码器和格式
//     av_register_all();
// }

// 设置输出文件和视频流的编码参数
AVFormatContext* setup_output(const char *filename, AVCodecContext **codec_context, AVStream **video_stream) {
    AVFormatContext *format_context = NULL;
    AVCodec *codec = NULL;

    // 为输出文件分配格式上下文
    avformat_alloc_output_context2(&format_context, NULL, NULL, filename);
    if (!format_context) {
        fprintf(stderr, "Could not create output context\n");
        exit(1);
    }

    // 查找 H.264 编码器
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    // 创建一个新的视频流
    *video_stream = avformat_new_stream(format_context, codec);
    if (!*video_stream) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }

    // 为编码器分配上下文
    *codec_context = avcodec_alloc_context3(codec);
    if (!*codec_context) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    // 设置编码参数（视频宽度、高度、像素格式、时间基准等）
    (*codec_context)->height = HEIGHT;
    (*codec_context)->width = WIDTH;
    (*codec_context)->sample_aspect_ratio = (AVRational){1, 1};
    (*codec_context)->pix_fmt = AV_PIX_FMT_YUV420P; // 使用 YUV 420P 格式
    (*codec_context)->time_base = (AVRational){1, FPS}; // 帧率 25 fps

    // 打开编码器
    if (avcodec_open2(*codec_context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    // 将编码器的参数复制到视频流中
    avcodec_parameters_from_context((*video_stream)->codecpar, *codec_context);

    // 打开输出文件
    if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_context->pb, filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open output file\n");
            exit(1);
        }
    }

    // 写入文件头
    if (avformat_write_header(format_context, NULL) < 0) {
        fprintf(stderr, "Error occurred when writing header\n");
        exit(1);
    }

    return format_context;
}

// 编码并写入帧数据
void encode_frame(AVCodecContext *codec_context, AVFormatContext *format_context, AVStream *video_stream, uint8_t *frame_data) {
    AVFrame *frame = av_frame_alloc();
    AVPacket packet;

    // 初始化 AVPacket
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    // 设置 AVFrame 的基本参数
    frame->format = codec_context->pix_fmt;
    frame->width  = codec_context->width;
    frame->height = codec_context->height;
    
    // 为帧分配缓冲区
    av_frame_get_buffer(frame, 32);

    // 这里模拟将原始的 RGB24 数据拷贝到 AVFrame 中
    // 实际上你可能需要根据输入数据的格式使用 libswscale 转换
    memcpy(frame->data[0], frame_data, codec_context->width * codec_context->height * 3);

    // 发送帧到编码器
    if (avcodec_send_frame(codec_context, frame) < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        exit(1);
    }

    // 从编码器中接收已编码的包
    while (avcodec_receive_packet(codec_context, &packet) == 0) {
        av_write_frame(format_context, &packet);
        av_packet_unref(&packet);
    }

    av_frame_free(&frame);
}

// 结束编码并写入文件尾
void finalize(AVFormatContext *format_context, AVCodecContext *codec_context) {
    // 写入文件尾
    av_write_trailer(format_context);

    // 关闭编码器
    avcodec_close(codec_context);

    // 关闭输出文件
    if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&format_context->pb);
    }

    // 释放格式上下文
    avformat_free_context(format_context);
}
