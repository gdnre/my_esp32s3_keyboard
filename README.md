# 简介
本项目使用esp32s3作为主控芯片，在esp-idf平台开发，如果要全部功能同时开启，需要2MB psram以上的芯片版本，相关硬件将上传至嘉立创开源广场，[一个待更新的链接]。
## 功能
* 键盘为全键无冲，最高支持hid协议中编号0-135的键盘按键以及8个修饰键同时按下，支持所有consumer按键，但同时只能有一个被按下。
* 不支持自定义组合键和宏等功能。
* 键盘连接方式支持usb、蓝牙（ble）、espnow，其中espnow模式需要另一个设备作为接收器，espnow模式下，可实现睡眠唤醒时不吞键。
* 特定按键可替换为屏幕，显示键盘按键信息或自定义图片。
* 特定按键可替换为旋转编码器。
* 可扩展usb2.0 hub。
* 支持rgb灯，但需自行编写灯光模式。
* 支持通过网页或者msc功能配置键盘和更新固件。

# 构建固件
## 烧录已有固件
如果有bin格式的固件程序，可以不用配置完整esp-idf开发环境进行烧录，本仓库build文件夹下提供了所需bin文件，但仍建议自行构建。


首先安装python环境，然后安装esptool：
```
pip install esptool
```
准备好仓库build文件夹所示的文件，通过usb连接esp32s3，确保处于可进行烧录的模式，使用如下esptool命令进行烧录：
```
esptool write-flash @flash_project_args
```
## 自行构建固件
根据esp-idf官方教程搭建好开发环境，克隆或下载本仓库下build文件夹以外的代码，检查配置后即可进行构建。
### 修改esp-idf组件源码
我使用的esp-idf版本为v5.3.2，该版本下nimble的部分功能没有生效，需要添加部分函数，如果之后的版本这些功能可用了，建议使用官方函数，有改动的文件保持原目录结构放在my_v5.3.2文件夹中，修改文件和函数如下：
* ble_hs.h和ble_svc_hid.c文件中添加函数```my_nimble_register_report_cb```，修改函数```ble_svc_hid_access```，用于接收hid output报告，获取主机的指示灯状态。
* 如果构建时还有其它函数报错提示不存在，请给我提issue，我忘了是否还改过其它文件了。

### 项目配置
```sdkconfig.defaults```为初次构建会使用的默认配置，本仓库中的该文件为键盘固件的配置。
如果要构建接收器固件，可将```my_receiver_sdkconfig.defaults```文件重命名为```sdkconfig.defaults```。也可通过menuconfig修改配置，勾选```MY_DEVICE_IS_RECEIVER```配置项即可，接收器功能可不用psram，如果使用无psram的芯片，需要取消勾选"Support for external, SPI-connected RAM"配置项。
