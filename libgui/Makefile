


libgui_src = \
	libgui/libdrm.c \
	libgui/libx11.c \
	libgui/libsdl.c

LOCAL_SRC_FILES += $(libgui_src)


libgui_module += $(patsubst %cpp,%o,$(filter %cpp ,$(libgui_src)))
libgui_module += $(patsubst %c,%o,$(filter %c ,$(libgui_src)))
