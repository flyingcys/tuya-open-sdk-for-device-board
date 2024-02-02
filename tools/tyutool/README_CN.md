# TuyaOS Flash下载工具
[English](https://github.com/tuya/T2/blob/master/README.md) | 简体中文

TuyaOS Flash下载工具 tuya uart tool 是涂鸦自研的开发阶段调试使用的串口下载工具，统一便捷的操作模式，可支持多种芯片，支持命令行和图形化界面，支持 Linux、Windows、macOS 等操作系统。

## 支持芯片型号

| 开发框架 | 类型        | 开发平台     | 下载方式     | 下载文件类型 | 支持波特率 | 功能 |
| -------- | ----------------- | --------- | ------------ | ------------ | ---------- |  ---------- | 
| 联网单品 | WiFi BLE 双模设备 | BK7231N    | 串口         | QIO.bin      | 921600/1500000/2000000     | 写/读 |
|          | WiFi BLE 双模设备 | T2         | 串口         | QIO.bin      | 921600/1500000/2000000     | 写/读 |
|          | WiFi BLE 双模设备 | RTL8720CF   | 串口         | QIO.bin      | 2000000    | 写 |
|          | WiFi BLE 双模设备 | RTL8720CM    | 串口         | QIO.bin      | 2000000    | 写 |

更多芯片适配开发中。

> 波特率可能与使用的 USB-TTL 转换芯片有关，可根据实际情况选择。


## 命令行使用

### 烧录
通过 `write` 命令写入文件至 Flash 。

示例：`./cli write -d BK7231N -p /dev/ttyACM0 -b 2000000 -f ./hello_world.bin`

```shell
$ ./cli write -h
Options:
  -d, --device [BK7231N|T2|RTL8720CF|RTL8720CM]
                                  Soc name  [required]
  -p, --port TEXT                 Target port  [required]
  -b, --baud INTEGER              Uart baud rate
  -s, --start <LAMBDA>            Flash address of start
  -f, --file TEXT                 file of BIN  [required]
  --tqdm                          Progress use tqdm
  -h, --help                      Show this message and exit.
```


### 读取
通过 `read` 命令读取当前芯片内指定地址和大小内容。

示例：`./cli read -d BK7231N -p /dev/ttyACM0 -b 2000000 -s 0 -l 0x200000 -f read.bin`

```shell
$ ./cli.py read -h
Options:
  -d, --device [BK7231N|T2|RTL8720CF|RTL8720CM]
                                  Soc name  [required]
  -p, --port TEXT                 Target port  [required]
  -b, --baud INTEGER              Uart baud rate
  -s, --start <LAMBDA>            Flash address of start
  -l, --length <LAMBDA>           Flash read length [0x200000]
  -f, --file TEXT                 file of BIN  [required]
  --tqdm                          Progress use tqdm
  -h, --help                      Show this message and exit.
```

> 注：是否支持 read 功能需根据当前芯片支持情况而定，并非所有芯片都支持 read 功能。

### 升级
通过 `upgrade` 命令自动升级当前程序。

示例：`./cli upgrade`
```shell
$ ./cli upgrade
[INFO]: tyut_logger init done.
[INFO]: Run Tuya Uart Tool.
[INFO]: Upgrading...
[INFO]: [1.2.0] is last version.
```

## 注意

1. Linux 操作系统下，默认串口需要管理员权限，需要执行 `sudo usermod -aG dialout $USER` 并重启Linux 系统，才能正常运行串口固件下载。
2. Linux 下可在命令行下获取串口号：
```shell
$ ls /dev/tty*
/dev/tty    /dev/tty16  /dev/tty24  /dev/tty32  /dev/tty40  /dev/tty49  /dev/tty57  /dev/tty8       /dev/ttyS12  /dev/ttyS20  /dev/ttyS29  /dev/ttyS9
/dev/tty0   /dev/tty17  /dev/tty25  /dev/tty33  /dev/tty41  /dev/tty5   /dev/tty58  /dev/tty9       /dev/ttyS13  /dev/ttyS21  /dev/ttyS3
/dev/tty1   /dev/tty18  /dev/tty26  /dev/tty34  /dev/tty42  /dev/tty50  /dev/tty59  /dev/ttyACM0    /dev/ttyS14  /dev/ttyS22  /dev/ttyS30
/dev/tty10  /dev/tty19  /dev/tty27  /dev/tty35  /dev/tty43  /dev/tty51  /dev/tty6   /dev/ttyACM1    /dev/ttyS15  /dev/ttyS23  /dev/ttyS31
/dev/tty11  /dev/tty2   /dev/tty28  /dev/tty36  /dev/tty44  /dev/tty52  /dev/tty60  /dev/ttyprintk  /dev/ttyS16  /dev/ttyS24  /dev/ttyS4
/dev/tty12  /dev/tty20  /dev/tty29  /dev/tty37  /dev/tty45  /dev/tty53  /dev/tty61  /dev/ttyS0      /dev/ttyS17  /dev/ttyS25  /dev/ttyS5
/dev/tty13  /dev/tty21  /dev/tty3   /dev/tty38  /dev/tty46  /dev/tty54  /dev/tty62  /dev/ttyS1      /dev/ttyS18  /dev/ttyS26  /dev/ttyS6
/dev/tty14  /dev/tty22  /dev/tty30  /dev/tty39  /dev/tty47  /dev/tty55  /dev/tty63  /dev/ttyS10     /dev/ttyS19  /dev/ttyS27  /dev/ttyS7
/dev/tty15  /dev/tty23  /dev/tty31  /dev/tty4   /dev/tty48  /dev/tty56  /dev/tty7   /dev/ttyS11     /dev/ttyS2   /dev/ttyS28  /dev/ttyS8  
```

上面的 `/dev/ttyACM0` 和 `/dev/ttyACM1` 就是 USB-TTL 的设备，也有的设备叫 `/dev/ttyUSB0`，根据实际使用情况选择。

3. Windows 操作系统下，运行 `cli.exe` 程序。

4. TuyaOS Flash下载工具主要用于开发阶段调试烧录使用，批量生产阶段推荐使用 `涂鸦生产解决方案`，详情点击 [https://developer.tuya.com/cn/docs/iot/SCJJ-01?id=Kcpv15oujgz00](https://developer.tuya.com/cn/docs/iot/SCJJ-01?id=Kcpv15oujgz00)