# 简介
本项目使用esp32s3作为主控芯片，在esp-idf平台开发，如果要全部功能同时开启，需要2MB psram以上的芯片版本，相关硬件将上传至嘉立创开源广场，[点击跳转](https://oshwhub.com/gdnre/esp32s3-san-mo-xiao-jian-pan)。
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

### 项目组件简介
我自己创建或修改的组件，都放在components文件夹中，且绝大多数文件带my_前缀，使用idf组件管理器管理的文件则会放在构建项目时自动创建的managed_components文件夹中，不建议直接修改该文件夹中的内容。
#### 硬件不同时需要修改的配置
硬件不同时，需要配置引脚等信息，对应组件如下：
* my_config组件：；大部分组件都依赖该组件中的信息，my_config.h中可以修改键盘、传感器等对应的引脚信息，还用于管理运行时的配置。
* my_display组件：屏幕显示相关的组件，屏幕的引脚信息要在my_lvgl_private.h中修改。
* my_keyboard组件：键盘按键、行为相关组件，按键数组要在my_keyboard.c中配置。
#### 其它组件
* my_init组件：应该被整合进my_config中，仅用于初始化默认esp事件循环、获取设备mac等大部分组件都需要且不会被释放的资源。
* my_fat_mount组件：用于msc功能以外的fat分区挂载、文件读取。
* my_http_start组件：被my_http_server组件使用，用于启动http服务器。
* my_http_server组件：用于启动http服务器，匹配和处理uri。
* my_ina226组件：用于获取ina226传感器数据，被main组件中的my_esp_timer.c使用，定时获取数据。
* my_led_strips组件：用于管理rgb灯。
* my_input_keys组件：用于在硬件层面读取按键矩阵、gpio按键、编码器按键的输入，并执行特定事件以供my_keyboard组件使用。
* my_wifi_start组件：控制ap、sta、espnow功能的开关和行为，也管理http服务器等依赖wifi的功能的开关。
* my_usb组件：管理usb的hid和msc功能，键盘的hid报告也由该组件管理（更合理的结构应该将hid报告分离出来）。
* my_ble_hid组件：管理蓝牙的hid功能，本身没有管理hid报告，和my_usb组件共用一份hid报告。