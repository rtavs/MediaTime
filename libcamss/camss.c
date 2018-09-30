
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include <pthread.h>

#define LOG_TAG "libcamss"
#include "liblog.h"

#include "libv4l2/v4l2.h"
#include "utils.h"
#include "fourcc.h"
#include "i420.h"
#include "camss.h"

#define V4L2_MODE_PREVIEW           0x0001  /**  For video preview */
#define V4L2_MODE_VIDEO             0x0002  /**  For video record */
#define V4L2_MODE_IMAGE             0x0003  /**  For image capture */


typedef enum _CamssErrorType {
    VIDEO_ERROR_NONE      =  0,
    VIDEO_ERROR_BADPARAM  = -1,
    VIDEO_ERROR_OPENFAIL  = -2,
    VIDEO_ERROR_NOMEM     = -3,
    VIDEO_ERROR_APIFAIL   = -4,
    VIDEO_ERROR_MAPFAIL   = -5,
    VIDEO_ERROR_NOBUFFERS = -6,
    VIDEO_ERROR_POLL      = -7,
} CamssErrorType;


struct camss_param {
    uint32_t           width;       /** preview width */
    uint32_t           height;      /** preview height */
    uint32_t           frate;       /** preview framerate */

    uint32_t           mode;        /** preview/shutter/record mode */
};



struct camss_buffer {
    void            *start;
    size_t          length;
    unsigned int    bytesused;
    int             fd;
};

struct camss_context {
    int                     fd;
    struct v4l2_capability  cap;        /** camera caps */

    int                     buftype;    /** v4l2 buffer type */
    int                     memtype;    /** mmap/userptr/dmabuf etc ..*/

    struct v4l2_pix_format  pixfmt;     /** actual pixel format used, w/h/pixelformat/bytesperline*/

    int                     nbufs;
    struct camss_buffer     *buffers;

    struct i420_buffer      *i420;

    pthread_t               thread;
    int                     quit;
    camss_data_cb           datacb;
};

/** g_parm --> s_parm --> g_parm */
static int camss_setup_framerate(void *handle, uint32_t frate)
{
    struct v4l2_streamparm parm;
    CamssErrorType ret  = VIDEO_ERROR_NONE;
    struct camss_context *cam = (struct camss_context *)handle;

    memset(&parm, 0, sizeof(struct v4l2_streamparm));
    parm.type = cam->buftype;

    if(v4l2_g_parm(cam->fd, &parm) != 0) {
        ALOGE("%s: Failed to g_parm", __func__);
        ret = VIDEO_ERROR_APIFAIL;
        goto exit;
    }

    if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)){
        ALOGE("%s: cam no TIMEPERFRAME support", __func__);
        ret = VIDEO_ERROR_BADPARAM;
        goto exit;
    }

    parm.type = cam->buftype;
    /** V4L2_MODE_IMAGE/V4L2_MODE_VIDEO/V4L2_MODE_PREVIEW; */
    parm.parm.capture.capturemode = V4L2_MODE_PREVIEW; //cam->cammode;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = frate;

    if (v4l2_s_parm(cam->fd, &parm) != 0) {
        ALOGE("%s: cam no TIMEPERFRAME support", __func__);
        ret = VIDEO_ERROR_BADPARAM;
        goto exit;
    }

    int numerator = parm.parm.capture.timeperframe.numerator;
    int denominator = parm.parm.capture.timeperframe.denominator;

    ALOGD("%s: framerate %d, %d, %dfps.", __func__,numerator, denominator, denominator / numerator);

exit:
    return ret;
}



static int camss_setup_hflip(void *handle, int hflip)
{
    int ret;
    int flip;
    struct camss_context *cam = (struct camss_context *)handle;

    ret = v4l2_g_ctrl(cam->fd, V4L2_CID_HFLIP, &flip);

    ret = v4l2_s_ctrl(cam->fd, V4L2_CID_HFLIP, hflip);

    return ret;
}


static int camss_setup_vflip(void *handle, int vflip)
{
    int ret;
    int flip;
    struct camss_context *cam = (struct camss_context *)handle;

    ret = v4l2_g_ctrl(cam->fd, V4L2_CID_VFLIP, &flip);

    ret = v4l2_s_ctrl(cam->fd, V4L2_CID_VFLIP, vflip);

    return ret;
}

