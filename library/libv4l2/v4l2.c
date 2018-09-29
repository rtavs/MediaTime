/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "v4l2.h"

#define LOG_NDEBUG 1
//#define KV4L2_TRACE 0

#define LOG_TAG "libv4l2"
#include "liblog.h"

#define VIDEODEV_MINOR_MAX 63


#ifdef KV4L2_TRACE
#define KV4L2_IN    FUNC_IN
#define KV4L2_OUT   FUNC_OUT
#else
#define KV4L2_IN() ((void *)0)
#define KV4L2_OUT() ((void *)0)
#endif



static bool __v4l2_check_buf_type(enum v4l2_buf_type type)
{
    bool supported;

    switch (type) {
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
            supported = true;
            break;

        default:
            supported = (type >= V4L2_BUF_TYPE_PRIVATE) ? true : false;
            break;
    }

    return supported;
}

static int __v4l2_open(const char *filename, int oflag, va_list ap)
{
    mode_t mode = 0;
    int fd;

    if (oflag & O_CREAT)
        mode = va_arg(ap, int);

    fd = open(filename, oflag, mode);

    return fd;
}

int v4l2_open(const char *filename, int oflag, ...)
{
    va_list ap;
    int fd;

    KV4L2_IN();

    va_start(ap, oflag);
    fd = __v4l2_open(filename, oflag, ap);
    va_end(ap);

    KV4L2_OUT();

    return fd;
}

int v4l2_open_devname(const char *devname, int oflag, ...)
{
    bool found = false;
    int fd = -1;
    struct stat s;
    va_list ap;
    FILE *stream_fd;
    char filename[64], name[64];
    int minor, size, i = 0;

    KV4L2_IN();

    do {
        if (i > VIDEODEV_MINOR_MAX)
            break;

        /* video device node */
        sprintf(filename, "/dev/video%d", i++);

        /* if the node is video device */
        if ((lstat(filename, &s) == 0) && S_ISCHR(s.st_mode) &&
                ((int)((unsigned short)(s.st_rdev) >> 8) == 81)) {
            minor = (int)((unsigned short)(s.st_rdev & 0x3f));
            ALOGD("try node: %s, minor: %d", filename, minor);
            /* open sysfs entry */
            sprintf(filename, "/sys/class/video4linux/video%d/name", minor);
            stream_fd = fopen(filename, "r");
            if (stream_fd == NULL) {
                ALOGE("failed to open sysfs entry for videodev");
                continue;   /* try next */
            }

            /* read sysfs entry for device name */
            size = (long)fgets(name, sizeof(name), stream_fd);
            fclose(stream_fd);

            /* check read size */
            if (size == 0) {
                ALOGE("failed to read sysfs entry for videodev");
            } else {
                /* matched */
                if (strncmp(name, devname, strlen(devname)) == 0) {
                    ALOGI("node found for device '%s': /dev/video%d", devname, minor);
                    found = true;
                }
            }
        }
    } while (found == false);

    if (found) {
        sprintf(filename, "/dev/video%d", minor);
        va_start(ap, oflag);
        fd = __v4l2_open(filename, oflag, ap);
        va_end(ap);

        if (fd > 0)
            ALOGI("open video device %s", filename);
        else
            ALOGE("failed to open video device %s", filename);
    } else {
        ALOGE("no video device found");
    }

    KV4L2_OUT();

    return fd;
}

int v4l2_close(int fd)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0)
        ALOGE("%s: invalid fd: %d", __func__, fd);
    else
        ret = close(fd);

    KV4L2_OUT();

    return ret;
}

bool v4l2_enum_input(int fd, int index, char *input_name_buf)
{
    int ret = -1;
    struct v4l2_input input;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return false;
    }

    input.index = index;
    ret = ioctl(fd, VIDIOC_ENUMINPUT, &input);
    if (ret) {
        ALOGE("%s: no matching index founds", __func__);
        return false;
    }

    ALOGI("Name of input channel[%d] is %s", input.index, input.name);

    strcpy(input_name_buf, (const char *)input.name);

    KV4L2_OUT();

    return true;
}

