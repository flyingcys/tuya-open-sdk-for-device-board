#编译所用变量
COMPILE_PREX ?=

CC = $(COMPILE_PREX)gcc

# 模块源代码
LOCAL_SRC_FILES := $(shell find tuyaos_adapter/src -name "*.c" -o -name "*.cpp" -o -name "*.cc")
LOCAL_SRC_FILES += $(shell find tuyaos_adapter/include -name "*.c" -o -name "*.cpp" -o -name "*.cc")

# 模块内部CFLAGS：仅供本组件使用
LOCAL_CFLAGS := $(addprefix -I, $(shell find tuyaos_adapter -type d)) 
LOCAL_CFLAGS += $(addprefix -I, $(shell find tuyaos_adapter/include -type d))
LOCAL_CFLAGS += $(foreach base_dir, $(HEADER_DIR), $(addprefix -I , $(base_dir)))

LOCAL_CFLAGS += -fsanitize=address -fno-omit-frame-pointer -g

LOCAL_OUTPUT_DIR = $(OUTPUT_DIR)/$(EXAMPLE_NAME)_$(EXAMPLE_VER)
LOCAL_OUTPUT_DIR_OBJS = $(LOCAL_OUTPUT_DIR)/.objs

#user的obj命令
LOCAL_OBJS = $(addsuffix .o, $(LOCAL_SRC_FILES))
#user的实际obj地址
LOCAL_OBJS_OUT =  $(addprefix $(LOCAL_OUTPUT_DIR_OBJS)/, $(LOCAL_OBJS))
DEP_FILES = $(patsubst %.o,%.d,$(LOCAL_OBJS_OUT))

$(LOCAL_OUTPUT_DIR_OBJS)/%.c.o: %.c
	@echo "CC $<"
	@mkdir -p $(dir $@);
	$(CC) $(LOCAL_CFLAGS) -o $@ -c -MMD $<

-include $(DEP_FILES)

#库文件路径
TUYAOS_LIB_DIR = $(LIBS_DIR)

#链接选项
LINKFLAGS = -lasan -L$(TUYAOS_LIB_DIR) $(addprefix -l, $(LIBS)) -pthread -lm  -lsystemd
all: app_excute
app_excute: $(LOCAL_OBJS_OUT)
	@mkdir -p $(LOCAL_OUTPUT_DIR_OBJS)
	$(CC) $(LOCAL_OBJS_OUT) $(LINKFLAGS) -o $(LOCAL_OUTPUT_DIR)/$(EXAMPLE_NAME)_$(EXAMPLE_VER)
	@echo "Build APP Finish"

.PHONY: all clean SHOWARGS app_excute pack
clean:
	rm -rf $(LOCAL_OUTPUT_DIR)