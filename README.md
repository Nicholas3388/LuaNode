LuaNode
======================================

This project is an improved `Lua` SDK, based on ESP-IDF/ESP8266_RTOS_SDK, for Esp32 (compatible with Esp8266).

This project is developed by `DOIT` WiFi team.

DOIT is a high-tech company, focus on IoT. Visit homepage: www.doit.am


![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/logo.jpg "Esp32 & Esp8266")

NEW UPDATE FOR ESP32:
--------------------------------------

New version of LuaNode32 is developed based on esp-idf and compatible with `Esplorer`.

The following figures show the DOIT ESP32 dev-board. To buy the ESP32 dev-board click [Here](https://item.taobao.com/item.htm?spm=a312a.7700824.w4002-7420449993.9.yduA0V&id=542459608596)

 
![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/esp32_fig.png "ESP32 dev-board")

New version of ESP32 dev-board is coming! The new board will be ready for retail in few days. The new board is smaller than the old one, and has more pins.


![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/ESP32_dimension.png "dimension")

Download the DOIT dev-board schematic [here](https://www.dropbox.com/s/jefwxxtufgwg0ex/esp32_Schematic%20Prints.pdf?dl=0 "schematic")

### How To Build LuaNode32
* Download prebuild toolchain [Here](https://github.com/jmattsson/nodemcu-prebuilt-toolchains "toolchains"), you 
  can also build the tools from source, follow the steps [Here](https://docs.espressif.com/projects/esp-idf/en/v2.1.1/get-started/index.html#setup-toolchain "Setup toolchain (esp-idf v2.1.1)")
* Download the esp-idf framework ([official guide](https://docs.espressif.com/projects/esp-idf/en/v2.1.1/get-started/index.html#get-esp-idf)). However You will need to include the `-b v2.1.1` flag in the `git` command line to checkout the correct version (`git clone --recursive -b v2.1.1 https://github.com/espressif/esp-idf.git`)
* Export build tools' directory by executing the following command on terminal,
  `export PATH=/your_path/toolchains/esp32/bin:$PATH`, where the `your_path` is the
  path the toolchains stored.
* Export `esp-idf` path by the following command: `export IDF_PATH=the_esp-idf_path`
* Change current directory to `LuaNode32`, then input `make` to build firmware.

For more details, view LuaNode [wiki page](https://github.com/Nicholas3388/LuaNode/wiki "LuaNode Wiki")

LUA PROGRAMMING
--------------------------------------
Programming with Lua is easy, some samples are as follow:

```lua
-- create file on file system
file.open("myfile.lua", "w+");
file.write("hello world");
file.close();

-- connect to wifi
wifi.setmode(wifi.STATION);
config = {}
config.ssid = "MYSSID"
config.pwd = "password"
wifi.sta.config(config)

-- read file from file system
file.open("myfile.lua", "r");
-- read 1024 bytes from myfile.lua and save them
-- to content
content=file.read(1024);
print(content);
file.close();

-- remove file
file.remove("myfile.lua");

-- restart device
node.restart();
```

You can add your own Lua modules to LuaNode at will, visit the LuaNode [wiki page](https://github.com/Nicholas3388/LuaNode/wiki "LuaNode Wiki") for more details.

LuaNode is compatible with `Esplorer` now, you can edit and download Lua
Code to ESP32 with `Esplorer` conveniently.

Get Esplorer [Here](https://github.com/4refr0nt/ESPlorer "Esplorer")

HOW TO BUILD For `ESP32/ESP8266`:
--------------------------------------

For details, see LuaNode wiki page [Here](https://github.com/Nicholas3388/LuaNode/wiki/How-to-build-for-ESP8266-ESP32).

HOW TO FLASH THE FIRMWARE:
--------------------------------------

See LuaNode wiki page [Here](https://github.com/Nicholas3388/LuaNode/wiki/How-to-flash-the-firmware).

HOW TO DEBUG:
--------------------------------------

See wiki page [Here](https://github.com/Nicholas3388/LuaNode/wiki/How-to-debug).


APPS:
--------------------------------------

* `task`: This is a sample to show how to create an os task. Build the example by executing the gen_misc.sh.

How to create a task:

```c
void task1(void *pvParameters) {
    // do something
}

void user_init(void) {
    xTaskCreate(task1, (signed char *)"tsk1", 256, &mainqueue, 2, NULL);
}
```

`Note: There is a task to receive uart input. You'd better alloc more than 512k memory for this task,
since the lua command handler will be called in this task, more memory is required for lua handler.`

* `luaapp`: A lua app. 

* `wifikiller`: An wifi sample. Set baud rate to 115200pbs, using UART0. It will disconnect WiFi connection.

* `wifilister`: List all APs, along with clents connected to them, near your device. The list info then sent to Android device via OTG, and display on Android.

APPLICATIONS:
--------------------------------------

### Amazon Alexa for ESP32
This is an awesome application that using Amazon Echo Dot to turn on/off the blue LED on DOIT ESP32 dev-board via voice 
control service. You can find the source code, named `alexa_esp32`, within the `examples` directory. The `alexa_esp32` 
sample can control only one pin, if you want to control more pins or to use more resource on ESP32 to do more things, 
you can learn the `alexa_multi_esp32` sample. The sample will let Alexa to do more things with ESP32.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/alexa_esp32.jpg "Amazon Echo Dot & ESP32 board")

When you build the `alexa_esp32` within the examples folder and flash the firmware to ESP32 board, then you can ask 
Echo Dot to discover device by telling her "Alexa, discover device". She will answer you "Discovery starting...". After about
20 seconds, if She does find device, she will answer you "I found 1 device...". Plus, you can search device via Alexa App or 
via this page: `alexa.amazon.com`. When Echo Dot find ESP32, you will see the found device named "esp" on `alexa.amazon.com`, show 
as the following figure:

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/alexa_esp32_dev.png "Alexa found ESP32 device")

When Alexa find ESP32 device successfully, you can ask Echo Dot "Alexa, turn on ESP" to turn on the blue LED on ESP32 borad. Ask 
"Alexa, turn off ESP" to turn off the LED. The following link is a video to show the ESP32 voice control via Amazon Echo Dot: https://youtu.be/Eg1dApUHIBc

### ESP32-Camera
This simple application is to use ESP32 to controll a Camera. You can find the source code from the following diretory: _LuaNode_Esp32/LuaNode32/examples/camera_

There is another ESP32 camera demo create by _igrr_, find it from the link: https://github.com/igrr/esp32-cam-demo. 
We also utilize OV7725 for our camera test, but the hardware is a little different from _igrr_'s project. 
My camera is extended with a RAM, which is AL422B, with 390KB RAM. So the camera can cache 2 frames, if the 
resolution is set to 320x240 pixel. The following figure is my camera, called "Yehuo", a Chinese name.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/camera_test.jpg "Yehuo Camera")

The following figures is my ESP32-Camera.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/esp32_cam.jpg "ESP32-Camera")


I learn _igrr_'s project, and do the same test as _igrr_'s. For instance, I use the camera to scan the board, and then 
you can see the output from the uart terminal.


![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/esp32_cam2.jpg "Esp32 camera test")

### ESP32 Drives Nokia5110 LCD
This is an application to show how to drive Nokia5110 LCD. It is easy for ESP32 to drive the LCD. You 
don't even need to use SPI to drive the LCD. The LCD is very cheap, and easy to buy from taobao or eBay.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/esp32_LCD.jpg "output hello world")

Pin connections:

--------
| Interface | Nokia5110 Pin | DOIT ESP32 dev-board Pin |
| --- | --- | --- |
| RESET | RST | D4 |
| LCD Enable | CE | D2 |
| DC | DC | D5 |
| Data Input | Din | D18 |
| Clock | Clk | D19 |
| Back Light | BL | D21 |
| Power Supply 3.3V | Vcc | 3V3 |
| Ground | Gnd | GND | 

For more details, view the _lcd_nokia5110_driver_ sample within the **examples** folder.

### ESP32 BLE LED controller
This is a sample to show how to create BLE client and BLE server, and create connection between them. 
To test this sample, you have to prepare two 
ESP32 dev-board. Then build the sources within the `LuaNode_ESP32/examples/ble_led_controller`, and flash 
the client and server firmware to the two ESP32 dev-boards, respectively. you will see the blue LED on the 
server board is turned on/off each 2 seconds. You can see the test from the following video: https://youtu.be/UnzXCB5EYGU

In this sample, when the client connect to server, it will send BLE notify to the server each 2 seconds. 
When the server recieves the notification, 
the server will turn on/off the blue LED on board according to the notification value. If the value is 0x1, the server 
will turn on the LED, otherwise, the LED will be turned off.

### ESP32 communicate with nRF51822 via BLE

This is a example to show how to create BLE connection between ESP32 and nRF51 device (nRF51822 inside). In this sample, 
ESP32 write "on"/"off" string to the characteristic `6e400002-b5a3-f393-e0a9-e50e24dcca9e`, which provided 
by nRF51 device. When the nRF51 device receives the content sent by ESP32, the nRF51 will turn on/off the LED on board 
according to the received string. The ESP32 will write "on"/"off" to the characteristic each 2 seconds, so you 
will see that the LED on nRF51 device blink each 2 seconds.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/nrf51822_esp32_communication.jpg "nRF51_ESP32")

The folloing video is a test for this example: https://youtu.be/hXuCXh5lEew

The nRF51 device is a nRF51822 dev-board, you can buy it from Taobao China. You can download the sources and build firmware 
for the nRF51 device from the following link: https://github.com/Nicholas3388/nRF51822_ESP32_communicate 

### BLE debug tool (LuaNode_BLE_Client)

This is an App (including iOS & Android) for ESP32 as well as other BLE device. The app named `LuaNode_BLE_Client`.
 You can use this app to connect to ESP32 
and then control the LED on DOIT ESP32 dev board. Plus, you can scan BLE devices around your phone and then view the 
services, characteristics, and descriptors provided by the devices. Download LuaNode_BLE_Client sources from the 
following repository: https://github.com/Nicholas3388/LuaNode_BLE_Client

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/ble_debug_tool.png "LuaNode_BLE_Client")

To control the LED on ESP32 board, you have to build the example `esp32_ble_gatt_server_led_control_for_phone`, and then 
download the firmware. Run the `LuaNode_BLE_Client` and then toggle the LED switch on app to turn on/off the LED on board. 
The following video is a test for ESP32: https://www.youtube.com/watch?v=LhvA33yf7P8

### Wifilister

Another interesting application is the `Wifilister` app. The app scans all APs along with the clients connected to them around device, and scan results are sent to
Android device via OTG, then you can see the result displayed on Android device.

To test this app, you have to install the `LuanodeUsbHost` Android app to your Android phone (device).
`LuanodeUsbHost` is an Android USB Host app for ESP8266/ESP32. The Android device receive messages, sent from ESP8266, via OTG.
Then the messages display on this app.

Download the `LuanodeUsbHost` source [Here](https://github.com/Nicholas3388/LuanodeUsbHost).

Compile `Wifilister` provided in Luanode, flash it to ESP8266/ESP32, and then connect your ESP8266/ESP32 with Android phone.
You can see the scanning results.

![github](https://github.com/Nicholas3388/LuaNode/raw/master/images/luanode_host.jpg "LuanodeUsbHost")

FAQ:
--------------------------------------

See wiki page [Here](https://github.com/Nicholas3388/LuaNode/wiki/FAQ)


WHO ARE THE PEOPLE BEHIND THIS PROJECT
-------------------------------------

`Wei Wang`: He got his master's degree from Guilin University of Electronic Technology in 2012. He has more than 4 years work experience on Embedded System.

![github](https://ksr-ugc.imgix.net/assets/007/539/619/4b02fe2896172cf177b6558300cdfb45_original.jpg?w=680&fit=max&v=1461463286&auto=format&q=92&s=7d405bf015bc1b0036851755b03663dc)

`DOIT`(www.doit.am): DOIT is a high-tech company, who focus on IoT technology and open source hardware. He supports this project and provides hardware for this project. Its successful products include WifiMCU, Doit Video Car, and Wifi sniffer, etc.

![github](https://ksr-ugc.imgix.net/assets/010/012/333/2e153bf8a6f1cc5781643ad8d704f285_original.gif?w=680&fit=max&v=1461661228&q=92&s=c8f52001f353a41b455f2a55b3c11256)


CONTACT ME:
--------------------------------------

If you have any question, you can contact me via Email/QQ list below, Thanks:) 

  - `Email`: wangwei@doit.am
  - `QQ`: 535873968


CHANGE LOG:
--------------------------------------
2016.4.7 
	Modified lbaselib.c

2016.4.19
	Modified code structure	

2016.5.11
	Replace libc

2017.1.19
	Update to the latest esp-idf

2018.1.5
	Update for the latest esp-idf
