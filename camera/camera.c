#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include "jpeglib.h"
#include "LPF_V4L2.h"
#include "log.h"
#include "camera.h"
#include "video_rd.h"


 
static int rgb2jpgAction(struct jpeg_compress_struct* pCinfo, const char *pRgbData, const int width, const int height)
{
    int depth = 3;
    JSAMPROW row_pointer[1];
 
    pCinfo->image_width      = width;
    pCinfo->image_height     = height;
    pCinfo->input_components = depth;
    pCinfo->in_color_space   = JCS_RGB;
 
    jpeg_set_defaults(  pCinfo); 
    jpeg_set_quality(   pCinfo, 50, TRUE );
    jpeg_start_compress(pCinfo, TRUE);
 
    int row_stride = width * depth;
    while (pCinfo->next_scanline < pCinfo->image_height)
    {
        row_pointer[0] = (JSAMPROW)(pRgbData + pCinfo->next_scanline * row_stride);
        jpeg_write_scanlines(pCinfo, row_pointer, 1);
    }
 
    jpeg_finish_compress( pCinfo);
    jpeg_destroy_compress(pCinfo);
 
    return 0;
}
 
/**
 这里特别说明jpeg_mem_dest的第二个参数，buffer。
 如果在rgb2jpg声明指针或者缓冲区，然后试图复制到pDest，直接崩溃；或者取不到数据。
 研究了半天不行。必须是如下的写法。
 如果缓冲区不够怎么办？那就开大一点。
 char pDest[512*1024];
 int  size=512*1024;
 然后再传递过来。
 */
static int rgb2jpg(const char *pRgbData, const int width, const int height, int type, char* pDest, int* pSize)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
 
    FILE* pOutFile = NULL;
 
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
 
    if (type)
    {
        if ((pOutFile = fopen(pDest, "wb")) == NULL)
        {
            return -1;
        }
        jpeg_stdio_dest(&cinfo, pOutFile);
    }
    else
    {
        jpeg_mem_dest(&cinfo, (unsigned char **)&pDest, (long unsigned int *)pSize);
    }
 
    rgb2jpgAction(&cinfo, pRgbData, width, height);
    if (type)
    {
        fclose(pOutFile);
    }
 
    return 0;
}


void *thread_camera()
{
    LPF_SetResFpsFormat(960, 540, 20, V4L2_PIX_FMT_YUYV);
    LPF_StartRun(0);

    // // 初始化 FFmpeg
    // initialize_ffmpeg();

    // 设置输出文件
    AVCodecContext *codec_context = NULL;
    AVStream *video_stream = NULL;
    AVFormatContext *format_context = setup_output("./bin/camera.mp4", &codec_context, &video_stream);

    int image_num = 0;

    while (1)
    {
        LPF_GetFrame();  // 获取一帧数据
        

        encode_frame(codec_context, format_context, video_stream, rgb24);
        // 可选：可以在这里进行 JPEG 图像保存，但要注意可能对性能有影响
        // sprintf(filename, "/home/wuyiao/work/touch/gitfile/ffmpeg_rd/image%03d.jpg", image_num);
        // rgb2jpg(rgb24, 960, 540, 1, filename, IMAGE_SIZE_YUYV);
        
        usleep(50000);  // 控制帧率为 20fps (50 毫秒间隔)

        if (image_num++ > 100)  // 当写入超过 100 帧时结束
        {
            finalize(format_context, codec_context);
            LPF_StopRun();  // 停止获取帧数据
            break;  // 退出循环
        }
    }

    return NULL;
}


int init_camera_thread(pthread_t *camera_tid)
{
    int ret = pthread_create(camera_tid, NULL, thread_camera, NULL);
    if(ret != 0)
    {
        log_debug("pthread create stock thread_camera4 failed\n");
        return -1;
    }

    log_debug("init_camera_thread success\n");
    
	return 0;
}

void uninit_camera_thread(pthread_t *camera_tid)
{
    pthread_cancel(*camera_tid);
}
