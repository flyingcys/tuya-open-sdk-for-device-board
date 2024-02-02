# TuyaOS Flash Download Tool
English | [简体中文](README_CN.md)

The TuyaOS Flash download tool tuya uart tool is a self-developed serial port download tool used for development-stage debugging by Tuya. It offers a unified and convenient operating mode, supports multiple chips, and provides command-line and graphical user interface support for Linux, Windows, macOS, and other operating systems.

## Supported Chip Models

| Development Framework | Type       | Development Platform | Download method | Download file type | Supported Baud Rate | Function |
| --------------------- | ---------- | -------------------- | ---------------- | ------------------ | -------------------- | --------- |
| Networking single product | WiFi && BLE dual mode device | BK7231N | Serial port | QIO.bin | 921600/1500000/2000000 | Write/Read |
|                         | WiFi && BLE dual mode device | T2      | Serial port | QIO.bin | 921600/1500000/2000000 | Write/Read |
|                         | WiFi && BLE dual mode device | RTL8720CF | Serial port | QIO.bin | 2000000                | Write       |
|                         | WiFi && BLE dual mode device | RTL8720CM | Serial port | QIO.bin | 2000000                | Write       |

More chip adaptation is in progress.

> The baud rate may depend on the USB-TTL conversion chip in use and should be chosen accordingly.

## Command-line Usage

### Burning
Use the `write` command to write the file to Flash.

Example: `./cli write -d BK7231N -p /dev/ttyACM0 -b 2000000 -f ./hello_world.bin`

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

### Reading
Use the `read` command to read the specified address and size of the current chip's contents.

Example: `./cli read -d BK7231N -p /dev/ttyACM0 -b 2000000 -s 0 -l 0x200000 -f read.bin`

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

> Note: Whether the read function is supported depends on the current chip; not all chips support the read function.

### Upgrade
Uses the `upgrade` command to automatically upgrade the current program.

Example: `./cli upgrade`
```shell
$ ./cli upgrade
[INFO]: tyut_logger init done.
[INFO]: Run Tuya Uart Tool.
[INFO]: Upgrading...
[INFO]: [1.2.0] is the latest version.
```

## Attention

1. On Linux, the default serial port requires administrator rights. You need to execute `sudo usermod -aG dialout $USER` and restart the Linux system to use the serial port firmware download.
2. On Linux, you can obtain the port numbers at the command line:
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
The devices listed with the names `/dev/ttyACM0` and `/dev/ttyACM1` are the USB-TTL devices. In some cases, the device may be named `/dev/ttyUSB0`.

3. On Windows, run the `cli.exe` program.

4. The TuyaOS Flash download tool is mainly used for development-stage debugging and burning. For mass production, it is recommended to use the "Tuya Production Solution". For details, click [https://developer.tuya.com/eu/docs/iot/SCJJ-01?id=Kcpv15oujgz00](https://developer.tuya.com/cn/docs/iot/SCJJ-01?id=Kcpv15oujgz00).