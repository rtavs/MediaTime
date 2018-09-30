#ifndef __I420_H__
#define __I420_H__

#ifdef __cplusplus
extern "C" {
#endif



struct i420_buffer {
    int     width;
    int     height;
    int     stride[3];  /** y-u-v: 0-1-2 */
    uint8_t *data;
};



struct i420_buffer *i420_buffer_create(int width, int height,
                            int stride_y, int stride_u, int stride_v);

void i420_buffer_destory(struct i420_buffer *handle);


uint8_t* i420_buffer_dataY(struct i420_buffer *handle);

uint8_t* i420_buffer_dataU(struct i420_buffer *handle);

uint8_t* i420_buffer_dataV(struct i420_buffer *handle);

int i420_data_size(int height, int stride_y, int stride_u, int stride_v);


int i420_buffer_print(struct i420_buffer *i420, const char *fname);





int ToI420(const uint8_t* src_frame, uint32_t src_type, size_t src_size,
            int crop_x, int crop_y, int src_width, int src_height,
            int rotation, struct i420_buffer *dst_frame);





#ifdef __cplusplus
}
#endif

#endif /* __I420BUFFER_H__ */
