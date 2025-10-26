#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AVFrame structure - minimal fields we need
typedef struct AVFrame {
    uint8_t *data[8];
    int linesize[8];
    int width;
    int height;
    int format;
    // other fields exist but we don't care
} AVFrame;

// counters
static int packet_count = 0;
static int frame_count = 0;
static int replaced_count = 0;

// function pointers to real functions
static int (*real_avcodec_send_packet)(void*, void*) = NULL;
static int (*real_avcodec_receive_frame)(void*, void*) = NULL;

// hook avcodec_send_packet - called when compressed video packet is sent to decoder
int avcodec_send_packet(void* avctx, void* avpkt) {
    if (!real_avcodec_send_packet) {
        real_avcodec_send_packet = dlsym(RTLD_NEXT, "avcodec_send_packet");
        if (!real_avcodec_send_packet) {
            fprintf(stderr, "failed to find real avcodec_send_packet\n");
            return -1;
        }
    }

    packet_count++;
    printf("[ffmpeg hook] packet #%d sent to decoder\n", packet_count);

    return real_avcodec_send_packet(avctx, avpkt);
}

// replace frame with static bad apple frame
void replace_with_bad_apple(AVFrame* frame) {
    if (!frame->data[0] || frame->width <= 0 || frame->height <= 0) {
        return;
    }

    // YUV420P format (most common)
    // Y plane is full res, U and V are half res
    int y_size = frame->linesize[0] * frame->height;
    int uv_height = frame->height / 2;
    int u_size = frame->linesize[1] * uv_height;
    int v_size = frame->linesize[2] * uv_height;

    // fill with black (Y=16, U=128, V=128 in limited range YUV)
    if (frame->data[0]) memset(frame->data[0], 16, y_size);
    if (frame->data[1]) memset(frame->data[1], 128, u_size);
    if (frame->data[2]) memset(frame->data[2], 128, v_size);

    // draw white rectangle in center (Y=235 for white)
    int rect_w = frame->width / 3;
    int rect_h = frame->height / 3;
    int start_x = (frame->width - rect_w) / 2;
    int start_y = (frame->height - rect_h) / 2;

    for (int y = start_y; y < start_y + rect_h; y++) {
        if (y >= 0 && y < frame->height) {
            memset(frame->data[0] + y * frame->linesize[0] + start_x, 235, rect_w);
        }
    }

    replaced_count++;
}

// hook avcodec_receive_frame - called when decoder outputs a frame
int avcodec_receive_frame(void* avctx, void* frame) {
    if (!real_avcodec_receive_frame) {
        real_avcodec_receive_frame = dlsym(RTLD_NEXT, "avcodec_receive_frame");
        if (!real_avcodec_receive_frame) {
            fprintf(stderr, "failed to find real avcodec_receive_frame\n");
            return -1;
        }
    }

    int ret = real_avcodec_receive_frame(avctx, frame);

    if (ret == 0) {
        frame_count++;
        printf("[ffmpeg hook] frame #%d decoded - REPLACING WITH BAD APPLE\n", frame_count);

        // replace the frame data
        replace_with_bad_apple((AVFrame*)frame);
    }

    return ret;
}

__attribute__((constructor))
void init() {
    printf("\n=== ffmpeg hook loaded ===\n");
    printf("pid: %d\n", getpid());
    printf("watching avcodec_send_packet and avcodec_receive_frame\n");
    printf("========================\n\n");
}

__attribute__((destructor))
void cleanup() {
    printf("\n=== ffmpeg hook unloading ===\n");
    printf("packets: %d\n", packet_count);
    printf("frames: %d\n", frame_count);
    printf("frames replaced: %d\n", replaced_count);
    printf("===========================\n");
}