static int camss_setup_exposure(void *handle, int exposure)
{
    int ret;
    int exp;
    struct camss_context *cam = (struct camss_context *)handle;

    ret = v4l2_g_ctrl(cam->fd, V4L2_CID_EXPOSURE, &exp);

    ret = v4l2_s_ctrl(cam->fd, V4L2_CID_EXPOSURE, exposure);

    return ret;
}


static int camss_setup_focus(void *handle, int diopt)
{
    int ret;
    struct camss_context *cam = (struct camss_context *)handle;

    ret = v4l2_s_ctrl(cam->fd, V4L2_CID_FOCUS_ABSOLUTE, diopt);
    if (ret < 0) {
        printf("Could not set focus\n");
    }

    return 0;
}





static int camss_try_format(void *handle, struct camss_param *param, uint32_t *fourcc, int *width, int *height)
{
    unsigned int fmts[7];
    struct camss_context *camss = (struct camss_context *)handle;

    // Supported video formats in preferred order.
    /**
     * V4L2_PIX_FMT_NV21:   csi/dvp camera
     * V4L2_PIX_FMT_YUYV:   usb camera
     * V4L2_PIX_FMT_NV12;
     * V4L2_PIX_FMT_MJPEG
     * V4L2_PIX_FMT_JPEG
     * V4L2_PIX_FMT_SGRBG10: raw bayer ??
     ***/

    fmts[0] = V4L2_PIX_FMT_YUYV;
    fmts[1] = V4L2_PIX_FMT_NV21;
    fmts[2] = V4L2_PIX_FMT_NV12;
    fmts[3] = V4L2_PIX_FMT_YUV420;  // YU12
    fmts[4] = V4L2_PIX_FMT_UYVY;
    fmts[5] = V4L2_PIX_FMT_MJPEG;   // FIXME: add jpeg handler for MJPEG
    fmts[6] = V4L2_PIX_FMT_JPEG;

    // emum supported fourcc list from kernel driver
    *fourcc = 0;
    for (int i = 0; i < 7; ++i) {

        if (v4l2_enum_fmt(camss->fd, camss->buftype, fmts[i])) {
            *fourcc = fmts[i];

            // try format and get actual width/height
            struct v4l2_format fmt;
            fmt.type = camss->buftype;
            fmt.fmt.pix.pixelformat = *fourcc;
            fmt.fmt.pix.width = param->width;
            fmt.fmt.pix.height = param->height;
            fmt.fmt.pix.field = V4L2_FIELD_ANY;   //隔行或逐行; V4L2_FIELD_NONE: 逐行扫描

            if (v4l2_try_fmt(camss->fd, &fmt) != 0) {
                ALOGI("== v4l2_try_fmt ==");
                continue;
            }

            // driver surpport this size
            *width  = fmt.fmt.pix.width;
            *height = fmt.fmt.pix.height;

            break;
        }
    }

    return 0;
}

// setup sensor format in kernel driver
static int camss_setup_format(void *handle, struct camss_param *param)
{
    int ret = 0;
    uint32_t fourcc = 0;
    int width = 0, height = 0;
    struct v4l2_format fmt;
    struct camss_context *camss = (struct camss_context *)handle;

    // step 1: emum supported fourcc list from kernel driver
    ret = camss_try_format(camss, param, &fourcc, &width, &height);
    if (ret != 0) {
        return ret;
    }

    ALOGI("preferred camera fourcc '%.4s' for (%dx%d)", (char*)&fourcc, width, height);

    // step 2: set format to kernel driver
    fmt.type = camss->buftype;
    fmt.fmt.pix.pixelformat = fourcc;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;    //隔行或逐行; V4L2_FIELD_NONE: 逐行扫描

    if (v4l2_s_fmt(camss->fd, &fmt) < 0) {
        ALOGE("%s: Failed to s_fmt", __func__);
        ret = VIDEO_ERROR_APIFAIL;
        goto exit;
    }

    /* recheck format form kernel driver */
    if (v4l2_g_fmt(camss->fd, &fmt) != 0) {
        ALOGE("%s: Failed to s_fmt", __func__);
        ret = VIDEO_ERROR_APIFAIL;
        goto exit;
    }

    // step 3: save actual pix format. eg, image w/h/fourcc */
    memcpy(&camss->pixfmt, &fmt.fmt.pix, sizeof(struct v4l2_pix_format));

exit:
    return ret;
}


