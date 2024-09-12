#ifndef __CAMERA_H__
#define __CAMERA_H__

#define CAMERA   (char *)"/dev/video0"
#define IMAGE_WIDTH        (960)                           
#define IMAGE_HEIGHT       (540)
#define IMAGE_SIZE_NV12         (IMAGE_WIDTH * IMAGE_HEIGHT * 3 / 2)  //单张图片大小
#define IMAGE_SIZE_YUYV         (IMAGE_WIDTH * IMAGE_HEIGHT * 2)  //单张图片大小
#define IMAGE_SIZE_RGB24        (IMAGE_WIDTH * IMAGE_HEIGHT * 3)  //单张图片大小


#define gettid() syscall(__NR_gettid)


#ifdef __cplusplus
extern "C" 
{
#endif

int init_camera_thread(pthread_t *camera_tid);
void uninit_camera_thread(pthread_t *camera_tid);

#ifdef __cplusplus
}
#endif


#endif 