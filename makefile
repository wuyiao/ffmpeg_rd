# CROSS_COMPILE=aarch64-linux-gnu-
# CC = $(CROSS_COMPILE)gcc
# CPP = $(CROSS_COMPILE)gcc
# FORCE:
BIN=app
CC=gcc
CPP=g++

INCS=-I./include 
INCS+=-I./camera 
INCS+=-I./log
INCS+=-I./v4l2
INCS+=-I./video_process

ROOT_PATH = $(shell pwd)
INCS+=-I${ROOT_PATH}/lib/ffmpeg/include 
LIBS+=-L${ROOT_PATH}/lib/ffmpeg/lib -Wl,-rpath=${ROOT_PATH}/lib/ffmpeg/lib
LIBS+=-L${ROOT_PATH}/lib/ffmpeg/lib -Wl,-rpath-link=${ROOT_PATH}/lib/ffmpeg/lib

JPEG_LIBS+=-ljpeg
PTHREAD_LIBS+=-lpthread
FFMPEGLIB_LIBS+=-lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale 
LDFLAGS=$(JPEG_LIBS) $(PTHREAD_LIBS) $(FFMPEGLIB_LIBS)


CSRCS:=$(wildcard *.c ./camera/*.c ./log/*.c ./v4l2/*.c ./video_process/*.c) 
COBJS:=$(CSRCS:.c=.o)

CPPSRCS:=$(wildcard *.cpp) 
CPPOBJS:=$(CPPSRCS:.cpp=.o)

all:$(BIN)

$(COBJS) : %.o: %.c
	$(CC) -c $< -o $@ $(INCS) $(CFLAGS)

$(CPPOBJS) : %.o: %.cpp
	$(CPP) -c $< -o $@ $(INCS) $(CPPFLAGS) 

$(BIN):$(COBJS) $(CPPOBJS)
	$(CC) -o $(BIN) $(COBJS) $(CPPOBJS) $(LIBS) $(LDFLAGS) 

clean:
	rm $(BIN) $(COBJS) $(CPPOBJS)

#export LD_LIBRARY_PATH=~/work/touch/ffmpeg_rd/lib/ffmpeg/lib:$LD_LIBRARY_PATH
# ffmpeg -framerate 20 -i image%03d.jpg -c:v libx264 -pix_fmt yuv420p output.mp4

