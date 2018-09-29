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



struct x11_context {


    unsigned int    winWidth;
    unsigned int    winHeight;

    unsigned int    video_w;
    unsigned int    video_h;
};


void *libx11_init(const char *title, int width, int height)
{
	return NULL;
}


int libx11_texture_create(void *handle, int pixelformat)
{
	return 0;
}


int libx11_texture_render(void *handle, uint8_t *frame, int width, int height)
{
	return 0;
}


int libx11_texture_render_size(void *handle, uint8_t *frame, int size)
{
	return 0;
}

void libx11_exit(void *handle)
{
	;
}
