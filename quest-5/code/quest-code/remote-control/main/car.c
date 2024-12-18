#include <string.h>
#include <math.h>
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
#include <math.h>
#include "sdkconfig.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "driver/ledc.h"
#include "esp_err.h"

// adding in IR things
#include "driver/adc.h"
#include "esp_adc_cal.h"
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling
static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


#ifdef CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN
#include "addr_from_stdin.h"
#endif

// #if defined(CONFIG_EXAMPLE_IPV4)
// #define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
// #elif defined(CONFIG_EXAMPLE_IPV6)
// #define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
// #else
// #define HOST_IP_ADDR ""
// #endif


//menu config with router WIFI - TP-LINK_EE49
#define OPTI_ADDR "192.168.0.167"
#define OPTI_PORT 41234


//for WASD, trying menuconfig with ESP IP
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
#define LEDC_DUTY               (256) // Set duty to 50%. (2 ** 10) * 50% = 512
#define LEDC_FREQUENCY          (50) // Frequency in Hertz. Set frequency at 50 Hz

static const char *TAG_UDP = "udp_server";

// adding in optitrack

static const char *TAG = "example";
//static const char *payload = "ROBOTID 11";

static const char *payload = "ROBOTID 12";

typedef struct Waypoint Waypoint;

struct Waypoint {
    int id;
    float x;
    float z;
    Waypoint *next;
};

typedef struct {
    int id;
    float x;
    float z;
    float theta;
    char status[10];
} RobotData;

//make global for access from self_drive
RobotData data;
float dist;
TaskHandle_t self_drive_task_handle = NULL;

int parseRobotData(const char *input, RobotData *data) {
    // Parse the input string and populate the RobotData structure
    int parsed = sscanf(input, "%d,%f,%f,%f,%9s", &data->id, &data->x, &data->z, &data->theta, data->status);

    // Check if all fields were successfully parsed
    if (parsed == 5) {
        return 1; // Success
    } else {
        return 0; // Parsing failed
    }
}


static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}


static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}


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


void reverse(void){
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
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 400)); // Forward speed
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}

void turn_right(void) {
    //might need to change after testing
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 1);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 0);

    // Left motor moves forward (50% duty cycle)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 256)); // Forward speed
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));

    // Right motor stops or reverses (duty cycle = 0 for reverse)
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0)); // Stop/Reverse the right motor
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
}


void forward(void){
    gpio_set_level(LEFT_IN1, 1);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN1, 0);
    gpio_set_level(RIGHT_IN2, 1);

    // Set duty to 50%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 256));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 256)); //left wheel

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

void turn(float angle_to_turn) {

    //if positive, turn right, if negative, turn left

    if (angle_to_turn > 15.0) {      //I chose 3 b/c if its less than that I feel like turning wont rly help
        turn_right();
        vTaskDelay(150 / portTICK_PERIOD_MS);
        stop();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    } else if (angle_to_turn <= -15.0) {
        turn_left();
        vTaskDelay(150 / portTICK_PERIOD_MS);
        stop();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }  else {
        //forward();
        reverse();
        vTaskDelay(70 / portTICK_PERIOD_MS);
    }
}

void calcroute( RobotData data, Waypoint curr_waypoint) {
    //given the current position by robot data
    //and the current waypoint
    //calculate any angle changes, move forward
    //calcuate diff in x
    float diffx = curr_waypoint.x - data.x;
    float diffz = curr_waypoint.z - data.z;
    //float angle_rad = atan2f(-diffx, -diffx);
    float angle_rad = atan2f(diffz, diffx);
    float angle_deg = angle_rad * (180.0 / M_PI);

    float abs_degree = fabs(angle_deg);
    float abs_angle = 0;

    //if in quadrant 4, abs_angle = |arctanagnle| + 270
    //angle to turn = abs_angle - theta
    //if in quadrant 3 abs_angle = |arctanagnle| - 90
    //angle to turn = abs_angle - theta (if negative, turn left, else turn right)
    //if in quadrant 2, abs_angle = 270 - |arctangle|
    //angle to turn = abs_angle - theta (if negative, turn left, else turn right)
    //if in quadrant 1, abs_angle = 270 - |arctangle|

    if (curr_waypoint.z >= 0 && curr_waypoint.x >= 0) {                               //quadrants 1 and 2
        abs_angle = 270 - abs_degree;
    } else if (curr_waypoint.x < 0 && curr_waypoint.z >= 0) {     
        abs_angle = 270 - abs_degree;
    } else if (curr_waypoint.x < 0 && curr_waypoint.z < 0) {          //quadrant 3
        abs_angle = abs_degree - 90;
    } else if (curr_waypoint.x >=0 && curr_waypoint.z <0) {           //quadrant 4
        abs_angle = abs_degree + 250;
    }

    float angle_to_turn = abs_angle - data.theta;

    printf("diffx: %f, diffy: %f, angle_deg: %f, anglediff: %f, angle_to_turn: %f\n", diffx, diffz, angle_deg, abs_degree, angle_to_turn);


    //now that we have the angle we need to turn, do the actual turn;
    turn(angle_to_turn);
}

