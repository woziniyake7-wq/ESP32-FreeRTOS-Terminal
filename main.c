/**
 * @file main.c
 * @brief ESP32 Smart Terminal Core Application (FreeRTOS Architecture)
 * @details 实现了基于 FreeRTOS 的传感器数据采集、系统日志监控与 NVS 配置存储。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

static const char *TAG = "APP_MAIN";

// ----------------- [ 系统参数与硬件配置 ] -----------------
#define BOARD_STATUS_LED_GPIO   GPIO_NUM_2  
static float s_target_temperature = 26.0f;  

// ----------------- [ 硬件抽象与 Stub 接口 ] -----------------

/**
 * @brief 初始化传感器外设 ( Stub 实现 )
 */
static esp_err_t sensor_hardware_init(void) {
    ESP_LOGI(TAG, "Initializing hardware sensors...");
    vTaskDelay(pdMS_TO_TICKS(100)); 
    return ESP_OK;
}

/**
 * @brief 读取传感器温度数据
 * @return float 当前温度测得值
 */
static float sensor_read_temperature(void) {
    // 模拟采样波动数据 (20.0℃ ~ 30.0℃)
    return 20.0f + (float)(esp_random() % 100) / 10.0f;
}

// ----------------- [ FreeRTOS 业务任务 ] -----------------

/**
 * @brief 传感器数据采集与状态控制任务
 * @param pvParameters 任务传入参数
 */
static void vSensorProcessTask(void *pvParameters) {
    ESP_LOGI(TAG, "vSensorProcessTask started.");

    if (sensor_hardware_init() != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed! Terminating task.");
        vTaskDelete(NULL);
        return;
    }

    // 配置状态指示灯 GPIO 输出
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOARD_STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    float current_temp = 0.0f;

    while (1) {
        current_temp = sensor_read_temperature();

        if (current_temp > s_target_temperature + 1.0f) {
            ESP_LOGW(TAG, "Temperature high (%.1f C > %.1f C). Triggering active cooling.", 
                     current_temp, s_target_temperature);
            gpio_set_level(BOARD_STATUS_LED_GPIO, 1);
        } else if (current_temp < s_target_temperature - 1.0f) {
            ESP_LOGI(TAG, "Temperature nominal (%.1f C). Deactivating cooling.", current_temp);
            gpio_set_level(BOARD_STATUS_LED_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief 系统运行状态与堆内存监控任务
 * @param pvParameters 任务传入参数
 */
static void vSystemMonitorTask(void *pvParameters) {
    ESP_LOGI(TAG, "vSystemMonitorTask started.");

    while (1) {
        uint32_t free_heap = esp_get_free_heap_size();
        
        if (free_heap < 20000) {
            ESP_LOGW(TAG, "Low memory alert! Free Heap: %d bytes", free_heap);
        } else {
            ESP_LOGI(TAG, "System Health Check - Free Heap: %d bytes", free_heap);
        }

        vTaskDelay(pdMS_TO_TICKS(10000)); // 10s 监测周期
    }
}

// ----------------- [ 主程序入口 ] -----------------
void app_main(void) {
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  ESP32 Terminal Core System Initializing...  ");
    ESP_LOGI(TAG, "============================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated/corrupted, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS storage initialized successfully.");

    xTaskCreatePinnedToCore(
        vSensorProcessTask, 
        "Task_Sensor", 
        4096, 
        NULL, 
        5, 
        NULL, 
        tskNO_AFFINITY
    );
    
    xTaskCreate(
        vSystemMonitorTask, 
        "Task_Monitor", 
        2048, 
        NULL, 
        2, 
        NULL
    );

    ESP_LOGI(TAG, "FreeRTOS scheduler started.");
}
