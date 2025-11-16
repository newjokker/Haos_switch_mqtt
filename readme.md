# 说明

### 环境

```bash

platformio lib install "Adafruit GFX Library"

platformio lib install "Adafruit SSD1306"

platformio lib install "DHT sensor library"

platformio lib install "PubSubClient"

pio lib install "Adafruit NeoPixel"

pio pkg install --library 1076

pio pkg install --library "knolleary/PubSubClient"

pio lib install "Stepper"

pio lib install "AccelStepper"

```

### 调试代码

* 串口调试
```bash

tio -b 115200 --timestamp  /dev/cu.wchusbserial5A7B1617701 

```

* python 代码调试

```bash

python3 ./tools/read_csv.py /dev/cu.wchusbserial5A7B1617701 

```


### 需求

* 完善对应的注册，对于每一个产品，需要注册的信息应该都放在配置文件中，

* 可以在后续修改配置信息，是不是在 haos 中修改更加方便（目前没在 haos 中找对对应的页面或者 api）

* 写一个扫描 mqtt 的代码，查看各个 mac 的设备在做什么操作，

* 上传配置信息的时候要自动获取当前的板子的信号等信息，写在配置文件中，这样方便进行管理

* 连接上 wifi 应该自动跳出对应的页面进行配置，而不是需要打开对应的网页！！

### 已经解决

* 当前使用两个板子同时通电的时候两块板子互相影响，不知道是哪里出现了问题 （应该是使用了同一个 client_id 导致的）

* 用于初始化的 boot 按键 注意 esp32S3 是 0 ， esp32c3 是 9

* 获取的 mac 地址可能是重复的，获取 WiFi 的 mac 地址，是唯一的
