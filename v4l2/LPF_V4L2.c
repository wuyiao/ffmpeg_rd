#include "LPF_V4L2.h"
#include <jpeglib.h>
#include <stdio.h>
#include "video_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct buffer{
    void *start;
    unsigned int length;
}*buffers;
int buffers_length;

char runningDev[15] = "";
char devName[15] = "";
char camName[32] = "";
char devFmtDesc[4] = "";

int fd = -1;
int videoIsRun = -1;
int deviceIsOpen = -1;
unsigned char *rgb24 = NULL;
unsigned char *outBuf = NULL;
int outBufSize = 0;
u_int32_t outBufFmt = 0;
static int WIDTH, HEIGHT, FPS;
static u_int32_t FORMAT_TYPE;

//V4l2相关结构体
static struct v4l2_capability cap;
static struct v4l2_fmtdesc fmtdesc;
static struct v4l2_frmsizeenum frmsizeenum;
static struct v4l2_format format;
static struct v4l2_queryctrl  queryctrl;
static struct v4l2_requestbuffers reqbuf;
static struct v4l2_buffer buffer;
//static struct v4l2_streamparm streamparm;

//用户控制项ID
static __u32 brightness_id = -1;    //亮度
static __u32 contrast_id = -1;  //对比度
static __u32 saturation_id = -1;    //饱和度
static __u32 hue_id = -1;   //色调
static __u32 white_balance_temperature_auto_id = -1; //白平衡色温（自动）
static __u32 white_balance_temperature_id = -1; //白平衡色温
static __u32 gamma_id = -1; //伽马
static __u32 power_line_frequency_id = -1;  //电力线频率
static __u32 sharpness_id = -1; //锐度，清晰度
static __u32 backlight_compensation_id = -1;    //背光补偿
//扩展摄像头项ID
static __u32 exposure_auto_id = -1;  //自动曝光
static __u32 exposure_absolute_id = -1;

void StartVideoPrePare();
void StartVideoStream();
void EndVideoStream();
void EndVideoStreamClear();
int test_device_exist(char *devName);

static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;
    return pixel32;
}

// //convert mjpeg frame to RGB24
// int MJPEG2RGB(unsigned char* data_frame, unsigned char *rgb, int bytesused)
// {
//     // variables:
//     struct jpeg_decompress_struct cinfo;
//     struct jpeg_error_mgr jerr;
//     //unsigned int width, height;
//     // data points to the mjpeg frame received from v4l2.
//     unsigned char *data = data_frame;
//     size_t data_size =  bytesused;

// //    cinfo.image_width = WIDTH;
// //    cinfo.image_height = HEIGHT;
// //    cinfo.output_width = WIDTH;
// //    cinfo.output_height = HEIGHT;

//     // all the pixels after conversion to RGB.
//     int pixel_size = 0;//size of one pixel
//     if ( data == NULL  || data_size <= 0)
//     {
//         printf("Empty data!\n");
//         return -1;
//     }

//     // ... In the initialization of the program:
//     cinfo.err = jpeg_std_error(&jerr); //错误处理设置为默认处理方式
//     jpeg_create_decompress(&cinfo);
//     jpeg_mem_src(&cinfo, data, data_size);
//      int rc = jpeg_read_header(&cinfo, TRUE);
//      if(!(1==rc))
//      {
//          //printf("Not a jpg frame.\n");
//          //return -2;
//      }
//     jpeg_start_decompress(&cinfo);
//     // 6.申请存储一行数据的内存空间
//     int row_stride = cinfo.output_width * cinfo.output_components;//
//     unsigned char *buffer = malloc(row_stride);//
//     int i = 0;//

// //    width = cinfo.output_width;
// //	height = cinfo.output_height;
//     pixel_size = cinfo.output_components;

//     // ... Every frame:

