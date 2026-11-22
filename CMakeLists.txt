# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(KinBox)

FILE(GLOB app_sources
    src/*.c
    kindrals/*.c
    actions/*.c
    battle/*.c
    storage/*.c
    net/*.c
    ui/*.c
    sprites/*.c
)

target_sources(app PRIVATE ${app_sources})

# ESP32-S3 deep-sleep support missing from the Zephyr HAL source list
if(CONFIG_KINBOX_DEEP_SLEEP AND NOT CONFIG_PM AND NOT CONFIG_POWEROFF)
    set(ESP_IDF_COMPONENTS
        "${ZEPHYR_BASE}/../modules/hal/espressif/components"
    )

    target_sources(app PRIVATE
        ${ESP_IDF_COMPONENTS}/esp_hw_support/sleep_gpio.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/sleep_event.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/sleep_console.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/sleep_uart.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/clk_utils.c
        ${ESP_IDF_COMPONENTS}/esp_driver_gpio/src/gpio.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/sleep_modem.c
        ${ESP_IDF_COMPONENTS}/esp_hal_touch_sens/touch_sens_hal.c
        ${ESP_IDF_COMPONENTS}/esp_hal_touch_sens/esp32s3/touch_sensor_legacy_hal.c
        ${ESP_IDF_COMPONENTS}/esp_hw_support/lowpower/port/esp32s3/sleep_cpu.c
        ${ESP_IDF_COMPONENTS}/esp_hal_pmu/esp32s3/rtc_cntl_hal.c
    )
endif()

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
)
