
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "fourcc.h"


#define arraysize(array) ((int)(sizeof(array) / sizeof((array)[0])))

struct FourCCAliasEntry {
  uint32_t alias;
  uint32_t canonical;
};

static const struct FourCCAliasEntry kFourCCAliases[] = {
  {FOURCC_IYUV, FOURCC_I420},
  {FOURCC_YU12, FOURCC_I420},
  {FOURCC_YU16, FOURCC_I422},
  {FOURCC_YU24, FOURCC_I444},
  {FOURCC_YUYV, FOURCC_YUY2},
  {FOURCC_YUVS, FOURCC_YUY2},  // kCMPixelFormat_422YpCbCr8_yuvs
  {FOURCC_HDYC, FOURCC_UYVY},
  {FOURCC_2VUY, FOURCC_UYVY},  // kCMPixelFormat_422YpCbCr8
  {FOURCC_JPEG, FOURCC_MJPG},  // Note: JPEG has DHT while MJPG does not.
  {FOURCC_DMB1, FOURCC_MJPG},
  {FOURCC_BA81, FOURCC_BGGR},  // deprecated.
  {FOURCC_RGB3, FOURCC_RAW },
  {FOURCC_BGR3, FOURCC_24BG},
  {FOURCC_CM32, FOURCC_BGRA},  // kCMPixelFormat_32ARGB
  {FOURCC_CM24, FOURCC_RAW },  // kCMPixelFormat_24RGB
  {FOURCC_L555, FOURCC_RGBO},  // kCMPixelFormat_16LE555
  {FOURCC_L565, FOURCC_RGBP},  // kCMPixelFormat_16LE565
  {FOURCC_5551, FOURCC_RGBO},  // kCMPixelFormat_16LE5551
};
// TODO(fbarchard): Consider mapping kCMPixelFormat_32BGRA to FOURCC_ARGB.
//  {FOURCC_BGRA, FOURCC_ARGB},  // kCMPixelFormat_32BGRA

uint32_t CanonicalFourCC(uint32_t fourcc)
{
  for (uint32_t i = 0; i < arraysize(kFourCCAliases); ++i) {
    if (kFourCCAliases[i].alias == fourcc) {
      return kFourCCAliases[i].canonical;
    }
  }
  // Not an alias, so return it as-is.
  return fourcc;
}


void makeFourCC(uint32_t fourcc, char *s) {
    s[0] = (fourcc >> 24) & 0xff;
    if (s[0]) {
        s[1] = (fourcc >> 16) & 0xff;
        s[2] = (fourcc >> 8) & 0xff;
        s[3] = fourcc & 0xff;
        s[4] = 0;
    } else {
        sprintf(s, "%u", fourcc);
    }
}
