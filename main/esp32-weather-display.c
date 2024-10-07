// esp32-weather-display

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ESP-IDF includes
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_wifi_remote.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// SDL includes
#include "SDL3/SDL.h"
#include "SDL3_ttf/SDL_ttf.h"
// #include "SDL3_image/SDL_image.h"

#include "bsp/esp-bsp.h"

#include "esp_vfs.h"
#include "esp_littlefs.h"

#include "filesystem.h"

#include "graphics.h"

#include "weather.h"

static const char *TAG = "WeatherApp";

char wifi_ssid[32];
char wifi_password[64];

#define MAXIMUM_RETRY   5

// OpenWeatherMap API details
char openweather_api_key[42];
char openweather_city_name[32];
char openweather_code[6];

// Event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

SDL_Window *window;
SDL_Renderer *renderer;
TTF_Font *font;


// Function prototypes
static void wifi_init_sta(void);
static void time_sync(void);
static void fetch_weather_data(void);
static void parse_weather_data(const char *json);
static void initialize_sdl();


void log_memory()
{
    size_t free_heap = esp_get_free_heap_size();

    // Get the high water mark (minimum amount of stack that remained unused)
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);

    // Calculate available stack in bytes
    size_t free_stack = high_water_mark * sizeof(StackType_t);

    // Log the available heap and stack memory
    ESP_LOGI("MEMORY", "Free Heap: %u bytes", free_heap);
    ESP_LOGI("MEMORY", "Free Stack: %u bytes", free_stack);

}

// Wi-Fi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to the Wi-Fi network...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to Wi-Fi.");
        }
        ESP_LOGI(TAG,"Connect to the AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


esp_err_t get_wifi_credentials(void){

	esp_err_t err;

	ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle");
    nvs_handle_t nvs_mem_handle;
    err = nvs_open_from_partition("nvs", "storage", NVS_READWRITE, &nvs_mem_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "The NVS handle successfully opened");

	size_t ssid_len = sizeof(wifi_ssid);
	size_t pass_len = sizeof(wifi_password);
    size_t openweather_api_key_len = sizeof(openweather_api_key);
    size_t openweather_city_name_len = sizeof(openweather_city_name);
    size_t openweather_code_len = sizeof(openweather_code);

    err = nvs_get_str(nvs_mem_handle, "ssid", wifi_ssid, &ssid_len);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_mem_handle, "password", wifi_password, &pass_len);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_mem_handle, "ow_api_key", openweather_api_key, &openweather_api_key_len);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_mem_handle, "ow_city", openweather_city_name, &openweather_city_name_len);
    ESP_ERROR_CHECK(err);

    err = nvs_get_str(nvs_mem_handle, "ow_country", openweather_code, &openweather_code_len);
    ESP_ERROR_CHECK(err);

    nvs_close(nvs_mem_handle);
    return ESP_OK;
}

// Initialize Wi-Fi
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create the default Wi-Fi station
    esp_netif_create_default_wifi_sta();

    // Initialize Wi-Fi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(get_wifi_credentials());

    // Configure the Wi-Fi connection
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            // Setting the threshold of authentication mode
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    // Set the Wi-Fi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set the Wi-Fi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // **Disable Wi-Fi power saving mode**
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    // Check the event bits to determine the connection status
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s password:***",
                 wifi_ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:***",
                 wifi_ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
static char *response_buffer = NULL;
static int response_len = 0;
// Synchronize time using SNTP
static void time_sync(void) {
    ESP_LOGE(TAG, "Time sync.");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org"); // Use default NTP server
    esp_sntp_init();

    // Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (retry == retry_count) {
        ESP_LOGE(TAG, "Failed to synchronize time.");
    } else {
        ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
    }
}

