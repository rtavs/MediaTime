
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>


#include "libyuv.h"

#define LOG_TAG "i420"
#include "liblog.h"

#include "i420.h"

int i420_data_size(int height, int stride_y, int stride_u, int stride_v)
{
    /**
    * size_y = stride_y * height;
    * size_u = stride_u * ((height + 1)/2);
    * size_v = stride_v * ((height + 1)/2);
    **/

    return stride_y * height + (stride_u + stride_v) * ((height + 1) / 2);
}


struct i420_buffer *i420_buffer_create(int width, int height,
                            int stride_y, int stride_u, int stride_v)
{
    struct i420_buffer *handle;
    handle = (struct i420_buffer*)calloc(1, sizeof(*handle));
    if (handle == NULL) {
        ALOGE("%s: Failed to allocate i420_buffer", __func__);
        return NULL;
    }

    handle->width = width;
    handle->height = height;
    handle->stride[0] = stride_y;
    handle->stride[1] = stride_u;
    handle->stride[2] = stride_v;

    int size = i420_data_size(height, stride_y, stride_u, stride_v);
    //TODO: use memalign malloc
    handle->data = (uint8_t*)malloc(size);
    memset(handle->data, 0 , size);

    return handle;
}


struct i420_buffer *i420_buffer_create2(int width, int height)
{
    return i420_buffer_create(width, height, width, (width + 1) / 2, (width + 1) / 2);
}



uint8_t* i420_buffer_dataY(struct i420_buffer *handle)
{
  return handle->data;
}

uint8_t* i420_buffer_dataU(struct i420_buffer *handle)
{
  return handle->data + handle->stride[0] * handle->height;
}

uint8_t* i420_buffer_dataV(struct i420_buffer *handle)
{
  return handle->data +
         handle->stride[0] * handle->height +
         handle->stride[1] * ((handle->height + 1) / 2);
}



void i420_buffer_destory(struct i420_buffer *handle)
{
    if (handle->data)
        free(handle->data);

    free(handle);
}




static int i420_plane_print(const uint8_t* buf, int width, int height, int stride, FILE* file)
{
    for (int i = 0; i < height; i++, buf += stride) {
        if (fwrite(buf, 1, width, file) != width)
            return -1;
    }
    return 0;
}


int i420_buffer_print(struct i420_buffer *i420, const char *fname)
{
    FILE *file;

    file = fopen(fname, "ab+");
    if(!file)
        return -1;

    int width = i420->width;
    int height = i420->height;
    int chroma_width = (width + 1) / 2;
    int chroma_height = (height + 1) / 2;

    int stride_y = i420->stride[0];
    int stride_u = i420->stride[1];
    int stride_v = i420->stride[2];

    if (i420_plane_print(i420_buffer_dataY(i420), width, height, stride_y, file) < 0) {
        return -1;
    }

    if (i420_plane_print(i420_buffer_dataU(i420), chroma_width, chroma_height, stride_u, file) < 0) {
        return -1;
    }

    if (i420_plane_print(i420_buffer_dataV(i420), chroma_width, chroma_height, stride_v, file) < 0) {
        return -1;
    }

    fclose(file);
    return 0;
}


size_t calc_buffer_size(uint32_t fourcc, int width, int height)
{
    assert(width >= 0);
    assert(height >= 0);
    size_t buffer_size = 0;

    switch (fourcc) {
        case FOURCC_I420:
        case FOURCC_NV12:
        case FOURCC_NV21:
        case FOURCC_YV12: {
          int half_width = (width + 1) >> 1;
          int half_height = (height + 1) >> 1;
          buffer_size = width * height + half_width * half_height * 2;
          break;
        }
        case FOURCC_R444:
        case FOURCC_RGBP:
        case FOURCC_RGBO:
        case FOURCC_YUY2:
        case FOURCC_UYVY:
          buffer_size = width * height * 2;
          break;
        case FOURCC_24BG:
          buffer_size = width * height * 3;
          break;
        case FOURCC_BGRA:
        case FOURCC_ARGB:
          buffer_size = width * height * 4;
          break;
        default:
          break;
    }
    return buffer_size;
}





int ToI420(const uint8_t* src_frame, uint32_t src_type, size_t src_size,
            int crop_x, int crop_y, int src_width, int src_height,
            int rotation, struct i420_buffer *dst_frame)
{
    int dst_width = dst_frame->width;
    int dst_height = dst_frame->height;
    int stride_y = dst_frame->stride[0];
    int stride_u = dst_frame->stride[1];
    int stride_v = dst_frame->stride[2];

    // LibYuv expects pre-rotation values for dst.
    // Stride values should correspond to the destination values.
    if (rotation == 90 || rotation == 270) {
        dst_width = dst_frame->height;
        dst_height = dst_frame->width;
    }

    return ConvertToI420(
                  src_frame, src_size,
                  i420_buffer_dataY(dst_frame),
                  stride_y,
                  i420_buffer_dataU(dst_frame),
                  stride_u,
                  i420_buffer_dataV(dst_frame),
                  stride_v,
                  crop_x, crop_y,
                  src_width, src_height,
                  dst_width, dst_height,
                  rotation,
                  src_type);
}
