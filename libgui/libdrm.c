#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>


struct drm_context {


    unsigned int    winWidth;
    unsigned int    winHeight;

    unsigned int    video_w;
    unsigned int    video_h;
};


void *libdrm_init(const char *title, int width, int height)
{
	return NULL;
}


int libdrm_texture_create(void *handle, int pixelformat)
{
	return 0;
}


int libdrm_texture_render(void *handle, uint8_t *frame, int width, int height)
{
	return 0;
}


int libdrm_texture_render_size(void *handle, uint8_t *frame, int size)
{
	return 0;
}

void libdrm_exit(void *handle)
{
	;
}
