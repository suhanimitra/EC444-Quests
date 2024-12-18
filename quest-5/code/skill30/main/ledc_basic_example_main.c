/* tt-motor-ledc adapted from LEDC from ESP32 examples/peripherals
   20224-11-09

   Edits reflect tested settings (f=50Hz, duty cycle 0--1023). Drives
   one motor with single speed at 50% duty cycle without direction
   control.
   ============   
   LEDC (LED Controller) basic example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO_0          (5) // Define the output GPIO for the first motor  --> LEFT MOTOR (0) 
#define LEDC_OUTPUT_IO_1          (18) // Define the output GPIO for the second motor --> RIGHT MOTOR (1)
#define LEFT_IN1    12
#define LEFT_IN2    14
#define RIGHT_IN1   32
#define RIGHT_IN2   27

//new pins
/*
Input 1 –> 12
Input2 —> 14
Input3 —> 32
Input4 —> 27
Pwn/ enable/ 1,2 —> 5 
Pwn/enable/3,4 –> 18
*/


#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT // Set duty resolution to 10 bits
#define LEDC_DUTY               (512) // Set duty to 50%. (2 ** 10) * 50% = 512
#define LEDC_FREQUENCY          (50) // Frequency in Hertz. Set frequency at 50 Hz

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */

static void configure_gpio(void)
{
    gpio_reset_pin(LEFT_IN1);
    gpio_reset_pin(LEFT_IN2);
    gpio_reset_pin(RIGHT_IN1);
    gpio_reset_pin(RIGHT_IN2);
    gpio_reset_pin(LEDC_OUTPUT_IO_0);
    gpio_reset_pin(LEDC_OUTPUT_IO_1);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LEFT_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LEFT_IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(RIGHT_IN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(RIGHT_IN2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LEDC_OUTPUT_IO_0, GPIO_MODE_OUTPUT);
    gpio_set_direction(LEDC_OUTPUT_IO_1, GPIO_MODE_OUTPUT);
}

static void example_ledc_init_0(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO_0,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void example_ledc_init_1(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER_1,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_1,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO_1,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}


void forward(void){
    //might need to change after testing
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 1);
    gpio_set_level(RIGHT_IN1, 1);
    gpio_set_level(RIGHT_IN2, 0);

     // Set duty to 50%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, LEDC_DUTY));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}

void turn_left(void) {
    //might need to change after testing
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN1, 1);
    gpio_set_level(RIGHT_IN2, 0);

    // Left motor stops or reverses (duty cycle = 0 for reverse)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0)); // Stop/Reverse the left motor
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    
    // Right motor moves forward (50% duty cycle)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, LEDC_DUTY)); // Forward speed
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}

void turn_right(void) {
    //might need to change after testing
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 1);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 0);

    // Left motor moves forward (50% duty cycle)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, LEDC_DUTY)); // Forward speed
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));

    // Right motor stops or reverses (duty cycle = 0 for reverse)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0)); // Stop/Reverse the right motor
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}


void reverse(void){
    gpio_set_level(LEFT_IN1, 1);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 1);

    // Set duty to 50%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, LEDC_DUTY));

    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}


void stop(void){
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 0);

    // Set duty to 0%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}


void app_main(void)
{
    // Set the LEDC peripheral configuration for both motors
    example_ledc_init_0();
    example_ledc_init_1();
    configure_gpio();

    gpio_set_level(LEDC_OUTPUT_IO_0, 1);
    gpio_set_level(LEDC_OUTPUT_IO_1, 1);
    
while (1) {
    // Example usage: Move forward
    forward();
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Move forward for 2 seconds

    // Example usage: Turn left
    turn_left();
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Turn for 2 seconds

    // Example usage: Turn right
    turn_right();
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Turn for 2 seconds

    reverse();
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Stop for 2 seconds

    // Example usage: Stop
    stop();
    vTaskDelay(2000 / portTICK_PERIOD_MS); // Stop for 2 seconds
}
}
