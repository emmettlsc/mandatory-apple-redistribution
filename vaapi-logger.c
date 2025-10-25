#define _GNU_SOURCE
#include <dlfcn.h>
#include <va/va.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// func pointer to original vaRenderPicture
static VAStatus (*real_vaRenderPicture)(VADisplay, VAContextID, VABufferID*, int) = NULL;

// counters for logging
static int render_call_count = 0;
static int total_buffers_seen = 0;

// func for buffer type name
const char* get_buffer_type_name(VABufferType type) {
    switch (type) {
        case VAPictureParameterBufferType: return "PictureParameter";
        case VAIQMatrixBufferType: return "IQMatrix";
        case VABitPlaneBufferType: return "BitPlane";
        case VASliceGroupMapBufferType: return "SliceGroupMap";
        case VASliceParameterBufferType: return "SliceParameter";
        case VASliceDataBufferType: return "SliceData";
        case VAMacroblockParameterBufferType: return "MacroblockParameter";
        case VAResidualDataBufferType: return "ResidualData";
        case VADeblockingParameterBufferType: return "DeblockingParameter";
        case VAImageBufferType: return "Image"; // ??? is this the one we want
        case VAProtectedSliceDataBufferType: return "ProtectedSliceData";
        case VAQMatrixBufferType: return "QMatrix";
        case VAHuffmanTableBufferType: return "HuffmanTable";
        case VAProbabilityBufferType: return "Probability";
        default: return "Unknown";
    }
}

// dump buffer data as hex
void dump_buffer_hex(const void* data, size_t size, const char* prefix) {
    const unsigned char* bytes = (const unsigned char*)data;
    size_t dump_size = size > 64 ? 64 : size;  // Only dump first 64 bytes
    
    printf("%s: ", prefix);
    for (size_t i = 0; i < dump_size; i++) {
        printf("%02x ", bytes[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n%s: ", prefix);
        }
    }
    if (dump_size < size) {
        printf("... (%zu more bytes)", size - dump_size);
    }
    printf("\n");
}

// hooked vaRenderPicture function
VAStatus vaRenderPicture(VADisplay dpy, VAContextID context, 
                        VABufferID *buffers, int num_buffers) {
    
    // get the real function if we still need to
    if (!real_vaRenderPicture) {
        real_vaRenderPicture = dlsym(RTLD_NEXT, "vaRenderPicture");
        if (!real_vaRenderPicture) {
            printf("ERROR: Could not find real vaRenderPicture function!\n");
            return VA_STATUS_ERROR_UNIMPLEMENTED;
        }
    }
    
    // log the call
    render_call_count++;
    total_buffers_seen += num_buffers;
    
    printf("\n=== VA-API HOOK: vaRenderPicture Call #%d ===\n", render_call_count);
    printf("Display: %p, Context: %d, Buffers: %d\n", dpy, context, num_buffers);
    
    // examine each buffer
    for (int i = 0; i < num_buffers; i++) {
        VABufferID buffer_id = buffers[i];
        printf("\n--- Buffer %d/%d (ID: %d) ---\n", i + 1, num_buffers, buffer_id);
        
        // get buffer info
        VABufferType buffer_type;
        unsigned int buffer_size;
        
        VAStatus info_status = vaBufferInfo(dpy, buffer_id, &buffer_type, &buffer_size);
        if (info_status != VA_STATUS_SUCCESS) {
            printf("WARNING: Could not get buffer info (status: %d)\n", info_status);
            continue;
        }
        
        printf("Type: %s (%d), Size: %u bytes\n", 
               get_buffer_type_name(buffer_type), buffer_type, buffer_size);
        
        // map buffer to read its contents
        void* buffer_data = NULL;
        VAStatus map_status = vaMapBuffer(dpy, buffer_id, &buffer_data);
        if (map_status != VA_STATUS_SUCCESS) {
            printf("WARNING: Could not map buffer (status: %d)\n", map_status);
            continue;
        }
        
        if (buffer_data && buffer_size > 0) {
            // Log some interesting details based on buffer type
            if (buffer_type == VASliceDataBufferType) {
                printf("*** SLICE DATA BUFFER (compressed video data) ***\n");
                dump_buffer_hex(buffer_data, buffer_size, "DATA");
                
                // Look for common video codec signatures
                unsigned char* data = (unsigned char*)buffer_data;
                if (buffer_size >= 4) {
                    if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
                        printf("DETECTED: H.264 NAL unit start code\n");
                    } else if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) {
                        printf("DETECTED: H.264 short start code\n");
                    }
                }
            } else if (buffer_type == VASliceParameterBufferType) {
                printf("*** SLICE PARAMETER BUFFER (decode instructions) ***\n");
                dump_buffer_hex(buffer_data, buffer_size, "PARAM");
            } else if (buffer_type == VAPictureParameterBufferType) {
                printf("*** PICTURE PARAMETER BUFFER (frame info) ***\n");
                dump_buffer_hex(buffer_data, buffer_size, "PIC");
            } else {
                printf("Buffer contents (first 32 bytes):\n");
                dump_buffer_hex(buffer_data, buffer_size > 32 ? 32 : buffer_size, "MISC");
            }
        }
        
        // Unmap the buffer
        vaUnmapBuffer(dpy, buffer_id);
    }
    
    printf("\n=== Calling real vaRenderPicture ===\n");
    
    // Call the real function
    VAStatus result = real_vaRenderPicture(dpy, context, buffers, num_buffers);
    
    printf("Real vaRenderPicture returned: %d\n", result);
    printf("=== End of vaRenderPicture Call #%d ===\n\n", render_call_count);
    
    return result;
}

