


libenc_src = \
	libenc/libhva.c \
	libenc/libvpx.c \
	libenc/libx264.c \
	libenc/libx265.c \
	libenc/h264e.c \
	libenc/h265e.c \

LOCAL_SRC_FILES += $(libenc_src)


libenc_module += $(patsubst %cpp,%o,$(filter %cpp ,$(libenc_src)))
libenc_module += $(patsubst %c,%o,$(filter %c ,$(libenc_src)))
