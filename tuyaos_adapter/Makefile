ROOT_DIR ?= $(abspath ../../../)
-include ../../build/build_param

#编译所用变量
OUTPUT_DIR = $(ROOT_DIR)/$(APP_PATH)/output/$(APP_NAME)_$(APP_VER)

#库文件路径
TUYAOS_LIB_DIR = $(ROOT_DIR)/libs

#链接选项
LINKFLAGS = -L$(TUYAOS_LIB_DIR) -Xlinker "-(" -l$(APP_NAME) -ltuyaapp_components -ltuyaapp_drivers -ltuyaos -ltuyaos_adapter -Xlinker "-)" -pthread -lm  -lsystemd  

all: app_excute	
app_excute: 
	@mkdir -p $(OUTPUT_DIR)
	$(CC) $(LINKFLAGS) -o $(OUTPUT_DIR)/$(APP_NAME) 
	@echo "Build APP Finish"

.PHONY: all clean SHOWARGS app_excute pack
clean:
	rm -rf $(OUTPUT_DIR)
