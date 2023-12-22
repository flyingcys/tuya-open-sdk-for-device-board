# T2

[English](https://github.com/tuya/T2/blob/master/README.md) | 中文

**涂鸦 T2-U 开发板** 是一款专为开发者打造的智能硬件产品原型开发板。它可与其他功能电路模组或电路板配合使用，实现各种智能硬件产品的开发。涂鸦 T2-U 开发板非常易于使用，您可以利用此开发板快速实现各种智能硬件 Demo。

<img alt="image.png" src="https://airtake-public-data-1254153901.cos.ap-shanghai.myqcloud.com/content-platform/hestia/16781047011ffea0d5520.png" width="400">

## T2-U模组概述
涂鸦 T2-U 开发板主控采用涂鸦智能开发的一款嵌入式 Wi-Fi+蓝牙模组 [T2-U](https://developer.tuya.com/en/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq)。它由一个高集成度的无线射频模组 T2-U 和外围的按键、LED 指示灯、I/O 接口、电源和 USB 转串口芯片构成。

T2-U模组 内置运行速度最高可到 120 MHz 的 32-bit MCU，内置 2Mbyte 闪存和 256 KB RAM。

### 特性

- 内置低功耗 32 位 CPU，可以兼作应用处理器
- 主频支持 120MHz
- 工作电压：3.0V-3.6V
- 外设：
  - 6×PWM
  - 4xTimer
  - 2×UART
  - 1×SPI
  - 2xI2C
  - 1xADC
  - 19xGPIO
- Wi-Fi 连通性
  - 802.11 b/g/n
  - 通道1-14@2.4GHz
  - 支持 WEP、WPA/WPA2、WPA/WPA2 PSK (AES) 和 WPA3 安全模式
  - 802.11b 模式下最大 +16dBm 的输出功率
  - 支持 STA/AP/STA+AP 工作模式
  - 支持 SmartConfig 和 AP 两种配网方式（包括 Android 和 iOS 设备）
  - 板载 PCB 天线，天线峰值增益 2.2dBi
  - 工作温度：-40℃ 到 105℃
- 蓝牙连通性
  - 低功耗蓝牙 V5.2 完整标准
  - 蓝牙模式支持 6 dBm 发射功率
  - 完整的蓝牙共存接口
  - 板载 PCB 天线，天线峰值增益 2.2dBi

[更多T2-U模组资料](https://developer.tuya.com/cn/docs/iot/T2-U-module-datasheet?id=Kce1tncb80ldq)