int v4l2_s_input(int fd, int index)
{
    int ret = -1;
    struct v4l2_input input;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    input.index = index;

    ret = ioctl(fd, VIDIOC_S_INPUT, &input);
    if (ret){
        ALOGE("failed to ioctl: VIDIOC_S_INPUT (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

bool v4l2_querycap(int fd, struct v4l2_capability *saved, unsigned int need)
{
    struct v4l2_capability cap;
    int ret;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return false;
    }

    if (!(need & V4L2_CAP_VIDEO_CAPTURE) &&
            !(need & V4L2_CAP_VIDEO_CAPTURE_MPLANE) &&
            !(need & V4L2_CAP_VIDEO_OUTPUT) &&
            !(need & V4L2_CAP_VIDEO_OUTPUT_MPLANE) &&
            !(need & V4L2_CAP_VIDEO_OVERLAY)) {
        ALOGE("%s: unsupported capabilities", __func__);
        return false;
    }

    memset(&cap, 0, sizeof(cap));

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_QUERYCAP (%d - %s)", errno, strerror(errno));
        return false;
    }

    *saved = cap;

    if ((need & cap.capabilities) != need) {
        ALOGE("%s: unsupported capabilities", __func__);
        return false;
    }

    KV4L2_OUT();

    return true;
}

bool v4l2_enum_fmt(int fd, enum v4l2_buf_type type, unsigned int fmt)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;

    KV4L2_IN();

    fmtdesc.type = type;
    fmtdesc.index = 0;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        if (fmtdesc.pixelformat == fmt) {
            ALOGE("Passed fmt = '%.4s' found pixel format[%d]: %s", (char*)&fmt, fmtdesc.index, fmtdesc.description);
            found = 1;
            break;
        }

        fmtdesc.index++;
    }

    if (!found) {
        ALOGE("%s: unsupported pixel format %.4s", __func__, (char*)&fmt);
        return false;
    }

    KV4L2_OUT();

    return true;
}