//     while (cinfo.output_scanline < cinfo.output_height)
//     {
// //        unsigned char *temp_array[] ={ rgb + (cinfo.output_scanline) * WIDTH * pixel_size };
// //        jpeg_read_scanlines(&cinfo, temp_array, 1);
//         jpeg_read_scanlines(&cinfo, &buffer, 1);//
//         memcpy(rgb + i * WIDTH * 3, buffer, row_stride);//
//         i++;//
//     }
//     free(buffer);
//     jpeg_finish_decompress(&cinfo);
//     jpeg_destroy_decompress(&cinfo);

//     return 0;
// }

int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for(in = 0; in < width * height * 2; in += 4)
    {
        pixel_16 =
                yuv[in + 3] << 24 |
                               yuv[in + 2] << 16 |
                                              yuv[in + 1] <<  8 |
                                                              yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}

// int h264ToRGB(unsigned char *h264Data, int dataLen, unsigned char *rgb)
// {

//     int g_ffmpegParamBuf[8] = {0};

//     ffmpeg_decode_h264(h264Data, dataLen, g_ffmpegParamBuf, rgb, NULL);

//     return 0;
// }

void StartVideoPrePare()
{
    memset(&format, 0, sizeof (format));
    format.fmt.pix.field = V4L2_FIELD_ANY;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = WIDTH;
    format.fmt.pix.height = HEIGHT;
    format.fmt.pix.pixelformat = FORMAT_TYPE;//V4L2_PIX_FMT_MJPEG
    int ret = ioctl(fd, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        perror("change video format failed\n");
        return;
    }

    //申请帧缓存区
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 4;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf ("Video capturing or mmap-streaming is not supported\n");
        else
            perror ("VIDIOC_REQBUFS");
        return;
    }

    //分配缓存区
    buffers = calloc (reqbuf.count, sizeof (*buffers));
    if(buffers == NULL)
        perror("buffers is NULL");
    else
        assert (buffers != NULL);

    //mmap内存映射
    int i;
    for (i = 0; i < (int)reqbuf.count; i++) {
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
            perror ("VIDIOC_QUERYBUF");
            return;
        }

        buffers[i].length = buffer.length;

        buffers[i].start = mmap (NULL, buffer.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start) {
            perror ("mmap");
            return;
        }
    }

    //将缓存帧放到队列中等待视频流到来
    unsigned int ii;
    for(ii = 0; ii < reqbuf.count; ii++){
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = ii;
        if (ioctl(fd,VIDIOC_QBUF,&buffer)==-1){
            perror("VIDIOC_QBUF failed");
        }
    }

    WIDTH = LPF_GetCurResWidth();
    HEIGHT = LPF_GetCurResHeight();
    rgb24 = (unsigned char*)malloc(WIDTH*HEIGHT*3*sizeof(char));
    assert(rgb24 != NULL);
}

void StartVideoStream()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
        perror("VIDIOC_STREAMON failed");
    }
}

void EndVideoStream()
{
    //关闭视频流
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMOFF,&type) == -1) {
        perror("VIDIOC_STREAMOFF failed");
    }
}

void EndVideoStreamClear()
{
    //手动释放分配的内存
    int i;
    for (i = 0; i < (int)reqbuf.count; i++)
        munmap (buffers[i].start, buffers[i].length);
    free(rgb24);
    rgb24 = NULL;
}

