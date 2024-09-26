#ifndef WEATHER_H
#define WEATHER_H

#include "time.h"

// Global variables for weather data
typedef struct {
    char description[64];
    char icon[8];
    float temperature;
    int pressure;
    int humidity;
    time_t sunrise; // Update to time_t
    time_t sunset;  // Update to time_t
    int sunrise_hour;
    int sunrise_minute;
    int sunset_hour;
    int sunset_minute;
} weather_info_t;


extern weather_info_t current_weather;

#endif
