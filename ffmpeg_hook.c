#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// real AVFrame layout from ffmpeg
// dont use the typedef, just access the fields we need by offset
typedef struct {
    uint8_t *data[8];
    int linesize[8];
    // extended_data comes here
    uint8_t **extended_data;
    int width, height;
    int nb_samples;
    int format;
    // more fields but we dont care
} AVFrame;

// counters
static int packet_count = 0;
static int frame_count = 0;
static int replaced_count = 0;

// bad apple info
// found the vid on gh, forgot the link...
static uint8_t* bad_apple_data = NULL;
static size_t bad_apple_size = 0;
static int bad_apple_width = 640;
static int bad_apple_height = 480;
static int bad_apple_frame_size = 0;
static int bad_apple_num_frames = 0;

// function pointers to real functions
static int (*real_avcodec_send_packet)(void*, void*) = NULL;
static int (*real_avcodec_receive_frame)(void*, void*) = NULL;

// hook avcodec_send_packet -> called when compressed video packet is sent to decoder
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

// load bad apple frames
void load_bad_apple_frames() {
    FILE* f = fopen("./badapple/badapple_raw.yuv", "rb");
    if (!f) {
        printf("[ERROR] cant open badapple_raw.yuv\n");
        return;
    }

    fseek(f, 0, SEEK_END);
    bad_apple_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    bad_apple_data = malloc(bad_apple_size);
    if (!bad_apple_data) {
        printf("[ERROR] cant allocate memory for bad apple\n");
        fclose(f);
        return;
    }

    size_t read = fread(bad_apple_data, 1, bad_apple_size, f);
    fclose(f);

    // yuv420p frame size = width * height * 1.5
    bad_apple_frame_size = bad_apple_width * bad_apple_height * 3 / 2;
    bad_apple_num_frames = bad_apple_size / bad_apple_frame_size;

    printf("[BAD APPLE] loaded %zu bytes, %d frames at %dx%d\n",
           bad_apple_size, bad_apple_num_frames, bad_apple_width, bad_apple_height);
}

// replace frame with bad apple frame
void replace_with_bad_apple(AVFrame* frame) {
    if (!frame->data[0] || frame->width <= 0 || frame->height <= 0) {
        return;
    }

    if (!bad_apple_data) {
        printf("[DEBUG] no bad apple data loaded, skipping\n");
        return;
    }

    // cycle through bad apple frames 
    int ba_frame_idx = replaced_count % bad_apple_num_frames;
    uint8_t* ba_frame = bad_apple_data + (ba_frame_idx * bad_apple_frame_size);

    // bad apple is 640x480, source might be different
    // for now just copy what fits
    int copy_width = frame->width < bad_apple_width ? frame->width : bad_apple_width;
    int copy_height = frame->height < bad_apple_height ? frame->height : bad_apple_height;

    // Y plane
    uint8_t* ba_y = ba_frame;
    for (int y = 0; y < copy_height; y++) {
        memcpy(frame->data[0] + y * frame->linesize[0],
               ba_y + y * bad_apple_width,
               copy_width);
    }

    // U plane
    if (frame->data[1]) {
        uint8_t* ba_u = ba_frame + (bad_apple_width * bad_apple_height);
        int uv_copy_width = copy_width / 2;
        int uv_copy_height = copy_height / 2;
        for (int y = 0; y < uv_copy_height; y++) {
            memcpy(frame->data[1] + y * frame->linesize[1],
                   ba_u + y * (bad_apple_width / 2),
                   uv_copy_width);
        }
    }

    // V plane
    if (frame->data[2]) {
        uint8_t* ba_v = ba_frame + (bad_apple_width * bad_apple_height) + (bad_apple_width * bad_apple_height / 4);
        int uv_copy_width = copy_width / 2;
        int uv_copy_height = copy_height / 2;
        for (int y = 0; y < uv_copy_height; y++) {
            memcpy(frame->data[2] + y * frame->linesize[2],
                   ba_v + y * (bad_apple_width / 2),
                   uv_copy_width);
        }
    }
    //YUVVVV
    replaced_count++;
}

// hook avcodec_receive_frame -> called when decoder outputs a frame
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

    load_bad_apple_frames();
}

__attribute__((destructor))
void cleanup() {
    printf("\n=== ffmpeg hook unloading ===\n");
    printf("packets: %d\n", packet_count);
    printf("frames: %d\n", frame_count);
    printf("frames replaced: %d\n", replaced_count);
    printf("===========================\n");

    if (bad_apple_data) {
        free(bad_apple_data);
    }
}
