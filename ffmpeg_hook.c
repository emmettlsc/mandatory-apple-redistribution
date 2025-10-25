#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// counters
static int packet_count = 0;
static int frame_count = 0;

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
        printf("[ffmpeg hook] frame #%d decoded\n", frame_count);
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
    printf("===========================\n");
}
