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

* 可以快速去采集数据，但是采集的数据都保存在本地的文件系统中或者哪个存储中（断电可恢复），数据一小时或者指定周期上传一次

* 可以换一个板子 esp32C6 看看省电的效果, 


### 注意点


8位分辨率：可以产生 2⁸ = 256 个不同的电平（0-255）

频率决定了PWM信号每秒钟开关的次数，单位是赫兹(Hz)。esp32 默认是 5kHz

* chatgpt 给的结论是 0.1s 通 2s 散热，0.1mm 的导线能通过的最大电流是 3.4 A 左右






