#include <stdio.h>
#include<unistd.h>
#include <pthread.h>
#include "common.h"
#include "log.h"
#include "camera.h"

static void show_usage(const char *name)
{
    printf("usage: \n");
    printf("%s <options>\n", name);
    printf("\n");
    printf("  options:\n");
    printf("\n");
    printf("  -h             show this help text\n");
    printf("  -a             number\n");
    printf("  -b             char\n");
    printf("  -c             printf\n");
    printf("  -d             debug level of log terminal\n");
    printf("  -e             debug level of log file\n");
    printf("\n");
}

int main(int argc,char * argv[])
{
    int opt,log_level_terminal = LOG_DEBUG,log_level_file = LOG_DEBUG;
    const char *options = "abcd:e:";
    FILE * log_file_fd = NULL;


    while ((opt = getopt(argc, argv, options)) != -1) {
        switch (opt) {
            case 'a':
                break;
            case 'b':
                break;
            case 'c':
                break;
            case 'd':
                log_level_terminal = atoi(optarg);
                break;
            case 'e':
                log_level_file = atoi(optarg);
                break;
            default:
                show_usage(argv[0]);
                _exit(0);
                break;
        }
    }

    log_file_fd = log_init(log_level_terminal,log_level_file);
    if (log_file_fd == NULL)
        return 0;
    pthread_t camera_tid;
    init_camera_thread(&camera_tid);
    pthread_join(&camera_tid,NULL);
    uninit_camera_thread(&camera_tid);
    uninit_log(log_file_fd);


    return 0;
}