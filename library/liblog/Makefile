


liblog_src = \
	library/liblog/liblog.c


LOCAL_SRC_FILES += $(liblog_src)


liblog_module += $(patsubst %cpp,%o,$(filter %cpp ,$(liblog_src)))
liblog_module += $(patsubst %c,%o,$(filter %c ,$(liblog_src)))
