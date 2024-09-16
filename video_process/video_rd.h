#ifndef VIDEO_RD__H_
#define VIDEO_RD__H_

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#ifdef __cplusplus
extern "C" {
#endif

AVFormatContext* setup_output(const char *filename, AVCodecContext **codec_context, AVStream **video_stream);
void encode_frame(AVCodecContext *codec_context, AVFormatContext *format_context, AVStream *video_stream, uint8_t *frame_data);
// void initialize_ffmpeg();
void finalize(AVFormatContext *format_context, AVCodecContext *codec_context);

#ifdef __cplusplus
}
#endif

#endif

