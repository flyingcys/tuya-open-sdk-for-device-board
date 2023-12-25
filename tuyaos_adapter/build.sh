#!/bin/sh
APP_BIN_NAME=$1
APP_VERSION=$2
TARGET_PLATFORM=$3
APP_PATH=$4
USER_CMD=$5

echo APP_PATH=$APP_PATH
echo APP_NAME=$APP_BIN_NAME
echo APP_VERSION=$APP_VERSION
echo USER_CMD=$USER_CMD

#****************************************************************************************
# 在下下方添加您的编译、连接、打包命令
# 注意：
#   您不需要对tuyaos_adapter进行编译，tuya提供内容都将编译完成并存放于根目录的./libs目录之下，
#	您只需要按照顺序进行连接即可，链接的顺序为：
#   -l$(APP_BIN_NAME) -ltuyaapp_components -ltuyaapp_drivers -ltuyaos -ltuyaos_adapter
#****************************************************************************************

[ -z $APP_BIN_NAME ] && fatal "no app path!"
[ -z $APP_PATH ] && fatal "no app name!"
[ -z $APP_VERSION ] && fatal "no version!"

make APP_NAME=$APP_BIN_NAME USER_SW_VER=$APP_VERSION APP_PATH=$APP_PATH USER_CMD=$USER_CMD