void LPF_GetDevControlAll()
{
    int i = 0;
    for(i = V4L2_CID_BASE; i <= V4L2_CID_LASTP1; i++)
    {
        queryctrl.id = i;
        if(0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
            if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

            if(queryctrl.id == V4L2_CID_BRIGHTNESS)
                brightness_id = i;
            if(queryctrl.id == V4L2_CID_CONTRAST)
                contrast_id = i;
            if(queryctrl.id == V4L2_CID_SATURATION)
                saturation_id = i;
            if(queryctrl.id == V4L2_CID_HUE)
                hue_id = i;
            if(queryctrl.id == V4L2_CID_AUTO_WHITE_BALANCE)
                white_balance_temperature_auto_id = i;
            if(queryctrl.id == V4L2_CID_WHITE_BALANCE_TEMPERATURE)
                white_balance_temperature_id = i;
            if(queryctrl.id == V4L2_CID_GAMMA)
                gamma_id = i;
            if(queryctrl.id == V4L2_CID_POWER_LINE_FREQUENCY)
                power_line_frequency_id = i;
            if(queryctrl.id == V4L2_CID_SHARPNESS)
                sharpness_id = i;
            if(queryctrl.id == V4L2_CID_BACKLIGHT_COMPENSATION)
                backlight_compensation_id = i;
        }
        else
        {
            if(errno == EINVAL)
                continue;
            perror("VIDIOC_QUERYCTRL");
            return;
        }
    }

    queryctrl.id = V4L2_CTRL_CLASS_CAMERA | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (V4L2_CTRL_ID2CLASS (queryctrl.id) != V4L2_CTRL_CLASS_CAMERA)
            break;

        if(queryctrl.id == V4L2_CID_EXPOSURE_AUTO)
            exposure_auto_id = queryctrl.id;
        if(queryctrl.id == V4L2_CID_EXPOSURE_ABSOLUTE)
            exposure_absolute_id = queryctrl.id;

        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

void LPF_SetDevControlParam(int control_id, int value)
{
    if(videoIsRun < 0)
    {
        return;
    }

    struct v4l2_control control_s;
    control_s.id = control_id;
    control_s.value = value;

    if(ioctl(fd,VIDIOC_S_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_SetDevControlParam");
        return;
        //exit(EXIT_FAILURE);
    }
}

int LPF_SetResFpsFormat(int width, int height, int fps, u_int32_t formatType)
{
    WIDTH = width;
    HEIGHT = height;
    FPS = fps;
    FORMAT_TYPE = formatType;

    return 0;
}

int test_device_exist(char *devName)
{
    struct stat st;
    if (-1 == stat(devName, &st))
        return -1;

    if (!S_ISCHR (st.st_mode))
        return -1;

    return 0;
}
int LPF_GetDeviceCount()
{
    char devname[15] = "";
    int count = 0;
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
            count++;

        memset(devname, 0, sizeof(devname));
    }

    return count;
}

//根据索引获取设备名称
char *LPF_GetDeviceName(int index)
{
    memset(devName, 0, sizeof(devName));

    int count = 0;
    char devname[15] = "";
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
        {
            if(count == index)
                break;
            count++;
        }
        else
            memset(devname, 0, sizeof(devname));
    }

    strcpy(devName, devname);

    return devName;
}

//根据索引获取摄像头名称
char *LPF_GetCameraName(int index)
{
    if(videoIsRun > 0)
        return "";

    memset(camName, 0, sizeof(camName));

    char devname[15] = "";
    strcpy(devname, LPF_GetDeviceName(index));

    int fd = open(devname, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0) {
        close(fd);
        return NULL;
    }
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) != -1)
    {
        strcpy(camName, (char *)cap.card);
    }
    close(fd);

    return camName;
}

int videoDeviceOpen(int index)
{
    if(videoIsRun > 0)
        return -1;

    char *devname = LPF_GetDeviceName(index);
    if((fd = open(devname, O_RDWR | O_NONBLOCK, 0)) < 0) {
        perror("open uvc dev err\n");
        close(fd);
        return -1;
    }

    deviceIsOpen = 1;

    LPF_GetDevControlAll();

    return 0;
}

int videoDeviceClose()
{
    deviceIsOpen = -1;

    if(close(fd) != 0)
        return -1;

    return 0;
}

