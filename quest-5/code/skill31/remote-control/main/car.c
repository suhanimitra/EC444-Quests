#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "driver/ledc.h"
#include "esp_err.h"

#ifdef CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN
#include "addr_from_stdin.h"
#endif

#if defined(CONFIG_EXAMPLE_IPV4)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#elif defined(CONFIG_EXAMPLE_IPV6)
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#else
#define HOST_IP_ADDR ""
#endif


#define PORT 1111 // Port number for the UDP server
#define LED_GPIO_PIN 13 // GPIO pin number for LED

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

static const char *TAG_UDP = "udp_server";

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


// Function to handle UDP server tasks
static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG_UDP, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG_UDP, "Socket created");

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG_UDP, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG_UDP, "Socket bound, port %d", PORT);

    while (1) {
        ESP_LOGI(TAG_UDP, "Waiting for data");
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        // Error occurred during receiving
        if (len < 0) {
            ESP_LOGE(TAG_UDP, "recvfrom failed: errno %d", errno);
            break;
        }
        // Data received
        else {
            rx_buffer[len] = 0; // Null-terminate received data
            ESP_LOGI(TAG_UDP, "Received %d bytes from %s:", len, addr_str);
            ESP_LOGI(TAG_UDP, "%s", rx_buffer);

            // Check if the message is to turn on the LED
            if (strncmp(rx_buffer, "w", len) == 0) {
                ESP_LOGI(TAG_UDP, "Received W");
                forward();

                // Send acknowledgment back to the client
                const char *ack_msg = "Received W";
                sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            } else if (strncmp(rx_buffer, "a", len) == 0) {
                ESP_LOGI(TAG_UDP, "Received A");
                turn_left();
                // Send acknowledgment back to the client
                const char *ack_msg = "Received A";
                sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            } else if (strncmp(rx_buffer, "s", len) == 0) {
                ESP_LOGI(TAG_UDP, "Received A");
                reverse();
                // Send acknowledgment back to the client
                const char *ack_msg = "Received A";
                sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            } else if (strncmp(rx_buffer, "d", len) == 0) {
                ESP_LOGI(TAG_UDP, "Received D");
                turn_right();
                // Send acknowledgment back to the client
                const char *ack_msg = "Received D";
                sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
        }
    }

    if (sock != -1) {
        ESP_LOGE(TAG_UDP, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Initialize the NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect()); // Connect to WiFi

    example_ledc_init_0();
    example_ledc_init_1();
    configure_gpio();

    gpio_set_level(LEDC_OUTPUT_IO_0, 1);
    gpio_set_level(LEDC_OUTPUT_IO_1, 1);

    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}
