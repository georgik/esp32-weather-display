#ifndef WEATHER_H
#define WEATHER_H

// Global variables for weather data
typedef struct {
    char description[64];
    char icon[8];
    float temperature;
    int pressure;
    int humidity;
} weather_info_t;

extern weather_info_t current_weather;

#endif
