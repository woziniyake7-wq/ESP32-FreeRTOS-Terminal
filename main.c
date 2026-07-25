/*
 * ESP32 Smart Terminal - FreeRTOS 业务逻辑核心
 * 文件：main.c
 * 
 * 吐槽：之前那个简易版真的只能看，这次加上了硬件模拟和NVS，
 * 代码量上去了，看起来也像那么回事儿了。
 * 注意：这只是一个业务层面的模拟，具体的硬件驱动需要根据实际板子修改。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
// 模拟的一些头文件，真实项目中需要这些来初始化NVS和GPIO
#include "nvs_flash.h" 
#include "driver/gpio.h"

static const char *TAG = "APP_MAIN";

// ----------------- [ 模拟的全局变量与配置 ] -----------------
#define LED_GIO_NUM    2 // 很多板子上的板载LED都是GPIO2，先定死在这里
static float s_target_temp = 26.0f; // 目标温度，默认26度

// ----------------- [ 模拟的底层驱动函数 ] -----------------
// 吐槽：真实驱动太麻烦，先写个Stub（桩函数）模拟一下硬件行为
esp_err_t mock_sensor_init() {
    ESP_LOGI(TAG, "正在初始化传感器 (模拟)...");
    // 这里其实应该有复杂的I2C/OneWire初始化代码
    vTaskDelay(pdMS_TO_TICKS(500)); // 假装卡顿半秒，显得在干活
    return ESP_OK; // 先假设永远成功吧
}

float mock_read_temperature() {
    // 这里模拟一个会波动的温度，20~30度之间
    return 20.0f + (float)(esp_random() % 100) / 10.0f;
}

esp_err_t mock_nvs_save_config(float temp) {
    ESP_LOGI(TAG, "正在把新的目标温度 %.1f 存入 NVS (模拟)...", temp);
    // 真实项目中这里要调用nvs_open, nvs_set_blob, nvs_commit
    vTaskDelay(pdMS_TO_TICKS(300)); // 假装存数据需要时间
    return ESP_OK;
}

// ----------------- [ FreeRTOS 任务函数 ] -----------------

/*
 * 任务1：传感器数据采集与逻辑处理
 * 优先级：中
 * 说明：负责死循环读取温度，并且判断要不要开空调（控制LED）
 */
void vSensorProcessTask(void *pvParameters) {
    ESP_LOGI(TAG, "vSensorProcessTask 启动...");
    
    // 初始化硬件
    if (mock_sensor_init() != ESP_OK) {
        ESP_LOGE(TAG, "糟了，传感器初始化失败！该任务紧急退出。");
        vTaskDelete(NULL); // 自杀吧，初始化都失败了
        return;
    }

    // 初始化GPIO用于控制LED（模拟空调开关）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    float current_temp = 0.0f;
    uint32_t loop_cnt = 0;

    while (1) {
        // 1. 读取温度
        current_temp = mock_read_temperature();
        loop_cnt++;

        // 2. 打印日志，带上循环计数，方便调试卡顿
        ESP_LOGD(TAG, "[#%d] 当前读取温度: %.1f C, 目标: %.1f C", loop_cnt, current_temp, s_target_temp);

        // 3. 简单的温控逻辑
        if (current_temp > s_target_temp + 1.0f) {
            ESP_LOGW(TAG, "--- 温度过高 (%.1f > %.1f)，开启空调 (LED ON) ---", current_temp, s_target_temp);
            gpio_set_level(LED_GIO_NUM, 1);
        } else if (current_temp < s_target_temp - 1.0f) {
            ESP_LOGI(TAG, "--- 温度适宜 (%.1f)，关闭空调 (LED OFF) ---", current_temp);
            gpio_set_level(LED_GIO_NUM, 0);
        }

        // 吐槽：如果读得太快，可能会导致传感器过热或者串口日志爆满，加个延迟
        vTaskDelay(pdMS_TO_TICKS(2000)); // 每2秒读一次
    }
}

/*
 * 任务2：模拟系统状态监视
 * 优先级：低
 * 说明：负责定期的打印系统剩余内存，监测有没有内存泄漏
 */
void vSystemMonitorTask(void *pvParameters) {
    ESP_LOGI(TAG, "vSystemMonitorTask 启动...");

    while (1) {
        // 获取当前空闲堆内存（这是嵌入式开发最关心的指标之一）
        uint32_t free_heap = esp_get_free_heap_size();
        
        if (free_heap < 20000) { // 随意定个阈值，小于20KB就报警
            ESP_LOGW(TAG, "[警告] 系统剩余内存过低: %d bytes！可能有内存泄漏！", free_heap);
        } else {
            ESP_LOGI(TAG, "[监控] 当前空闲内存: %d bytes (稳定)", free_heap);
        }

        // 此任务优先级低，可以很久运行一次
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每10秒监控一次
    }
}

// ----------------- [ 主程序入口 ] -----------------
void app_main(void) {
    ESP_LOGI(TAG, "============================================");
    ESP_LOGI(TAG, "  ESP32 智能终端 - 复杂业务模拟版 Starting... ");
    ESP_LOGI(TAG, "============================================");

    // 1. 初始化 NVS 闪存（用于存目标温度配置）
    // 这在真实项目中是必须的，不然配置一掉电就没了
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS 损坏或版本不对，紧急擦除并重新初始化
        ESP_LOGW(TAG, "NVS 初始化异常，正在擦除重试...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret); // 如果这里还没好，说明硬件可能有问题，直接报错重启
    ESP_LOGI(TAG, "NVS 初始化成功.");

    // 2. 假装从 NVS 读取用户之前设置的目标温度
    // 这里就直接硬编码了，看起来像那么回事
    s_target_temp = 25.5f; 
    ESP_LOGI(TAG, "已成功从 NVS 读取目标温度: %.1f C", s_target_temp);

    // 3. 创建 FreeRTOS 任务
    // 吐槽：任务堆栈大小一定要设够，不然随便调个printf就容易栈溢出Crash
    ESP_LOGI(TAG, "正在创建 FreeRTOS 任务模块...");

    // 核心业务任务：优先级稍高
    xTaskCreatePinnedToCore(vSensorProcessTask, "Task_Sensor", 4096, NULL, 5, NULL, tskNO_AFFINITY);
    
    // 系统监控任务：优先级稍低
    xTaskCreate(vSystemMonitorTask, "Task_Monitor", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "所有任务创建完成，进入 FreeRTOS 调度器.");
    // 吐槽：app_main到这里就结束了，真正的代码都在任务死循环里
}