// Optional: Hook other interesting VA-API functions for context
VAStatus vaCreateContext(VADisplay dpy, VAConfigID config_id, int picture_width,
                        int picture_height, int flag, VASurfaceID *render_targets,
                        int num_render_targets, VAContextID *context) {
    
    static VAStatus (*real_vaCreateContext)(VADisplay, VAConfigID, int, int, int, 
                                          VASurfaceID*, int, VAContextID*) = NULL;
    
    if (!real_vaCreateContext) {
        real_vaCreateContext = dlsym(RTLD_NEXT, "vaCreateContext");
    }
    
    printf("\n*** VA-API: Creating decode context %dx%d ***\n", picture_width, picture_height);
    
    VAStatus result = real_vaCreateContext(dpy, config_id, picture_width, picture_height,
                                         flag, render_targets, num_render_targets, context);
    
    if (result == VA_STATUS_SUCCESS) {
        printf("Created context ID: %d\n", *context);
    }
    
    return result;
}

// Constructor function called when library loads
__attribute__((constructor))
void vaapi_logger_init() {
    printf("\n");
    printf("==============================================\n");
    printf("VA-API Logger Hook Initialized!\n");
    printf("PID: %d\n", getpid());
    printf("Process: ");
    
    // Try to get process name
    FILE* comm = fopen("/proc/self/comm", "r");
    if (comm) {
        char proc_name[256];
        if (fgets(proc_name, sizeof(proc_name), comm)) {
            printf("%s", proc_name);  // fgets includes newline
        }
        fclose(comm);
    } else {
        printf("unknown\n");
    }
    
    printf("Hooking vaRenderPicture calls...\n");
    printf("==============================================\n\n");
}

// Destructor function called when library unloads
__attribute__((destructor))
void vaapi_logger_cleanup() {
    printf("\n");
    printf("==============================================\n");
    printf("VA-API Logger Hook Shutting Down\n");
    printf("Total vaRenderPicture calls: %d\n", render_call_count);
    printf("Total buffers processed: %d\n", total_buffers_seen);
    printf("==============================================\n");
}