const char openweather_pem[] = R"EOF(
-----BEGIN CERTIFICATE-----
MIIGRjCCBS6gAwIBAgIRAOkdF7biDWqRMeJTQD8Ux4AwDQYJKoZIhvcNAQELBQAw
gY8xCzAJBgNVBAYTAkdCMRswGQYDVQQIExJHcmVhdGVyIE1hbmNoZXN0ZXIxEDAO
BgNVBAcTB1NhbGZvcmQxGDAWBgNVBAoTD1NlY3RpZ28gTGltaXRlZDE3MDUGA1UE
AxMuU2VjdGlnbyBSU0EgRG9tYWluIFZhbGlkYXRpb24gU2VjdXJlIFNlcnZlciBD
QTAeFw0yNDA3MTkwMDAwMDBaFw0yNTAzMjEyMzU5NTlaMB8xHTAbBgNVBAMMFCou
b3BlbndlYXRoZXJtYXAub3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
AQEAymaBT5fmqUJwvYrNItFxekHtC3RQkNUFn5MHc5UJcUpZD2SYBA4U7EVunaxs
6zxVcDShRiS9du3XZ+71S8j9sFWIVu0gEz3iCRarm6SAAf5I7oSYodNGhDN0bnB4
4dT25K8+n+V3x6imlF8s84pJ3kjNgDI1/IMqO5mMAJlBpzsWc30bmupas7AYwIkN
9yhUdMfcD3WOr45Ip8RgsUenqid/bDQ/CLXUzMrBnV6xlvkpFgWL+N0Q1WUwW9it
6u3RsamqqSNT1XsUq20EVd4ws9yP4vOk5k5dNUfrXuAJUBvStMk7JaDC68oI7fD+
UmGw8EsxRWFPF8tSW/FMUfyHNwIDAQABo4IDCjCCAwYwHwYDVR0jBBgwFoAUjYxe
xFStiuF36Zv5mwXhuAGNYeEwHQYDVR0OBBYEFCf9W7qNhvxyU3ATC0PyO3Te0O1D
MA4GA1UdDwEB/wQEAwIFoDAMBgNVHRMBAf8EAjAAMB0GA1UdJQQWMBQGCCsGAQUF
BwMBBggrBgEFBQcDAjBJBgNVHSAEQjBAMDQGCysGAQQBsjEBAgIHMCUwIwYIKwYB
BQUHAgEWF2h0dHBzOi8vc2VjdGlnby5jb20vQ1BTMAgGBmeBDAECATCBhAYIKwYB
BQUHAQEEeDB2ME8GCCsGAQUFBzAChkNodHRwOi8vY3J0LnNlY3RpZ28uY29tL1Nl
Y3RpZ29SU0FEb21haW5WYWxpZGF0aW9uU2VjdXJlU2VydmVyQ0EuY3J0MCMGCCsG
AQUFBzABhhdodHRwOi8vb2NzcC5zZWN0aWdvLmNvbTAzBgNVHREELDAqghQqLm9w
ZW53ZWF0aGVybWFwLm9yZ4ISb3BlbndlYXRoZXJtYXAub3JnMIIBfgYKKwYBBAHW
eQIEAgSCAW4EggFqAWgAdgDPEVbu1S58r/OHW9lpLpvpGnFnSrAX7KwB0lt3zsw7
CAAAAZDLi7LCAAAEAwBHMEUCIQC7sBbrN3agg5gTj7i8Fcb/xeNjOUA1R6px57xS
49ZpjAIgTlFKxXe/zQDH0K1gKwj0QC/YagaPt/7xLtIK5Jpwiw0AdwCi4wrkRe+9
rZt+OO1HZ3dT14JbhJTXK14bLMS5UKRH5wAAAZDLi7KGAAAEAwBIMEYCIQCBZS49
4Wc5JQW/qVL4iVBL/m5DCuxGIvxOCzc2axNnHgIhAOKkmInvO1CcHqWGqc0vsdii
h3Xv1UEegcCT09HUUqsfAHUATnWjJ1yaEMM4W2zU3z9S6x3w4I4bjWnAsfpksWKa
Od8AAAGQy4uyYQAABAMARjBEAiAMZM8OPuNnAI2I4k5PwYPUsD6Vci0pAvFnvQyZ
Q34JyAIgJvy9Bp4vID0eslLQf0l6lDF7P+vafEgJhsBGwffng+AwDQYJKoZIhvcN
AQELBQADggEBAIyxMyP0FUpBSP//gbqsRO/eipZXmbZLjcZI32G1GfmknqfLgSx6
7vdvL7o7vQIkrQ2R7x950dULEKZaZtWekm7+M05HnEXejpRVXPv948PswQJIR/nW
Q/ZIuLHOycgpictvOas4SW+8fsHrIAogc3CZ/p+BlCleRSTCmc7EpQyYpxou0fC/
bzEeG4j2liPdIXf9txOmPNARPwwCr/FbiqHpwzaMBQn4Ub7SquNw8QyhACjPxCyK
UkeAMh0Vg3+QuBXF70mHbfLpU+CeWJxSn9eISmqvoyS5lzzEqQ41JRPiF7PAGU5h
MSqCGoq+JnukAUzJaaj32ATdm6L78J1wEM4=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIGEzCCA/ugAwIBAgIQfVtRJrR2uhHbdBYLvFMNpzANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTgx
MTAyMDAwMDAwWhcNMzAxMjMxMjM1OTU5WjCBjzELMAkGA1UEBhMCR0IxGzAZBgNV
BAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEYMBYGA1UE
ChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFJTQSBEb21haW4g
VmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEA1nMz1tc8INAA0hdFuNY+B6I/x0HuMjDJsGz99J/LEpgPLT+N
TQEMgg8Xf2Iu6bhIefsWg06t1zIlk7cHv7lQP6lMw0Aq6Tn/2YHKHxYyQdqAJrkj
eocgHuP/IJo8lURvh3UGkEC0MpMWCRAIIz7S3YcPb11RFGoKacVPAXJpz9OTTG0E
oKMbgn6xmrntxZ7FN3ifmgg0+1YuWMQJDgZkW7w33PGfKGioVrCSo1yfu4iYCBsk
Haswha6vsC6eep3BwEIc4gLw6uBK0u+QDrTBQBbwb4VCSmT3pDCg/r8uoydajotY
uK3DGReEY+1vVv2Dy2A0xHS+5p3b4eTlygxfFQIDAQABo4IBbjCCAWowHwYDVR0j
BBgwFoAUU3m/WqorSs9UgOHYm8Cd8rIDZsswHQYDVR0OBBYEFI2MXsRUrYrhd+mb
+ZsF4bgBjWHhMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEAMB0G
A1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAbBgNVHSAEFDASMAYGBFUdIAAw
CAYGZ4EMAQIBMFAGA1UdHwRJMEcwRaBDoEGGP2h0dHA6Ly9jcmwudXNlcnRydXN0
LmNvbS9VU0VSVHJ1c3RSU0FDZXJ0aWZpY2F0aW9uQXV0aG9yaXR5LmNybDB2Bggr
BgEFBQcBAQRqMGgwPwYIKwYBBQUHMAKGM2h0dHA6Ly9jcnQudXNlcnRydXN0LmNv
bS9VU0VSVHJ1c3RSU0FBZGRUcnVzdENBLmNydDAlBggrBgEFBQcwAYYZaHR0cDov
L29jc3AudXNlcnRydXN0LmNvbTANBgkqhkiG9w0BAQwFAAOCAgEAMr9hvQ5Iw0/H
ukdN+Jx4GQHcEx2Ab/zDcLRSmjEzmldS+zGea6TvVKqJjUAXaPgREHzSyrHxVYbH
7rM2kYb2OVG/Rr8PoLq0935JxCo2F57kaDl6r5ROVm+yezu/Coa9zcV3HAO4OLGi
H19+24rcRki2aArPsrW04jTkZ6k4Zgle0rj8nSg6F0AnwnJOKf0hPHzPE/uWLMUx
RP0T7dWbqWlod3zu4f+k+TY4CFM5ooQ0nBnzvg6s1SQ36yOoeNDT5++SR2RiOSLv
xvcRviKFxmZEJCaOEDKNyJOuB56DPi/Z+fVGjmO+wea03KbNIaiGCpXZLoUmGv38
sbZXQm2V0TP2ORQGgkE49Y9Y3IBbpNV9lXj9p5v//cWoaasm56ekBYdbqbe4oyAL
l6lFhd2zi+WJN44pDfwGF/Y4QA5C5BIG+3vzxhFoYt/jmPQT2BVPi7Fp2RBgvGQq
6jG35LWjOhSbJuMLe/0CjraZwTiXWTb2qHSihrZe68Zk6s+go/lunrotEbaGmAhY
LcmsJWTyXnW0OMGuf1pGg+pRyrbxmRE1a6Vqe8YAsOf4vmSyrcjC8azjUeqkk+B5
yOGBQMkKW+ESPMFgKuOXwIlCypTPRpgSabuY0MLTDXJLR27lk8QyKGOHQ+SwMj4K
00u/I5sUKUErmgQfky3xxzlIPK1aEn8=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFgTCCBGmgAwIBAgIQOXJEOvkit1HX02wQ3TE1lTANBgkqhkiG9w0BAQwFADB7
MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYD
VQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UE
AwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTE5MDMxMjAwMDAwMFoXDTI4
MTIzMTIzNTk1OVowgYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5
MRQwEgYDVQQHEwtKZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBO
ZXR3b3JrMS4wLAYDVQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0
aG9yaXR5MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAgBJlFzYOw9sI
s9CsVw127c0n00ytUINh4qogTQktZAnczomfzD2p7PbPwdzx07HWezcoEStH2jnG
vDoZtF+mvX2do2NCtnbyqTsrkfjib9DsFiCQCT7i6HTJGLSR1GJk23+jBvGIGGqQ
Ijy8/hPwhxR79uQfjtTkUcYRZ0YIUcuGFFQ/vDP+fmyc/xadGL1RjjWmp2bIcmfb
IWax1Jt4A8BQOujM8Ny8nkz+rwWWNR9XWrf/zvk9tyy29lTdyOcSOk2uTIq3XJq0
tyA9yn8iNK5+O2hmAUTnAU5GU5szYPeUvlM3kHND8zLDU+/bqv50TmnHa4xgk97E
xwzf4TKuzJM7UXiVZ4vuPVb+DNBpDxsP8yUmazNt925H+nND5X4OpWaxKXwyhGNV
icQNwZNUMBkTrNN9N6frXTpsNVzbQdcS2qlJC9/YgIoJk2KOtWbPJYjNhLixP6Q5
D9kCnusSTJV882sFqV4Wg8y4Z+LoE53MW4LTTLPtW//e5XOsIzstAL81VXQJSdhJ
WBp/kjbmUZIO8yZ9HE0XvMnsQybQv0FfQKlERPSZ51eHnlAfV1SoPv10Yy+xUGUJ
5lhCLkMaTLTwJUdZ+gQek9QmRkpQgbLevni3/GcV4clXhB4PY9bpYrrWX1Uu6lzG
KAgEJTm4Diup8kyXHAc/DVL17e8vgg8CAwEAAaOB8jCB7zAfBgNVHSMEGDAWgBSg
EQojPpbxB+zirynvgqV/0DCktDAdBgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rID
ZsswDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAG
BgRVHSAAMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29t
L0FBQUNlcnRpZmljYXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggr
BgEFBQcwAYYYaHR0cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUA
A4IBAQAYh1HcdCE9nIrgJ7cz0C7M7PDmy14R3iJvm3WOnnL+5Nb+qh+cli3vA0p+
rvSNb3I8QzvAP+u431yqqcau8vzY7qN7Q/aGNnwU4M309z/+3ri0ivCRlv79Q2R+
/czSAaF9ffgZGclCKxO/WIu6pKJmBHaIkU4MiRTOok3JMrO66BQavHHxW/BBC5gA
CiIDEOUMsfnNkjcZ7Tvx5Dq2+UUTJnWvu6rvP3t3O9LEApE9GQDTF1w52z97GA1F
zZOFli9d31kWTz9RvdVFGD/tSo7oBmF0Ixa1DVBzJ0RHfxBdiSprhTEUxOipakyA
vGp4z7h/jnZymQyd/teRCBaho1+V
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEMjCCAxqgAwIBAgIBATANBgkqhkiG9w0BAQUFADB7MQswCQYDVQQGEwJHQjEb
MBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYDVQQHDAdTYWxmb3JkMRow
GAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UEAwwYQUFBIENlcnRpZmlj
YXRlIFNlcnZpY2VzMB4XDTA0MDEwMTAwMDAwMFoXDTI4MTIzMTIzNTk1OVowezEL
MAkGA1UEBhMCR0IxGzAZBgNVBAgMEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE
BwwHU2FsZm9yZDEaMBgGA1UECgwRQ29tb2RvIENBIExpbWl0ZWQxITAfBgNVBAMM
GEFBQSBDZXJ0aWZpY2F0ZSBTZXJ2aWNlczCCASIwDQYJKoZIhvcNAQEBBQADggEP
ADCCAQoCggEBAL5AnfRu4ep2hxxNRUSOvkbIgwadwSr+GB+O5AL686tdUIoWMQua
BtDFcCLNSS1UY8y2bmhGC1Pqy0wkwLxyTurxFa70VJoSCsN6sjNg4tqJVfMiWPPe
3M/vg4aijJRPn2jymJBGhCfHdr/jzDUsi14HZGWCwEiwqJH5YZ92IFCokcdmtet4
YgNW8IoaE+oxox6gmf049vYnMlhvB/VruPsUK6+3qszWY19zjNoFmag4qMsXeDZR
rOme9Hg6jc8P2ULimAyrL58OAd7vn5lJ8S3frHRNG5i1R8XlKdH5kBjHYpy+g8cm
ez6KJcfA3Z3mNWgQIJ2P2N7Sw4ScDV7oL8kCAwEAAaOBwDCBvTAdBgNVHQ4EFgQU
oBEKIz6W8Qfs4q8p74Klf9AwpLQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQF
MAMBAf8wewYDVR0fBHQwcjA4oDagNIYyaHR0cDovL2NybC5jb21vZG9jYS5jb20v
QUFBQ2VydGlmaWNhdGVTZXJ2aWNlcy5jcmwwNqA0oDKGMGh0dHA6Ly9jcmwuY29t
b2RvLm5ldC9BQUFDZXJ0aWZpY2F0ZVNlcnZpY2VzLmNybDANBgkqhkiG9w0BAQUF
AAOCAQEACFb8AvCb6P+k+tZ7xkSAzk/ExfYAWMymtrwUSWgEdujm7l3sAg9g1o1Q
GE8mTgHj5rCl7r+8dFRBv/38ErjHT1r0iWAFf2C3BUrz9vHCv8S5dIa2LX1rzNLz
Rt0vxuBqw8M0Ayx9lt1awg6nCpnBBYurDC/zXDrPbDdVCYfeU0BsWO/8tqtlbgT2
G9w84FoVxp7Z8VlIMCFlA2zs6SFz7JsDoeA3raAVGI/6ugLOpyypEBMs1OUIJqsi
l2D4kF501KKaU73yqWjgom7C12yxow+ev+to51byrvLjKzg6CYG1a4XXvi3tPxq3
smPi9WIsgtRqAEFQ8TmDn5XpNpaYbg==
-----END CERTIFICATE-----)EOF";