// setup format/fps

int camss_setup_param(void *handle, struct camss_param *param)
{
    int ret;
    struct camss_context *camss = (struct camss_context *)handle;

    // setup format to kernel driver
    ret = camss_setup_format(camss, param);

    // setup fps
    ret = camss_setup_framerate(camss, param->frate);

    // setup hflip/vflip/  etc..
    ret = camss_setup_hflip(camss, 0);
    ret = camss_setup_vflip(camss, 0);

    return 0;
}


static int camss_request_buffer(void *handle, uint32_t *memtype, int num)
{
    int i;
    struct v4l2_requestbuffers reqb;
    CamssErrorType   ret  = VIDEO_ERROR_NONE;
    struct camss_context *camss = (struct camss_context *)handle;

    /* request buffers */
    memset(&reqb, 0, sizeof(struct v4l2_requestbuffers));
    reqb.count = num;
    reqb.type = camss->buftype;

    /** try mmap/userptr/overlay/dmabuf */
    for (i = V4L2_MEMORY_MMAP; i <= V4L2_MEMORY_DMABUF; i++) {
        reqb.memory = i;
        if (v4l2_reqbufs(camss->fd, &reqb) != 0) {
            ALOGE("Unable to request buffers: %d.", errno);
            continue;
        }
        *memtype = reqb.memory;
        break;
    }

    if ( (i > V4L2_MEMORY_DMABUF) || ((int)reqb.count != num)) {
        ALOGE("%s: asked for %d, got %d", __func__, num, reqb.count);
        ret = VIDEO_ERROR_NOMEM;
    }

    ALOGD("%s: got %d buffers use method %d", __func__, num, *memtype);
    return ret;
}


static int mmap_alloc_buffers(void *handle, int num)
{
    struct v4l2_buffer buf;
    struct camss_buffer *buffer;
    CamssErrorType   ret  = VIDEO_ERROR_NONE;
    struct camss_context *cam = (struct camss_context *)handle;

    /* map the buffers */
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = cam->buftype;
    buf.memory = cam->memtype;


    for (int i = 0; i < num; i++) {

        buf.index = i;
        //把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
        if (v4l2_querybuf(cam->fd, &buf) != 0) {
            ALOGE("Unable to query buffer (%d).", errno);
            ret = VIDEO_ERROR_APIFAIL;
            goto bail;
        }

        ALOGD("length: %u offset: %u", buf.length, buf.m.offset);

        buffer = &cam->buffers[i];

        buffer->length = buf.length;
        buffer->start = mmap(NULL, buf.length,
                            PROT_READ | PROT_WRITE, MAP_SHARED,
                            cam->fd, buf.m.offset);
        if (buffer->start == MAP_FAILED) {
            ALOGE("Unable to map buffer (%d)", errno);
            ret = VIDEO_ERROR_MAPFAIL;
            goto bail;
        }

        ALOGD("Buffer mapped at address %p.", buffer->start);

        // enqueue this buffer
        if (v4l2_qbuf(cam->fd, &buf) != 0) {
            ALOGE("Unable to queue buffer (%d).", errno);
            ret = VIDEO_ERROR_APIFAIL;
            goto bail;
        }

    }
    return ret;


bail:

    for (int i = 0; i < cam->nbufs; i++) {
        buffer = &cam->buffers[i];
        if (buffer->start == MAP_FAILED) {
            buffer->start = NULL;
            break;
        }

        munmap(buffer->start, buffer->length);
    }

    return ret;
}


