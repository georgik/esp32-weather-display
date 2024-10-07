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

## Build for P4

Update of on-board ESP32-C6 firmware for Wi-Fi connectivity.

Follow official instructions: https://github.com/espressif/esp-hosted/blob/feature/esp_as_mcu_host/docs/esp32_p4_function_ev_board.md#52-using-esp-prog

Steps used in this version:

```shell
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c6
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build flash monitor
```

Connection via ESP-PROG. Connect all 6 wires in the same order. Do not switch RX/TX. Swich whole board to Boot mode: Hold boot, press Reset.
The reset applies to both chips.

## Credits

- FreeSans.ttf - https://github.com/opensourcedesign/fonts/blob/master/gnu-freefont_freesans/FreeSans.ttf