#define MAX_HTTP_RECV_BUFFER 1023


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Reallocate response_buffer to fit new data
                char *ptr = realloc(response_buffer, response_len + evt->data_len + 1);
                if (ptr == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
                    return ESP_FAIL;
                }
                response_buffer = ptr;
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
                response_buffer[response_len] = '\0'; // Null-terminate
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}


// Fetch weather data from OpenWeatherMap
static void fetch_weather_data(void) {
    char url[256];
    // snprintf(url, sizeof(url), "https://georgik.rocks/tmp/weather.json");
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&appid=%s&units=metric",
             openweather_city_name, openweather_code, openweather_api_key);

    // Initialize response buffer
    response_buffer = NULL;
    response_len = 0;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .event_handler = _http_event_handler,
        // .cert_pem = openweather_pem, // Make sure this is correctly set
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d", status_code);

        if (status_code == 200) {
            if (response_buffer != NULL) {
                ESP_LOGI(TAG, "Received weather data: %s", response_buffer);
                parse_weather_data(response_buffer);
                free(response_buffer);
                response_buffer = NULL;
                response_len = 0;
            } else {
                ESP_LOGE(TAG, "Response buffer is NULL");
            }
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Parse the JSON weather data
static void parse_weather_data(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *weather_array = cJSON_GetObjectItem(root, "weather");
    if (weather_array != NULL && cJSON_GetArraySize(weather_array) > 0) {
        cJSON *weather = cJSON_GetArrayItem(weather_array, 0);
        cJSON *description = cJSON_GetObjectItem(weather, "description");
        cJSON *icon = cJSON_GetObjectItem(weather, "icon");

        if (description != NULL && icon != NULL) {
            strlcpy(current_weather.description, description->valuestring, sizeof(current_weather.description));
            strlcpy(current_weather.icon, icon->valuestring, sizeof(current_weather.icon));
        }
    }

    cJSON *main = cJSON_GetObjectItem(root, "main");
    if (main != NULL) {
        cJSON *temp = cJSON_GetObjectItem(main, "temp");
        cJSON *pressure = cJSON_GetObjectItem(main, "pressure");
        cJSON *humidity = cJSON_GetObjectItem(main, "humidity");

        if (temp != NULL) current_weather.temperature = temp->valuedouble;
        if (pressure != NULL) current_weather.pressure = pressure->valueint;
        if (humidity != NULL) current_weather.humidity = humidity->valueint;
    }

    // Parse sunrise and sunset
    cJSON *sys = cJSON_GetObjectItem(root, "sys");
    if (sys != NULL) {
        cJSON *sunrise = cJSON_GetObjectItem(sys, "sunrise");
        cJSON *sunset = cJSON_GetObjectItem(sys, "sunset");

        if (sunrise != NULL) {
            current_weather.sunrise = (time_t)sunrise->valueint; // Store as time_t
            struct tm *sunrise_tm = localtime(&current_weather.sunrise);
            if (sunrise_tm != NULL) {
                current_weather.sunrise_hour = sunrise_tm->tm_hour;
                current_weather.sunrise_minute = sunrise_tm->tm_min;
            }
        }

        if (sunset != NULL) {
            current_weather.sunset = (time_t)sunset->valueint; // Store as time_t
            struct tm *sunset_tm = localtime(&current_weather.sunset);
            if (sunset_tm != NULL) {
                current_weather.sunset_hour = sunset_tm->tm_hour;
                current_weather.sunset_minute = sunset_tm->tm_min;
            }
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Parsed weather data:");
    ESP_LOGI(TAG, "Description: %s", current_weather.description);
    ESP_LOGI(TAG, "Icon: %s", current_weather.icon);
    ESP_LOGI(TAG, "Temperature: %.2f", current_weather.temperature);
    ESP_LOGI(TAG, "Pressure: %d", current_weather.pressure);
    ESP_LOGI(TAG, "Humidity: %d", current_weather.humidity);
    ESP_LOGI(TAG, "Sunrise: %02d:%02d", current_weather.sunrise_hour, current_weather.sunrise_minute);
    ESP_LOGI(TAG, "Sunset: %02d:%02d", current_weather.sunset_hour, current_weather.sunset_minute);
}


// Initialize SDL, create window and renderer, load font
static void initialize_sdl() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        ESP_LOGE(TAG, "Unable to initialize SDL: %s", SDL_GetError());
        return;
    }

     window = SDL_CreateWindow("SDL on ESP32", BSP_LCD_H_RES, BSP_LCD_V_RES, 0);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        return;
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return;
    }

    if (!TTF_Init()) {
        ESP_LOGE(TAG, "Failed to initialize TTF: %s", SDL_GetError());
        return;
    }

    font = TTF_OpenFont("/assets/FreeSans.ttf", 24); // Ensure the font file is available
    if (!font) {
        ESP_LOGE(TAG, "Failed to open font: %s", SDL_GetError());
        return;
    }
}


// Application
void* sdl_thread(void* args) {
    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    SDL_InitFS();
    initialize_sdl();

    // Synchronize time using SNTP
    time_sync();

    // Fetch weather data
    fetch_weather_data();

    ESP_LOGI(TAG, "Shutdown WiFi");
    esp_wifi_stop();
    esp_wifi_deinit();

    // Clean up
    // if (font) TTF_CloseFont(font);
    // TTF_Quit();
    // if (renderer) SDL_DestroyRenderer(renderer);
    // if (window) SDL_DestroyWindow(window);
    // SDL_Quit();

    // Render weather data
    render_weather_data(renderer, font);
    ESP_LOGI(TAG, "Finished rendering. ");

    // Optionally, enter deep sleep or restart
    while(1) {

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    // esp_restart();
}


void app_main(void) {
    pthread_t sdl_pthread;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 32000);  // Set the stack size for the thread

    int ret = pthread_create(&sdl_pthread, &attr, sdl_thread, NULL);
    if (ret != 0) {
        printf("Failed to create SDL thread: %d\n", ret);
        return;
    }

    pthread_detach(sdl_pthread);
}