static int userptr_alloc_buffers(void *handle, int num)
{
    int i;
    struct camss_buffer *buffer;
    struct v4l2_buffer buf;
    CamssErrorType   ret  = VIDEO_ERROR_NONE;
    struct camss_context *cam = (struct camss_context *)handle;

    /* map the buffers */
    memset(&buf, 0, sizeof(struct v4l2_buffer));
    buf.type = cam->buftype;
    buf.memory = cam->memtype;


    for (i = 0; i < num; i++) {

        buf.index = i;
        buffer = &cam->buffers[i];

        buffer->length = cam->pixfmt.sizeimage;
        buffer->start = malloc(buffer->length);

        if (buffer->start == NULL) {
            ALOGE("Unable to alloc userptr buffer (%d)", errno);
            ret = VIDEO_ERROR_MAPFAIL;
            goto bail;
        }

        ALOGD("Buffer alloced at address %p.", buffer->start);

        // enqueue this buffer
        buf.m.userptr = (unsigned long)buffer->start;
        buf.length = buffer->length;
        if (v4l2_qbuf(cam->fd, &buf) != 0) {
            ALOGE("Unable to queue buffer (%d).", errno);
            ret = VIDEO_ERROR_APIFAIL;
            goto bail;
        }

    }

    return ret;

bail:

    for (i = 0; i < cam->nbufs; i++) {
        buffer = &cam->buffers[i];
        free(buffer->start);
    }

    return ret;
}


static int dmabuf_alloc_buffers(void *handle, int num)
{
#if 0
    int i;
    struct camss_buffer *buffer;
    struct v4l2_exportbuffer expbuf;
    CamssErrorType   ret  = VIDEO_ERROR_NONE;
    struct camss_context *cam = (struct camss_context *)handle;

    /* export the buffers */
    memset(&expbuf, 0, sizeof(struct v4l2_exportbuffer));
    expbuf.type = cam->buftype;
    expbuf.flags = O_CLOEXEC;

    for (i = 0; i < num; i++) {
        expbuf.index = i;

        if (v4l2_export_buffer (cam->fd, &expbuf) < 0) {
            ALOGE("get dma buf(%d) failed", i);
            ret = VIDEO_ERROR_MAPFAIL;
            goto bail;
        }

        buffer = &cam->buffers[i];

        buffer->fd = expbuf.fd;
        //buffer->length = format.fmt.pix.sizeimage;

        // enqueue this buffer
        buf.m.fd = buffer->fd;
        if (v4l2_qbuf(cam->fd, &buf) != 0) {
            ALOGE("Unable to queue buffer (%d).", errno);
            ret = VIDEO_ERROR_APIFAIL;
            goto bail;
        }

    }

bail:
    return ret;
#else
    (void)&num;
    (void)&handle;
    return VIDEO_ERROR_NONE;
#endif
}

/*
 * 1. reauest kernel buffers
 * 2. mmap/alloc/export user buffers
 */
static int camss_alloc_buffers(void *handle, int count)
{
    int ret;
    uint32_t memtype = 0;
    struct camss_context *camss = (struct camss_context *)handle;

    // assign mem type and count
    ret = camss_request_buffer(camss, &memtype, count);
    if (ret != VIDEO_ERROR_NONE) {
        ALOGE("try memtype error");
        return -1;
    }

    camss->memtype = memtype;
    camss->nbufs = count;

    /** alloc buffers */
    camss->buffers = calloc(count, sizeof(struct camss_buffer));
    if (camss->buffers == NULL) {
        ALOGE("%s: Failed to allocate cam_buffer", __func__);
        ret = VIDEO_ERROR_NOMEM;
        goto bail;
    }

    /** map/export and enqueue buffers */
    if (memtype == V4L2_MEMORY_MMAP) {
        ret = mmap_alloc_buffers(camss, camss->nbufs);
    } else if (memtype == V4L2_MEMORY_USERPTR) {
        ret = userptr_alloc_buffers(camss, camss->nbufs);
    } else if (memtype == V4L2_MEMORY_DMABUF) {
        ret = dmabuf_alloc_buffers(camss, camss->nbufs);
    }

    return ret;

bail:
    if (camss->buffers != NULL) {
        free(camss->buffers);
    }
    return VIDEO_ERROR_NOMEM;
}


static int camss_free_buffers(void *handle)
{
    int i;
    struct v4l2_requestbuffers reqb;
    struct camss_context *camss = (struct camss_context *)handle;
    struct camss_buffer *buffers = camss->buffers;

    // TODO: need dequeue first ?
    for (i = 0; i < camss->nbufs; ++i) {
        if (camss->memtype == V4L2_MEMORY_MMAP) {
            munmap(buffers[i].start, buffers[i].length);
        } else if (camss->memtype == V4L2_MEMORY_USERPTR) {
            free(buffers[i].start);
        } else if (camss->memtype == V4L2_MEMORY_DMABUF) {
            ;/** TODO */
        }
    }

    // release buffers:  request count = 0 for clean-up
    memset(&reqb, 0, sizeof(struct v4l2_requestbuffers));
    reqb.count = 0;
    reqb.type = camss->buftype;
    reqb.memory = camss->memtype;
    if (v4l2_reqbufs(camss->fd, &reqb) != 0) {
        ALOGE("Unable to request buffers: %d.", errno);
    }

    if (buffers != NULL) {
        free(buffers);
    }

    return 0;
}

