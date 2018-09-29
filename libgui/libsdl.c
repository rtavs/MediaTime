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

#include <linux/videodev2.h>



#include "SDL2/SDL.h"
#include "SDL2/SDL_mutex.h"
#include "SDL2/SDL_stdinc.h"



#define LOG_TAG "libsdl"
#include "liblog.h"



struct sdl_context {

    SDL_Window      *window;
    int             mWindowID;

    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_mutex       *mutex;

    SDL_Rect        xx;
    SDL_Rect        dich;


    unsigned int    winWidth;
    unsigned int    winHeight;

    unsigned int    video_w;
    unsigned int    video_h;
};


static int sdl_init_window(struct sdl_context *c, const char *title, int width, int height)
{
    int num_displays, dpy;
    SDL_DisplayMode mode;

    num_displays = SDL_GetNumVideoDisplays();

    for (dpy = 0; dpy < num_displays; dpy++ ) {
        const int num_modes = SDL_GetNumDisplayModes(dpy);
        SDL_Rect rect = { 0, 0, 0, 0 };
        int m;

        SDL_GetDisplayBounds(dpy, &rect);
        printf("%d: \"%s\" (%dx%d, (%d, %d)), %d modes.\n", dpy, SDL_GetDisplayName(dpy), rect.w, rect.h, rect.x, rect.y, num_modes);

        SDL_GetCurrentDisplayMode(dpy, &mode);

        printf("current: fmt=%s w=%d h=%d refresh=%d\n",
                SDL_GetPixelFormatName(mode.format),
                mode.w, mode.h, mode.refresh_rate);

        SDL_GetDesktopDisplayMode(dpy, &mode);
        printf("desktop: fmt=%s w=%d h=%d refresh=%d\n",
                SDL_GetPixelFormatName(mode.format),
                mode.w, mode.h, mode.refresh_rate);

        for (m = 0; m < num_modes; m++) {
            SDL_GetDisplayMode(dpy, m, &mode);
            char prefix[64];
            SDL_snprintf(prefix, sizeof (prefix), "    MODE %d", m);
            printf("%s: fmt=%s w=%d h=%d refresh=%d\n",
                    prefix, SDL_GetPixelFormatName(mode.format),
                    mode.w, mode.h, mode.refresh_rate);
        }
    }

    c->winWidth = mode.w;
    c->winHeight = mode.h;

    if (width > 0 && height > 0) {
        c->winWidth = width;
        c->winHeight = height;
    }

    c->window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            c->winWidth, c->winHeight, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);

    c->mWindowID = SDL_GetWindowID(c->window);

    int a, b;
    SDL_GetWindowSize(c->window, &a, &b);
    printf("SDL Window ID: %d, size: %d, %d\n", c->mWindowID, a,b);

    c->renderer = SDL_CreateRenderer(c->window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);


    /* fill canvass initially with all green */
    SDL_SetRenderDrawColor(c->renderer, 0, 80, 0, 255);
    SDL_RenderClear(c->renderer);
    SDL_RenderPresent(c->renderer);

#if 0
    /* dump all renderinfo and videodriver */
    int i = 0;
    int n = SDL_GetNumRenderDrivers();
    for (i = 0; i < n; i++) {
        SDL_RendererInfo info;
        SDL_GetRenderDriverInfo(i, &info);
        printf("Renderer %s: flags = %d\n",info.name,info.flags);
    }

    n = SDL_GetNumVideoDrivers();
    for (i = 0; i < n; i++) {
        printf("VideoDrivers %s: \n",SDL_GetVideoDriver(i));
    }
#endif

    SDL_RendererInfo info;
    SDL_GetRendererInfo(c->renderer, &info);
    printf("use Renderer %s: max: texture_height %d, texture_width %d\n",info.name,info.max_texture_height,info.max_texture_width);
    printf("use VideoDrivers %s: \n",SDL_GetCurrentVideoDriver());





    return 0;
}

/*
 * init render
 * args:
 *    width -  window width
 *    height - window height
 *
 * returns: render
 */
void *libsdl_init(const char *title, int width, int height)
{
    struct sdl_context *sdl;

    sdl = (struct sdl_context *)calloc(1, sizeof(struct sdl_context));
    if (!sdl) {
        return NULL;
    }

    //videodriver: x11/mir/wayland/dummy
    SDL_setenv("SDL_VIDEODRIVER", "x11", 1);
    //renderdriver: opengl/opengles2/software
    //SDL_setenv("SDL_RENDER_DRIVER", "software", 1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software" );
    //SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1" );

    // Initialise libSDL.
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        ALOGE("Could not initialize SDL: %s.\n", SDL_GetError());
    }
    //SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);


    if (sdl_init_window(sdl, title, width, height) != 0) {
        free(sdl);
        return NULL;
    }

    return sdl;
}



int libsdl_texture_create(void *handle, int pixelformat)
{

    struct sdl_context *c = (struct sdl_context *)handle;

/** //pixelformat
    SDL_PIXELFORMAT_YV12; ok
    SDL_PIXELFORMAT_IYUV; ok default
    SDL_PIXELFORMAT_UYVY;
    SDL_PIXELFORMAT_YUY2;
    SDL_PIXELFORMAT_YVYU;
*/
    c->texture = SDL_CreateTexture(c->renderer, pixelformat,
            SDL_TEXTUREACCESS_STREAMING, c->winWidth, c->winHeight);
    if (!c->texture) {
        ALOGE("Couldn't create texture: %s", SDL_GetError());
        return -1;
    }

    return 0;
}

void libsdl_exit(void *handle)
{
    struct sdl_context *sdl = (struct sdl_context *)handle;

    SDL_DestroyTexture(sdl->texture);
    SDL_DestroyRenderer(sdl->renderer);
    SDL_Quit();
    free(sdl);
}

/*
 * render a frame
 * args:
 *   handle - render handle
 *   frame - pointer to frame data (yuyv format)
 *   width - frame width
 *   height - frame height
 *
 * asserts:
 *   poverlay is not nul
 *   frame is not null
 *
 * returns: error code
 */

int libsdl_texture_render(void *handle, uint8_t *frame, int width, int height)
{
    struct sdl_context *c = (struct sdl_context *)handle;

    SDL_SetRenderDrawColor(c->renderer, 0, 0, 0, 255); /*black*/
    SDL_RenderClear(c->renderer);


    SDL_UpdateTexture(c->texture, NULL, frame,  width);
    SDL_RenderCopy(c->renderer, c->texture, NULL, NULL);
    SDL_RenderPresent(c->renderer);

    return 0;
}

int libsdl_texture_render_size(void *handle, uint8_t *frame, int size)
{
    uint8_t* dst;
    void* pixels;
    int pitch = 0;
    struct sdl_context *c = (struct sdl_context *)handle;


    if (SDL_LockTexture(c->texture, NULL, &pixels, &pitch) < 0) {
        ALOGE( "Couldn't lock texture: %s\n", SDL_GetError());
    }

    dst = (uint8_t*)pixels;
    memcpy(dst, frame, size);

    SDL_UnlockTexture(c->texture);

    SDL_RenderCopy(c->renderer, c->texture, NULL, NULL);
    SDL_RenderPresent(c->renderer);
    return 0;
}
