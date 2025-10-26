#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AVFrame structure 
typedef struct AVFrame {
    uint8_t *data[8];
    int linesize[8];
    int width;
    int height;
    int format;
    // other fields exist but we don't care???
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

// replace frame with static bad apple frame (rn we just nuke it)
void replace_with_bad_apple(AVFrame* frame) {
    printf("[DEBUG] replace_with_bad_apple called\n");
    printf("[DEBUG] frame ptr: %p\n", frame);
    printf("[DEBUG] frame->data[0]: %p\n", frame->data[0]);
    printf("[DEBUG] frame->width: %d, height: %d\n", frame->width, frame->height);
    printf("[DEBUG] frame->format: %d\n", frame->format);

    if (!frame->data[0]) {
        printf("[DEBUG] no data[0], skipping\n");
        return;
    }

    if (frame->width <= 0 || frame->height <= 0) {
        printf("[DEBUG] invalid dimensions, skipping\n");
        return;
    }

    printf("[DEBUG] starting replacement...\n");

    // just nuke everything to full white
    int y_size = frame->linesize[0] * frame->height;

    printf("[DEBUG] filling Y plane with white (size: %d)\n", y_size);
    memset(frame->data[0], 255, y_size);

    // set U and V to neutral if they exist <-- ?? llm said this was needed
    if (frame->data[1] && frame->data[2]) {
        int uv_height = frame->height / 2;
        int u_size = frame->linesize[1] * uv_height;
        int v_size = frame->linesize[2] * uv_height;
        printf("[DEBUG] filling U/V planes (sizes: %d, %d)\n", u_size, v_size);
        memset(frame->data[1], 128, u_size);
        memset(frame->data[2], 128, v_size);
    }

    replaced_count++;
    printf("[DEBUG] replacement done! count: %d\n", replaced_count);
}

// hook avcodec_receive_frame - called when decoder outputs a frame
int avcodec_receive_frame(void* avctx, void* frame) {
    printf("[HOOK] avcodec_receive_frame called\n");

    if (!real_avcodec_receive_frame) {
        printf("[HOOK] finding real avcodec_receive_frame...\n");
        real_avcodec_receive_frame = dlsym(RTLD_NEXT, "avcodec_receive_frame");
        if (!real_avcodec_receive_frame) {
            fprintf(stderr, "failed to find real avcodec_receive_frame\n");
            return -1;
        }
        printf("[HOOK] found real function at %p\n", real_avcodec_receive_frame);
    }

    printf("[HOOK] calling real avcodec_receive_frame...\n");
    int ret = real_avcodec_receive_frame(avctx, frame);
    printf("[HOOK] real function returned: %d\n", ret);

    if (ret == 0) {
        frame_count++;
        printf("\n=== [HOOK] FRAME #%d DECODED - REPLACING NOW ===\n", frame_count);

        // replace the frame data
        replace_with_bad_apple((AVFrame*)frame);

        printf("=== [HOOK] REPLACEMENT COMPLETE ===\n\n");
    } else {
        printf("[HOOK] ret != 0, not replacing (ret=%d)\n", ret);
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
