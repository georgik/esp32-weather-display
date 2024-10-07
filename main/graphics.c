#include "graphics.h"
#include <stdio.h>
#include "esp_log.h"
#include "weather.h"

// SDL_Color textColor = {255, 255, 255, 255}; // White color
SDL_Color textColor = {0, 0, 0, 255}; // Black color


static const char *TAG = "graphics";

SDL_Texture *LoadBackgroundImage(SDL_Renderer *renderer, const char *imagePath)
{
    // Load the image into a surface
    SDL_Surface *imageSurface = SDL_LoadBMP(imagePath);
    if (!imageSurface) {
        printf("Failed to load image: %s\n", SDL_GetError());
        return NULL;
    }

    // Convert the surface to a texture
    SDL_Texture *imageTexture = SDL_CreateTextureFromSurface(renderer, imageSurface);
    SDL_DestroySurface(imageSurface); // We no longer need the surface
    if (!imageTexture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }
    return imageTexture;
}

void clear_screen(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
}

void draw_image(SDL_Renderer *renderer, SDL_Texture *texture, float x, float y, float w, float h) {
    SDL_FRect destRect = {x, y, w, h};
    SDL_RenderTexture(renderer, texture, NULL, &destRect);
}

void draw_moving_rectangles(SDL_Renderer *renderer, float rect_x) {
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 0, 0, 0, 0);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 255, 0, 0, 1);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 255, 165, 0, 2);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 255, 255, 0, 3);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 0, 255, 0, 4);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 0, 0, 255, 5);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 75, 0, 130, 6);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 238, 130, 238, 7);
    DrawColoredRect(renderer, rect_x, 20, 50, 10, 255, 255, 255, 8);
}

void DrawColoredRect(SDL_Renderer *renderer, int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, int index) {
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_FRect rect = {x, y + index * h, w, h};
    SDL_RenderFillRect(renderer, &rect);
}


// Render weather data using SDL
void render_weather_data(SDL_Renderer *renderer, TTF_Font *font) {
    ESP_LOGI(TAG, "Clearing screen. ");
    // Clear screen
    clear_screen(renderer);

    ESP_LOGI(TAG, "Preparing content. ");
    // Prepare text strings
    char temp_str[64];
    snprintf(temp_str, sizeof(temp_str), "Temperature: %.1f°C", current_weather.temperature);

    char pressure_str[64];
    snprintf(pressure_str, sizeof(pressure_str), "Pressure: %d hPa", current_weather.pressure);

    char humidity_str[64];
    snprintf(humidity_str, sizeof(humidity_str), "Humidity: %d%%", current_weather.humidity);

    ESP_LOGI(TAG, "Sending SDL data ");
    // Render temperature
    SDL_Surface *temp_surface = TTF_RenderText_Blended(font, temp_str, 0, textColor);
    SDL_Texture *temp_texture = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FRect temp_rect = {20.0f, 20.0f, (float)temp_surface->w, (float)temp_surface->h};
    SDL_RenderTexture(renderer, temp_texture, NULL, &temp_rect);
    SDL_DestroySurface(temp_surface);
    SDL_DestroyTexture(temp_texture);

    // Render pressure
    SDL_Surface *pressure_surface = TTF_RenderText_Blended(font, pressure_str, 0, textColor);
    SDL_Texture *pressure_texture = SDL_CreateTextureFromSurface(renderer, pressure_surface);
    SDL_FRect pressure_rect = {20.0f, 60.0f, (float)pressure_surface->w, (float)pressure_surface->h};
    SDL_RenderTexture(renderer, pressure_texture, NULL, &pressure_rect);
    SDL_DestroySurface(pressure_surface);
    SDL_DestroyTexture(pressure_texture);

    // Render humidity
    SDL_Surface *humidity_surface = TTF_RenderText_Blended(font, humidity_str, 0, textColor);
    SDL_Texture *humidity_texture = SDL_CreateTextureFromSurface(renderer, humidity_surface);
    SDL_FRect humidity_rect = {20.0f, 100.0f, (float)humidity_surface->w, (float)humidity_surface->h};
    SDL_RenderTexture(renderer, humidity_texture, NULL, &humidity_rect);
    SDL_DestroySurface(humidity_surface);
    SDL_DestroyTexture(humidity_texture);

    // Render description
    SDL_Surface *desc_surface = TTF_RenderText_Blended(font, current_weather.description, 0, textColor);
    SDL_Texture *desc_texture = SDL_CreateTextureFromSurface(renderer, desc_surface);
    SDL_FRect desc_rect = {20.0f, 140.0f, (float)desc_surface->w, (float)desc_surface->h};
    SDL_RenderTexture(renderer, desc_texture, NULL, &desc_rect);
    SDL_DestroySurface(desc_surface);
    SDL_DestroyTexture(desc_texture);

        // Render sunrise time
    char sunrise_str[64];
    snprintf(sunrise_str, sizeof(sunrise_str), "Sunrise: %02d:%02d", current_weather.sunrise_hour, current_weather.sunrise_minute);
    SDL_Surface *sunrise_surface = TTF_RenderText_Blended(font, sunrise_str, 0, textColor);
    SDL_Texture *sunrise_texture = SDL_CreateTextureFromSurface(renderer, sunrise_surface);
    SDL_FRect sunrise_rect = {20.0f, 180.0f, (float)sunrise_surface->w, (float)sunrise_surface->h};
    SDL_RenderTexture(renderer, sunrise_texture, NULL, &sunrise_rect);
    SDL_DestroySurface(sunrise_surface);
    SDL_DestroyTexture(sunrise_texture);

    // Render sunset time
    char sunset_str[64];
    snprintf(sunset_str, sizeof(sunset_str), "Sunset: %02d:%02d", current_weather.sunset_hour, current_weather.sunset_minute);
    SDL_Surface *sunset_surface = TTF_RenderText_Blended(font, sunset_str, 0, textColor);
    SDL_Texture *sunset_texture = SDL_CreateTextureFromSurface(renderer, sunset_surface);
    SDL_FRect sunset_rect = {20.0f, 220.0f, (float)sunset_surface->w, (float)sunset_surface->h};
    SDL_RenderTexture(renderer, sunset_texture, NULL, &sunset_rect);
    SDL_DestroySurface(sunset_surface);
    SDL_DestroyTexture(sunset_texture);


    // Load and render weather icon
    // char icon_path[64];
    // snprintf(icon_path, sizeof(icon_path), "/spiffs/icons/%s.png", current_weather.icon);
    // SDL_Surface *icon_surface = IMG_Load(icon_path);
    // if (icon_surface) {
    //     SDL_Texture *icon_texture = SDL_CreateTextureFromSurface(renderer, icon_surface);
    //     SDL_FRect icon_rect = {300.0f, 20.0f, 100.0f, 100.0f}; // Position and size of the icon
    //     SDL_RenderTexture(renderer, icon_texture, NULL, &icon_rect);
    //     SDL_DestroySurface(icon_surface);
    //     SDL_DestroyTexture(icon_texture);
    // } else {
    //     ESP_LOGE(TAG, "Failed to load icon: %s", SDL_GetError());
    // }

    // Update the renderer to display everything
    ESP_LOGI(TAG, "Flushing to screen. ");
    SDL_RenderPresent(renderer);
    ESP_LOGI(TAG, "Draw operation complete ");
}