int v4l2_g_fmt(int fd, struct v4l2_format *fmt)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!fmt) {
        ALOGE("%s: fmt is NULL", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(fmt->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_FMT, fmt);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_G_FMT (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

static int __v4l2_s_fmt(int fd, unsigned int request, struct v4l2_format *fmt)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!fmt) {
        ALOGE("%s: fmt is NULL", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(fmt->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    } else {
        ret = ioctl(fd, request, fmt);
        if (ret) {
            if (request == VIDIOC_TRY_FMT)
                ALOGE("failed to ioctl: VIDIOC_TRY_FMT (%d - %s)", errno, strerror(errno));
            else
                ALOGE("failed to ioctl: VIDIOC_S_FMT (%d - %s)", errno, strerror(errno));

            return ret;
        }
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_try_fmt(int fd, struct v4l2_format *fmt)
{
    return __v4l2_s_fmt(fd, VIDIOC_TRY_FMT, fmt);
}

int v4l2_s_fmt(int fd, struct v4l2_format *fmt)
{
    int ret;
    v4l2_format_dump("tryset v4l2 fmt", fmt);
    ret = __v4l2_s_fmt(fd, VIDIOC_S_FMT, fmt);
    v4l2_format_dump("actual v4l2 fmt", fmt);
    return ret;
}

int v4l2_reqbufs(int fd, struct v4l2_requestbuffers *req)
{
    int ret = -1;
    unsigned int count;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!req) {
        ALOGE("%s: req is NULL", __func__);
        return ret;
    }

    if ((req->memory != V4L2_MEMORY_MMAP) &&
        (req->memory != V4L2_MEMORY_USERPTR) &&
        (req->memory != V4L2_MEMORY_DMABUF)) {
        ALOGE("%s: unsupported memory type", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(req->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    count = req->count;

    ret = ioctl(fd, VIDIOC_REQBUFS, req);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_REQBUFS (%d - %s)", ret, strerror(errno));
        return ret;
    }

    if (count != req->count) {
        ALOGW("number of buffers had been changed: %d => %d", count, req->count);
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_querybuf(int fd, struct v4l2_buffer *buf)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!buf) {
        ALOGE("%s: buf is NULL", __func__);
        return ret;
    }

    if ((buf->memory != V4L2_MEMORY_MMAP) &&
        (buf->memory != V4L2_MEMORY_DMABUF)) {
        ALOGE("%s: unsupported memory type", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(buf->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_QUERYBUF, buf);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_QUERYBUF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_qbuf(int fd, struct v4l2_buffer *buf)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!buf) {
        ALOGE("%s: buf is NULL", __func__);
        return ret;
    }

    if ((buf->memory != V4L2_MEMORY_MMAP) &&
        (buf->memory != V4L2_MEMORY_USERPTR) &&
        (buf->memory != V4L2_MEMORY_DMABUF)) {
        ALOGE("%s: unsupported memory type", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(buf->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_QBUF, buf);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_QBUF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_dqbuf(int fd, struct v4l2_buffer *buf)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!buf) {
        ALOGE("%s: buf is NULL", __func__);
        return ret;
    }

    if ((buf->memory != V4L2_MEMORY_MMAP) &&
        (buf->memory != V4L2_MEMORY_USERPTR)&&
        (buf->memory != V4L2_MEMORY_DMABUF)) {
        ALOGE("%s: unsupported memory type", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(buf->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_DQBUF, buf);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_DQBUF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_streamon(int fd, enum v4l2_buf_type type)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (__v4l2_check_buf_type(type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_STREAMON (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_streamoff(int fd, enum v4l2_buf_type type)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (__v4l2_check_buf_type(type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_STREAMOFF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_cropcap(int fd, struct v4l2_cropcap *crop)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!crop) {
        ALOGE("%s: crop is NULL", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(crop->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_CROPCAP, crop);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_CROPCAP (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_g_crop(int fd, struct v4l2_crop *crop)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!crop) {
        ALOGE("%s: crop is NULL", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(crop->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_CROP, crop);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_G_CROP (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_s_crop(int fd, struct v4l2_crop *crop)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (!crop) {
        ALOGE("%s: crop is NULL", __func__);
        return ret;
    }

    if (__v4l2_check_buf_type(crop->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_S_CROP, crop);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_S_CROP (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_g_ctrl(int fd, unsigned int id, int *value)
{
    int ret = -1;
    struct v4l2_control ctrl;

    KV4L2_IN();

    ctrl.id = id;

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_G_CTRL (%d - %s)", errno, strerror(errno));
        return ret;
    }

    *value = ctrl.value;

    KV4L2_OUT();

    return ret;
}

int v4l2_s_ctrl(int fd, unsigned int id, int value)
{
    int ret = -1;
    struct v4l2_control ctrl;

    KV4L2_IN();

    ctrl.id = id;
    ctrl.value = value;

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_S_CTRL (%d)", errno);
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_query_ctrl(int fd, unsigned int id, int value)
{
    int ret = -1;
    struct v4l2_queryctrl ctrl;

    //#define VIDIOC_QUERYCTRL	_IOWR('V', 36, struct v4l2_queryctrl)

}

int v4l2_query_menu()
{
    //#define VIDIOC_QUERYMENU	_IOWR('V', 37, struct v4l2_querymenu)
}


int v4l2_g_parm(int fd, struct v4l2_streamparm *streamparm)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (__v4l2_check_buf_type(streamparm->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_PARM, streamparm);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_G_PARM (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_s_parm(int fd, struct v4l2_streamparm *streamparm)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (__v4l2_check_buf_type(streamparm->type) == false) {
        ALOGE("%s: unsupported buffer type", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_S_PARM, streamparm);
    if (ret) {
        ALOGE("failed to ioctl: VIDIOC_S_PARM (%d - %s)", errno, strerror(errno));
        return ret;
    }

    KV4L2_OUT();

    return ret;
}

int v4l2_g_ext_ctrl(int fd, struct v4l2_ext_controls *ctrl)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (ctrl == NULL) {
        ALOGE("%s: ctrl is NULL", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, ctrl);
    if (ret)
        ALOGE("failed to ioctl: VIDIOC_G_EXT_CTRLS (%d - %s)", errno, strerror(errno));

    KV4L2_OUT();

    return ret;
}

int v4l2_s_ext_ctrl(int fd, struct v4l2_ext_controls *ctrl)
{
    int ret = -1;

    KV4L2_IN();

    if (fd < 0) {
        ALOGE("%s: invalid fd: %d", __func__, fd);
        return ret;
    }

    if (ctrl == NULL) {
        ALOGE("%s: ctrl is NULL", __func__);
        return ret;
    }

    ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, ctrl);
    if (ret)
        ALOGE("failed to ioctl: VIDIOC_S_EXT_CTRLS (%d - %s)", errno, strerror(errno));

    KV4L2_OUT();

    return ret;
}

void v4l2_format_dump(char *str, struct v4l2_format *fmt)
{
    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
        struct v4l2_pix_format_mplane *pix = &fmt->fmt.pix_mp;
        ALOGD("%s: width=%u height=%u format=%.4s bytesperline=%u", str,
            pix->width, pix->height, (char*)&pix->pixelformat,
            pix->plane_fmt[0].bytesperline);
    } else {
        struct v4l2_pix_format *pix = &fmt->fmt.pix;
        ALOGD("%s: width=%u height=%u format=%.4s bytesperline=%u", str,
            pix->width, pix->height, (char*)&pix->pixelformat,
            pix->bytesperline);
    }
}
