

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "x264.h"

struct x264_context {
	x264_t			*x264;	// x264 handle

	x264_param_t	*param;		// enc param

	x264_picture_t	*picture;	//

	int				csp;	// color space

};


void *h264e_init(int width, int height, int frate)
{
	x264_param_t		*param;
    x264_picture_t		*pic_in;
	struct x264_context *ctx;

    ctx = (struct x264_context *)calloc(1, sizeof(struct x264_context));
    if (!ctx) {
        return NULL;
    }

	param = (x264_param_t *)malloc(sizeof(x264_param_t));
    if (!param) {
		free(ctx);
        return NULL;
    }

    pic_in = (x264_picture_t *) malloc(sizeof(x264_picture_t));
    if (!pic_in) {
		free(param);
		free(ctx);
        return NULL;
    }

    ctx->param = param;
    ctx->picture = pic_in;

	// use i420 color space
    ctx->csp = X264_CSP_I420;

	// initial default param
    x264_param_default(param);

    param->i_width = width;
    param->i_height = height;
    param->i_csp = ctx->csp;

    /* Param
    param->i_log_level  = X264_LOG_DEBUG;
    param->i_threads  = X264_SYNC_LOOKAHEAD_AUTO;
    param->i_frame_total = 0;
    param->i_keyint_max = 10;
    param->i_bframe  = 5;
    param->b_open_gop  = 0;
    param->i_bframe_pyramid = 0;
    param->rc.i_qp_constant=0;
    param->rc.i_qp_max=0;
    param->rc.i_qp_min=0;
    param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
	param->rc.i_bitrate = 1024 * 10;//rate 10 kbps
    param->i_fps_den  = 1;
    param->i_fps_num  = 25;
    param->i_timebase_den = param->i_fps_num;
    param->i_timebase_num = param->i_fps_den;
    */

    x264_param_apply_profile(param, x264_profile_names[5]);


	ctx->x264 = x264_encoder_open(param);

    x264_picture_alloc(pic_in, ctx->csp, param->i_width, param->i_height);
	pic_in->img.i_csp = X264_CSP_I420;
	pic_in->img.i_plane = 3;


	return ctx;
}



int h264e_run(int32_t width, int32_t height, int32_t fps)
{

}


int h264e_exit(int32_t width, int32_t height, int32_t fps)
{

}
