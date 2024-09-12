#ifndef VIDEO_RD__H_
#define VIDEO_RD__H_

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    AVFormatContext *format_context;
    AVStream *video_stream;
    AVCodecContext *codec_context;
    AVFrame *frame;
    struct SwsContext *sws_context;
    int frame_index;
} VideoContext;


void init_video(VideoContext *vc, const char *filename, int width, int height, int fps);
void write_frame(VideoContext *vc, const uint8_t *yuyv_data);
void close_video(VideoContext *vc);

#ifdef __cplusplus
}
#endif

#endif