//运行指定索引的视频
int LPF_StartRun(int index)
{
    if(videoIsRun > 0 || index < 0)
        return -1;

    char *devname = LPF_GetDeviceName(index);

    if((fd = open(devname, O_RDWR | O_NONBLOCK, 0)) < 0) {
        perror("open uvc dev err\n");
        close(fd);
        return -1;
    }

    deviceIsOpen = 1;

    // setCurDecode_width_height(WIDTH, HEIGHT);
    // ffmpeg_init_h264_decoder();//init avcodec

    // LPF_GetDevControlAll();

    StartVideoPrePare();
    StartVideoStream();

    strcpy(runningDev, devname);

    videoIsRun = 1;

    return 0;
}

int LPF_GetFrame()
{
    if(videoIsRun > 0)
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO (&fds);
        FD_SET (fd, &fds);

        /* Timeout. */
        tv.tv_sec = 0;
        tv.tv_usec = 30000;

        r = select (fd + 1, &fds, NULL, NULL, &tv);

        if (0 == r)
            return -1;
        else if(-1 == r)
            return errno;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            perror("GetFrame VIDIOC_DQBUF Failed");
            return errno;
        }
        else
        {
            if (FORMAT_TYPE == V4L2_PIX_FMT_YUYV) {
                convert_yuv_to_rgb_buffer((unsigned char*)buffers[buffer.index].start, rgb24, WIDTH, HEIGHT);
            } else if (FORMAT_TYPE == V4L2_PIX_FMT_MJPEG) {
                // MJPEG2RGB((unsigned char*)buffers[buffer.index].start, rgb24, buffer.bytesused);
            } else if (FORMAT_TYPE == V4L2_PIX_FMT_H264) {
                // h264ToRGB((unsigned char*)buffers[buffer.index].start, buffer.bytesused, rgb24);
            }

            if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
                perror("GetFrame VIDIOC_QBUF Failed");
                return errno;
            }

            return 0;
        }
    }

    return 0;
}

int LPF_ReadFrame()
{
    if(videoIsRun > 0)
    {
        fd_set fds;
        struct timeval timeout;
        int ret;

        FD_ZERO (&fds);
        FD_SET (fd, &fds);

        /* Timeout. */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ret = select (fd + 1, &fds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "V4L2_CORE: Could not grab image (select error): %s\n", strerror(errno));
            return E_SELECT_ERR;
        }

        if (ret == 0)
        {
            //fprintf(stderr, "V4L2_CORE: Could not grab image (select timeout): %s\n", strerror(errno));
            return E_SELECT_TIMEOUT_ERR;
        }

        if (!((ret > 0) && (FD_ISSET(fd, &fds))))
            return E_UNKNOWN_ERR;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            perror("GetFrame VIDIOC_DQBUF Failed");
            return errno;
        }
        else
        {
            outBuf = (unsigned char*)buffers[buffer.index].start;
            outBufSize = buffer.bytesused;
            outBufFmt = FORMAT_TYPE;

            return 0;
        }
    }

    return -1;
}

int LPF_Videoc_Qbuf()
{

    if(videoIsRun > 0)
    {
        if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
            perror("GetFrame VIDIOC_QBUF Failed");
            return errno;
        }
        return 0;
    }
    return -1;
}

int LPF_StopRun()
{
    if(videoIsRun > 0)
    {
        EndVideoStream();
        EndVideoStreamClear();
    }

    memset(runningDev, 0, sizeof(runningDev));
    videoIsRun = -1;
    deviceIsOpen = -1;

    if(close(fd) != 0)
        return -1;

    // ffmpeg_release_video_decoder();

    return 0;
}

int LPF_DeviceRunState()
{
    return videoIsRun;
}

char *LPF_GetDevFmtDesc(int index)
{
    memset(devFmtDesc, 0, sizeof(devFmtDesc));

    fmtdesc.index=index;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        char fmt[5] = "";
        sprintf(fmt, "%c%c%c%c",
                (__u8)(fmtdesc.pixelformat&0XFF),
                (__u8)((fmtdesc.pixelformat>>8)&0XFF),
                (__u8)((fmtdesc.pixelformat>>16)&0XFF),
                (__u8)((fmtdesc.pixelformat>>24)&0XFF));

        strncpy(devFmtDesc, fmt, 4);
    }

    return devFmtDesc;
}