static int camss_frame_avail(void *handle)
{
    int ret;
    fd_set rdset;
    struct timeval timeout;

    struct camss_context *cam = (struct camss_context *)handle;

    FD_ZERO(&rdset);
    FD_SET(cam->fd, &rdset);

    timeout.tv_sec = 1; /* 1 sec timeout*/
    timeout.tv_usec = 0;

    /* select - wait for data or timeout*/
    ret = select(cam->fd + 1, &rdset, NULL, NULL, &timeout);
    if (ret <= 0) {
        ALOGE("Could not grab image (select error/timeout),try again...");
        return ret;
    }


    if ((ret > 0) && (FD_ISSET(cam->fd, &rdset)))
        return 1;

    return -1;
}



static int camss_dump_raw(const char *fname, struct camss_buffer *cambuf)
{
    uint64_t start;
    uint64_t stop;

    start = nowMs();

    FILE *yuv;
    yuv = fopen(fname, "ab+");
    fwrite(cambuf->start, cambuf->bytesused, 1, yuv);
    fclose(yuv);

    stop = nowMs();

    ALOGD("save %s used %lu ms", fname, stop - start);

    return 0;
}


// push or pull ??
static void camss_thread(void *data)
{
    int ret = 0;

    static int64_t lastMs = 0;
    int target_w, target_h, stride_y, stride_uv;
    struct camss_context *camss = (struct camss_context *)data;

    const int32_t width = camss->pixfmt.width;
    const int32_t height = camss->pixfmt.height;
    uint32_t src_type = camss->pixfmt.pixelformat;


    if (src_type == V4L2_PIX_FMT_YUYV) {
        target_w = width;
        target_h = height;
        stride_y = width;
        stride_uv = (width + 1) / 2;
        camss->i420 = i420_buffer_create(target_w, target_h, stride_y, stride_uv, stride_uv);
        if (camss->i420 == NULL) {
            goto bail;
        }
    }


    while(!camss->quit) {

        if (camss_frame_avail(camss) > 0) {
            struct v4l2_buffer buf;

            memset (&buf, 0, sizeof(buf));
            buf.type = camss->buftype;
            buf.memory = camss->memtype;

            if (v4l2_dqbuf(camss->fd, &buf) != 0) {
                ALOGE("v4l2_qbuf error");
            }

            //ALOGD("index=%02d, seq=%d, timestamp=%d%06d", buf.index, buf.sequence, (int)buf.timestamp.tv_sec, (int)buf.timestamp.tv_usec);
            // add watermark
            // 通知 preview线程 显示预览
            // 如果需要拍照  则通知picture线程 进行拍照
            // 如果需要录像 则通知record线程? 编码？

            struct camss_buffer *cambuf = &camss->buffers[buf.index];
            cambuf->bytesused = buf.bytesused;

            if (src_type == V4L2_PIX_FMT_YUYV) {


            #if 0
                ALOGD("kernel fourcc '%.4s' stride %d-%d-%d bytesused=%d", (char*)&src_type, stride_y,stride_uv,stride_uv, cambuf->bytesused);
                //vlc -vvv --demux rawvideo --rawvid-fps 30 --rawvid-width 640 --rawvid-height 480 --rawvid-chroma YUYV 640x480.yuyv
                char filename[64] = {0};
                sprintf(filename, "./%dx%d.yuyv", width, height);
                camss_dump_raw(filename, cambuf);
            #endif

                ret = ToI420(cambuf->start, CanonicalFourCC(src_type), cambuf->bytesused, 0, 0, width, height, 0, camss->i420);

            }

            if (camss->datacb) {
                ret = camss->datacb(camss, camss->i420);
            }

            if (v4l2_qbuf(camss->fd, &buf) != 0) {
                ALOGE("v4l2_qbuf error");
            }
        }
    }

bail:
    return;
}

