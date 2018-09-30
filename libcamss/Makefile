


libcamss_src = \
	libcamss/camss.c \
	libcamss/fourcc.c \
	libcamss/i420.c


LOCAL_SRC_FILES += $(libcamss_src)


libcamss_module += $(patsubst %cpp,%o,$(filter %cpp ,$(libcamss_src)))
libcamss_module += $(patsubst %c,%o,$(filter %c ,$(libcamss_src)))