int LPF_GetDevFpsDesc(int i, int width, int height, u_int32_t data)
{
    struct v4l2_frmivalenum frmival;//frmival.index = 0;
    frmival.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//    if (data != V4L2_PIX_FMT_MJPEG) {
//        return -2;
//    }
    frmival.pixel_format = data;//V4L2_PIX_FMT_MJPEG
    frmival.width = width;
    frmival.height = height;
    frmival.index = i;
    while (0 == ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival)) {
        //printf("Frame interval<%ffps> ", frmival.discrete.denominator / frmival.discrete.numerator);
        return frmival.discrete.denominator / frmival.discrete.numerator;
    }
    return -1;
}

//获取图像的格式属性相关
int LPF_GetDevFmtWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtWidth:");
        return -1;
    }
    return format.fmt.pix.width;
}

int LPF_GetDevFmtHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtHeight:");
        return -1;
    }
    return format.fmt.pix.height;
}
int LPF_GetDevFmtSize()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtSize:");
        return -1;
    }
    return format.fmt.pix.sizeimage;
}
int LPF_GetDevFmtBytesLine()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtBytesLine:");
        return -1;
    }
    return format.fmt.pix.bytesperline;
}

//设备分辨率相关
int LPF_GetResolutinCount()
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;
    int i = 0;
    for(i = 0; ; i++)
    {
        frmsizeenum.index = i;
        if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1)
            break;
    }
    return i;
}
int LPF_GetResolutionWidth(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.width;
    else
        return -1;
}

int LPF_GetResolutionHeight(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.height;
    else
        return -1;
}
int LPF_GetCurResWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.width;
}
int LPF_GetCurResHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.height;
}

char* LPF_GetCurPixformat()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return NULL;

    memset(devFmtDesc, 0, sizeof(devFmtDesc));

    char fmt[5] = "";
    sprintf(fmt, "%c%c%c%c",
            (__u8)(format.fmt.pix.pixelformat&0XFF),
            (__u8)((format.fmt.pix.pixelformat>>8)&0XFF),
            (__u8)((format.fmt.pix.pixelformat>>16)&0XFF),
            (__u8)((format.fmt.pix.pixelformat>>24)&0XFF));

    strncpy(devFmtDesc, fmt, 4);
    return devFmtDesc;
}

int LPF_GetCurFps()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_streamparm parm;
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_PARM, &parm) == -1)
        return -1;
//frmival.discrete.denominator / frmival.discrete.numerator
    return parm.parm.capture.timeperframe.denominator/parm.parm.capture.timeperframe.numerator;
}

int LPF_GetExposureMode()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_EXPOSURE_AUTO;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetExposureMode");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

int LPF_GetExposureValue()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_EXPOSURE;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetExposureValue");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

int LPF_GetWhiteBalanceMode()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_AUTO_WHITE_BALANCE;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetWhiteBalanceMode");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

int LPF_GetWhiteBalance()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetWhiteBalance");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

int LPF_GetBrightness()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_BRIGHTNESS;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetBrightness");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

//对比度
int LPF_GetContrast()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_CONTRAST;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetContrast");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

//饱和度
int LPF_GetSaturation()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_SATURATION;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetSaturation");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

//色度
int LPF_GetHue()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_HUE;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetHue");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

//锐度
int LPF_GetSharpness()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_SHARPNESS;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetSharpness");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

int LPF_GetGain()
{
    if(videoIsRun < 0)
    {
        return -1;
    }

    struct v4l2_control control_s;
    control_s.id = V4L2_CID_GAIN;

    if(ioctl(fd,VIDIOC_G_CTRL,&control_s)==-1)
    {
        perror("ioctl LPF_GetGain");
        return -1;
        //exit(EXIT_FAILURE);
    }

    return control_s.value;
}

#ifdef __cplusplus
}
#endif


