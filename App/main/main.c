/* 
    InfoGateway Application
    Author: Arpan Patel
    Date: Oct 2, 2023
*/
#include "main.h"

static const char *TAG = "AppMain";
#define MOTOR_PIN               (GPIO_NUM_26)
#define STAY_AWAKE_TIME_SEC     (5 * 60 * 1000)  //12 * 6 * 60 = 720000 = 12 Mins


#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL);
   ESP_LOGI(TAG, "Time is synchronized from custom code");
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

/* TODO: If Wifi Fails, retry after every 5 mins or on button event.
   Case1: Wifi doesn't connect at start
   Case2: Wifi disconnects in between
*/
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < AP_MAXIMUM_RETRY) {            
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = AP_WIFI_SSID,
            .password = AP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by wifi_event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to AP SSID:%s", AP_WIFI_SSID);  
        //ESP_LOGI(TAG, "connected to AP SSID:%s password:%s", AP_WIFI_SSID, AP_WIFI_PASS);        
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", AP_WIFI_SSID, AP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

//timer to print real world time every second in log, for test purpose.
//later to be shown on the display.
void real_world_time_Timer(TimerHandle_t xTimer)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in India is: %s", strftime_buf);
}

#if 0 //DEEP_SLEEP_TASK
static void deep_sleep_task(void *args)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: {
            printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
            break;
        }
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

#if CONFIG_IDF_TARGET_ESP32
    // Isolate GPIO12 pin from external circuits. This is needed for modules
    // which have an external pull-up resistor on GPIO12 (such as ESP32-WROVER)
    // to minimize current consumption.
    rtc_gpio_isolate(GPIO_NUM_12);
#endif
    
    //tried to turn off the display but not successful.
    //backlight before deep sleep.
    //bsp_display_lock(500);
    //bsp_display_backlight_off();

    printf("Entering deep sleep\n");

    // get deep sleep enter time
    gettimeofday(&sleep_enter_time, NULL);

    // enter deep sleep
    esp_deep_sleep_start();
}

static void deep_sleep_register_rtc_timer_wakeup(void)
{
    const int wakeup_time_sec = 60;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}
#endif //DEEP_SLEEP_TASK

void app_main(void)
{
    ++boot_count;
    ESP_LOGI(TAG, "App Started.");
    ESP_LOGI(TAG, "Boot Count. [%d]", boot_count);

    // Set your desired wake-up time (24-hour format)
    const int wakeHour = 8;    // 8:00 AM
    const int wakeMinute = 0;  // 0 minutes
    const int wakeSecond = 0;  // 0 seconds

#if 0 //DEEP_SLEEP_TASK    
    /* Enable wakeup from deep sleep by rtc timer */
    deep_sleep_register_rtc_timer_wakeup();
#endif

    time_t now;
    struct tm timeinfo;

//1. Wifi Enable
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

//1. Time SNTP
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }

#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    else {
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(TAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
            int64_t error_time = cpu_time + 500 * 1000L;
            struct timeval tv_error = { .tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L };
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "IST-5:30", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in India is: %s", strftime_buf);

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
            adjtime(NULL, &outdelta);
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %jd sec: %li ms: %li us",
                        (intmax_t)outdelta.tv_sec,
                        outdelta.tv_usec/1000,
                        outdelta.tv_usec%1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }

    // Get the current time
    struct tm new_timeinfo;
    time(&now);
    localtime_r(&now, &new_timeinfo);

    // Calculate the next wake-up time
    struct tm nextWakeTime = new_timeinfo;
    nextWakeTime.tm_hour = wakeHour; 
    nextWakeTime.tm_min = wakeMinute;
    nextWakeTime.tm_sec = wakeSecond;

    // If the next wake-up time is in the past, add 24 hours
    time_t currentTime = mktime(&new_timeinfo);
    time_t nextWakeTimestamp = mktime(&nextWakeTime);
    if (nextWakeTimestamp <= currentTime) {
        nextWakeTimestamp += 24 * 3600; // Add 24 hours
    }

    // Calculate sleep duration until the next wake-up time
    uint64_t sleepDuration = (nextWakeTimestamp - currentTime) * 1000000;

    // Configure ESP32 to wake up at the specified time
    esp_sleep_enable_timer_wakeup(sleepDuration);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &nextWakeTime);
    ESP_LOGI(TAG, "The Wake Up date/time in India is: %s", strftime_buf);

    //3. Display Enable
    bsp_display_start();
    ESP_LOGI(TAG, "Display Start");
    bsp_display_lock(0);    
    show_current_time_lvgl();
    bsp_display_unlock();
    bsp_display_backlight_on();
    
    if (isWifiConnected() && isInternetAvailable()) {
        get_weather_data();
    }

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE; // Disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT; // Set as output mode
    io_conf.pin_bit_mask = (1ULL << MOTOR_PIN); // Bitmask for the pin
    io_conf.pull_down_en = 0; // Disable pull-down
    io_conf.pull_up_en = 0; // Disable pull-up
    gpio_config(&io_conf); // Configure the GPIO pin
  
    ESP_LOGI(TAG, "Testing DC Motor...\n");

    // Turn on the motor //relay is Negative logic, LOW is ON, HIGH is OFF
    gpio_set_level(MOTOR_PIN, 0);

    // Stay awake for __ minutes (____ seconds)
    vTaskDelay(STAY_AWAKE_TIME_SEC / portTICK_PERIOD_MS);

    // Turn off the motor //relay is Negative logic, LOW is ON, HIGH is OFF
    gpio_set_level(MOTOR_PIN, 1);

    // Put the ESP32 into deep sleep mode
    ESP_LOGI(TAG, "Enter Deep Sleep");
    esp_deep_sleep_start();