static int camss_setup(void *handle, int width, int height, int frate)
{
    int ret;
    struct camss_param param;
    struct camss_context *camss = (struct camss_context *)handle;

    param.width  = width;
    param.height = height;
    param.frate = frate;

    // setup format to kernel driver
    ret = camss_setup_param(camss, &param);

    // alloc buffer
    ret = camss_alloc_buffers(camss, 8);

    // start thread
    if(pthread_create(&camss->thread, NULL, camss_thread, camss)) {
        ALOGE("callVoid: failed to create a cam thread");
        goto bail;
    }

    return 0;

bail:
    camss_free_buffers(camss);
    return -1;
}

/**************************************************************/
/*******  Camera Sensor low-level API                         */
/**************************************************************/

void *camss_open(const char *devname, int width, int height, int frate)
{
    int ret;
    struct camss_context *camss;
    int needCaps = (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING);

    camss = (struct camss_context*)calloc(1, sizeof(struct camss_context));
    if (camss == NULL) {
        ALOGE("%s: Failed to allocate camera context buffer", __func__);
        goto bail;
    }

    camss->fd = v4l2_open_devname(devname, O_RDWR | O_NONBLOCK, 0);
    if (camss->fd  < 0) {
        ALOGE("%s: Failed to open camera device", __func__);
        goto bail;
    }

    if (!v4l2_querycap(camss->fd, &camss->cap, needCaps)) {
        ALOGE("%s: Failed to querycap", __func__);
        goto bail;
    }

    uint32_t caps = camss->cap.capabilities;

    // assign buftype
    if (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        camss->buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    } else if (caps & V4L2_CAP_VIDEO_CAPTURE) {
        camss->buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    } else if (caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE) {
        camss->buftype = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    } else if (caps & V4L2_CAP_VIDEO_OUTPUT) {
        camss->buftype = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    } else {
        ALOGI("neither video capture nor video output supported.");
    }


    // enum all v4l2_input and set input index ??
#if 0 // taken kernel hexium_gemini.c
        static struct v4l2_input hexium_inputs[HEXIUM_INPUTS] = {
            { 0, "CVBS 1",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 1, "CVBS 2",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 2, "CVBS 3",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 3, "CVBS 4",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 4, "CVBS 5",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 5, "CVBS 6",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 6, "Y/C 1",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 7, "Y/C 2",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
            { 8, "Y/C 3",	V4L2_INPUT_TYPE_CAMERA,	0, 0, V4L2_STD_ALL, 0, V4L2_IN_CAP_STD },
        };
#endif

    // multi channel cvbs or hdmi need  select input, 0 for default;
    {
        int index = 0;
        if (v4l2_s_input(camss->fd, index) != 0) {
            ALOGE("%s: v4l2_s_input error!",__func__);
            goto bail;
        }
    }

    if (camss_setup(camss, width, height, frate) != 0) {
        ALOGE("%s: v4l2_s_input error!",__func__);
        goto bail;
    }

    return camss;

bail:
    close(camss->fd);
    free(camss);
    return NULL;
}

int camss_install_cb(void *handle, camss_data_cb callback)
{
    struct camss_context *camss = (struct camss_context *)handle;

    camss->datacb = callback;
    return 0;
}

int camss_start(void *handle)
{
    enum v4l2_buf_type type;
    struct camss_context *camss = (struct camss_context *)handle;

    /** stream_on */
    type = camss->buftype;
    if (v4l2_streamon(camss->fd, type) != 0) {
        ALOGE("%s: Failed to stream on", __func__);
        return -1;
    }
    return 0;
}

int camss_stop(void *handle)
{
    enum v4l2_buf_type type;
    struct camss_context *camss = (struct camss_context *)handle;

    camss->quit = 1;
    pthread_join(camss->thread, 0);

    /** stream off */
    type = camss->buftype;
    if (v4l2_streamoff(camss->fd, type) != 0) {
        ALOGE("%s: Failed to stream off", __func__);
        return -1;
    }
    return 0;
}

int camss_close(void *handle)
{
    int ret;
    struct camss_context *camss = (struct camss_context *)handle;

    ret = camss_free_buffers(camss);

    v4l2_close(camss->fd);

    free(camss);

    return 0;
}
