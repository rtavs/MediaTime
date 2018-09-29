
#ifndef __LIBSDL_H__
#define __LIBSDL_H__


#include "SDL2/SDL.h"

void *libsdl_init(const char *title, int width, int height);


int libsdl_texture_create(void *handle, int pixelformat);


int libsdl_texture_render(void *handle, uint8_t *frame, int width, int height);
int libsdl_texture_render_size(void *handle, uint8_t *frame, int size);

void libsdl_exit(void *handle);


#endif