#if 0 //DEEP_SLEEP_TASK
    vTaskDelay(1250 / portTICK_PERIOD_MS);
    xTaskCreate(deep_sleep_task, "deep_sleep_task", 4096, NULL, 6, NULL);
#endif
    
    ESP_LOGI(TAG, "App Ended.");
}

void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (esp_sntp_getservername(i)){
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

bool isWifiConnected()
{
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    return ap_info.primary != 0;
}
 
bool isInternetAvailable()
{
    esp_http_client_config_t config = {
        .url = "http://clients3.google.com/generate_204",
        .method = HTTP_METHOD_HEAD,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return err == ESP_OK;
}

//URL = https://api.openweathermap.org/data/3.0/weather?q=city_name&appid=api_key&units=metric

void get_temp(char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    if (cJSON_IsInvalid(root)) {
        ESP_LOGE(TAG,"Invalid Json data!!");
        return;
    }
    cJSON *obj_main = cJSON_GetObjectItemCaseSensitive(root, "main");
    if (cJSON_IsInvalid(obj_main)) {
        ESP_LOGE(TAG, "Error: %s is missing.", "main");        
        return;
    }

    cJSON_Print(obj_main);

    cJSON *obj_temp = cJSON_GetObjectItemCaseSensitive(obj_main, "temp");
    if (cJSON_IsInvalid(obj_temp)) {
        ESP_LOGE(TAG, "Error: %s is missing.", "temp");
        return;
    }
    if (!cJSON_IsNumber(obj_temp)) {
        ESP_LOGE(TAG, "Error: %s is not a number.", "temp");
        return;
    }
    double temp = obj_temp->valuedouble;
    ESP_LOGI(TAG,"Temperature: %0.00f °C", temp);
    
    if (root != NULL)
        cJSON_Delete(root);

    /* if (json_string != NULL)
        free(json_string); */
}

esp_err_t get_weather_data_client_handler(esp_http_client_event_t *event)
{
    switch (event->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Resize the buffer to fit the new chunk of data
            memcpy(response_data_weather_api + response_len_weather_api, event->data, event->data_len);
            response_len_weather_api += event->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:            
            //ESP_LOGI("OpenWeatherAPI", "Received len: %u", response_len_weather_api);
            get_temp(response_data_weather_api);
            memset(response_data_weather_api, 0, 1024); //clear the buffer before reuse.
            response_len_weather_api = 0; //reset the length to avoid overflow
            break;
        default:
            break;
    }
    return ESP_OK;
}

void get_weather_data() {    

    //clear the buffer before use.
    memset(response_data_weather_api, 0, 1024); //0 all bytes.
    response_len_weather_api = 0;

    esp_http_client_config_t config = {
        .url = "http://api.openweathermap.org/data/2.5/weather?q=Pune&units=metric&appid=81735552c309dfe587cd3a64fe13b016",
        .method = HTTP_METHOD_GET,
        .event_handler = get_weather_data_client_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code != 200)        
        {
            ESP_LOGI(TAG, "OpenWeather API Message sent Failed");
        }
    }
    esp_http_client_cleanup(client);
}

void obtain_time(void)
{    
#if LWIP_DHCP_GET_NTP_SRV
    /**
     * NTP server address could be acquired via DHCP,
     * see following menuconfig options:
     * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
     * 'LWIP_SNTP_DEBUG' - enable debugging messages
     *
     * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
     * otherwise NTP option would be rejected by default.
     */
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
    config.start = false;                       // start SNTP service explicitly (after connecting)
    config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // configure the event on which we renew servers
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
#else
    config.ip_event_to_renew = IP_EVENT_ETH_GOT_IP;
#endif
    config.sync_cb = time_sync_notification_cb; // only if we need the notification function
    esp_netif_sntp_init(&config);

#endif /* LWIP_DHCP_GET_NTP_SRV */

#if LWIP_DHCP_GET_NTP_SRV
    ESP_LOGI(TAG, "Starting SNTP");
    esp_netif_sntp_start();
#if LWIP_IPV6 && SNTP_MAX_SERVERS > 2
    /* This demonstrates using IPv6 address as an additional SNTP server
     * (statically assigned IPv6 address is also possible)
     */
    ip_addr_t ip6;
    if (ipaddr_aton("2a01:3f7::1", &ip6)) {    // ipv6 ntp source "ntp.netnod.se"
        esp_sntp_setserver(2, &ip6);
    }
#endif  /* LWIP_IPV6 */

#else
    ESP_LOGI(TAG, "Initializing and starting SNTP");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    /* This demonstrates configuring more than one server
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                               ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org" ) );
#else
    /*
     * This is the basic default config with one server and starting the service
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
#endif
    config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    config.smooth_sync = true;
#endif

    esp_netif_sntp_init(&config);
#endif

    print_servers();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    time(&now);
    localtime_r(&now, &timeinfo);

}
