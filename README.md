# Weather display for ESP32

![Test Status](https://github.com/georgik/esp32-weather-display/actions/workflows/test.yml/badge.svg)

Display information from [Open Weather Map](https://www.openweathermap.org) on the screen.

## Requirements

`idf_component_manager` 2.x - part of [ESP-IDF 5.4](https://github.com/espressif/esp-idf)

## Configuration

Copy `nvs-template.csv` to `nvs.csv`.

```shell
cp nvs-template.csv nvs.csv
```

Update information about Wi-Fi network and Open Weather Map API key.

## Build

```
git clone git@github.com:georgik/esp32-weather-display.git
cd weather-display

idf.py @boards/lilygo-ttgo-t5-47.cfg build
```

### Other boards

- ESP32-S3-BOX-3
```shell
idf.py @boards/esp-box-3.cfg build
```

- ESP32-S3-BOX (prior Dec. 2023)
```shell
idf.py @boards/lilygo-ttgo-t5-47.cfg build
```

- ESP32-P4
```shell
idf.py @boards/esp32_p4_function_ev_board.cfg build
```

- LilyGo TTGO T5 4.7 with ESP32
```shell
idf.py @boards/lilygo-ttgo-t5-47.cfg build
```

- M5Stack-CoreS3
```shell
idf.py @boards/m5stack_core_s3.cfg build
```


## Credits

- FreeSans.ttf - https://github.com/opensourcedesign/fonts/blob/master/gnu-freefont_freesans/FreeSans.ttf
