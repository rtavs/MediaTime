




libv4l2_src = \
	library/libv4l2/mdev.c \
	library/libv4l2/v4l2.c \
	library/libv4l2/subdev.c

LOCAL_SRC_FILES += $(libv4l2_src)


libv4l2_module += $(patsubst %cpp,%o,$(filter %cpp ,$(libv4l2_src)))
libv4l2_module += $(patsubst %c,%o,$(filter %c ,$(libv4l2_src)))