void self_drive(void) {
    // Default to self-driving behavior
    // Add in waypoints here to traverse the course

    //initialize waypoints
    struct Waypoint w1 = { 1, -693, -427, NULL }; //bottom left
    struct Waypoint w2 = { 2, 745, -467, NULL }; //bottom right 
    struct Waypoint w3 = { 3, -728, 730, NULL }; //top left assigned: -728, 730 actual: -708, 716
    struct Waypoint w4 = { 4, 765, 748, NULL}; //top right assigned: 765, 748 actual: 724, 752
    

    w1.next = &w2;
    w2.next = &w3;
    w3.next = &w4;
    w4.next = &w1;

    struct Waypoint curr_waypoint = w1;
    
    while(1) {
        //change angle, go forward
        if (dist < 30){
            printf("distance less than 30\n");
            stop();
        } else {
            calcroute(data, curr_waypoint);
        }
        
        if ((fabs(curr_waypoint.x - data.x) <= 110.0 && fabs(curr_waypoint.z - data.z) <= 60.0) || (fabs(curr_waypoint.x - data.x) <= 100.0 && fabs(curr_waypoint.z - data.z) <= 100.0)) { //changed from and to or and from 100 to 80
            // Move to the next waypoint
            stop();
            curr_waypoint = *(curr_waypoint.next);
            printf("reached the corner!\n");
            printf("new id: %d\n", curr_waypoint.id);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        printf("Current waypoint: %d, coordinates: %f, %f\n", curr_waypoint.id, curr_waypoint.x, curr_waypoint.z);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
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

    bool wasd_mode = false; // Track mode: false = self-driving, true = WASD mode

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

            if (strncmp(rx_buffer, "g", len) == 0) {
                ESP_LOGI(TAG_UDP, "Entering WASD mode");
                wasd_mode = true;
                if (self_drive_task_handle != NULL) {
                    vTaskSuspend(self_drive_task_handle);
                }
            } else if (strncmp(rx_buffer, "q", len) == 0) {
                ESP_LOGI(TAG_UDP, "Exiting WASD mode");
                wasd_mode = false;
                stop();
                if (self_drive_task_handle != NULL) {
                    vTaskResume(self_drive_task_handle);
                }
            }

            // Check if the message is to turn on the LED
            if (wasd_mode) {
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
                    ESP_LOGI(TAG_UDP, "Received S");
                    reverse();
                    // Send acknowledgment back to the client
                    const char *ack_msg = "Received S";
                    sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                } else if (strncmp(rx_buffer, "d", len) == 0) {
                    ESP_LOGI(TAG_UDP, "Received D");
                    turn_right();
                    // Send acknowledgment back to the client
                    const char *ack_msg = "Received D";
                    sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                }
            }
             // Self-driving mode if not in WASD mode
            else {
                ESP_LOGI(TAG_UDP, "Self-driving mode active");
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



static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = OPTI_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {

#if defined(CONFIG_EXAMPLE_IPV4)
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(OPTI_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(OPTI_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
        struct sockaddr_in6 dest_addr = { 0 };
        inet6_aton(OPTI_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(OPTI_PORT);
        dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
        struct sockaddr_storage dest_addr = { 0 };
        ESP_ERROR_CHECK(get_addr_from_stdin(OPTI_PORT, SOCK_DGRAM, &ip_protocol, &addr_family, &dest_addr));
#endif

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", OPTI_ADDR, OPTI_PORT);

        while (1) {

            int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);

                //parse received data
                // RobotData data;
                const char *input = rx_buffer;

                if (parseRobotData(input, &data)) {
                    printf("ID: %d\n", data.id);
                    printf("X: %.2f\n", data.x);
                    printf("Z: %.2f\n", data.z);
                    printf("Theta: %.2f\n", data.theta);
                    printf("Status: %s\n", data.status);
                } else {
                    printf("Failed to parse robot data.\n");
    }

                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }

            vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}


void get_distance_task(void *pvParameters) {
    while (1) {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw ADC Reading: %ld\tVoltage: %ld mV\n", adc_reading, voltage);

        if (voltage > 1400) { // Near range
            dist = (2670.0 - voltage) / 20.0;
        } else { // Far range
            dist = (61980.0 / voltage) - 4.41;
        }
        printf("Distance: %.2f cm\n", dist);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
    } 

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

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
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    xTaskCreate(get_distance_task, "get_distance_task", 4096, NULL, 5, NULL);
    xTaskCreate(self_drive, "Self Drive Task", 4096, NULL, 5, &self_drive_task_handle);

}
