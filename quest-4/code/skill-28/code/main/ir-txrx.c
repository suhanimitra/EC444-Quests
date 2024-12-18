//Leyandra Burke, Suhani Mitra, Marghe Piana, Kyla Wilson November 11, 2024

#include <stdio.h>
#include <string.h>
#include <stdlib.h>             // Added in 2023..
#include <inttypes.h>           // Added in 2023
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"     // Added in 2023
#include "esp_log.h"
#include "esp_system.h"         // Added in 2023
#include "driver/rmt_tx.h"      // Modified in 2023
#include "soc/rmt_reg.h"        // Not needed?
#include "driver/uart.h"
//#include "driver/periph_ctrl.h"
#include "driver/gptimer.h"     // Added in 2023
//#include "clk_tree.h"           // Added in 2023
#include "driver/gpio.h"        // Added in 2023
#include "driver/mcpwm_prelude.h"// Added in 2023
#include "driver/ledc.h"        // Added in 2023
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <sys/param.h>
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


// MCPWM defintions -- 2023: modified
#define MCPWM_TIMER_RESOLUTION_HZ 10000000 // 10MHz, 1 tick = 0.1us
#define MCPWM_FREQ_HZ             38000    // 38KHz PWM -- 1/38kHz = 26.3us
#define MCPWM_FREQ_PERIOD         263      // 263 ticks = 263 * 0.1us = 26.3us
#define MCPWM_GPIO_NUM            25


// UART definitions -- 2023: no changes
#define UART_TX_GPIO_NUM 26 // A0
#define UART_RX_GPIO_NUM 34 // A2
#define BUF_SIZE (1024)
#define BUF_SIZE2 (32)

// Hardware interrupt definitions -- 2023: no changes
#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1

// LED Output pins definitions -- 2023: minor changes
#define BLUEPIN   14
#define GREENPIN  32
#define REDPIN    15
#define ONBOARD   13
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<BLUEPIN) | (1ULL<<GREENPIN) | (1ULL<<REDPIN) | (1ULL<<ONBOARD) )

//define rasberry py IP
#define RASPBERRY_IP "192.168.1.143"

/*     CHANGE THESE VALUES BASED ON YOUR ESP          */

static const int hardcoded_ID = 1; // CHANGE ID WHEN FLASHING
#define MY_IP "192.168.1.114"

/*     END OF CHANGE          */




#define COLOR 'r'
#define HTTP_PORT 8080 //added to  make it work with rasberry pi
#define UDP_PORT 3000 //added to  make it work with rasberry pi
#define PORT1 3001
#define PORT2 3002
#define PORT3 3003
#define PORT4 3004

#define ELECTION_TIMEOUT 10000 // in milliseconds
#define COORDINATOR_TIMEOUT 10000 // in milliseconds

//typedef enum {ELECTION, ANSWER, VICTOR, KEEP_ALIVE} msg_type_t;
typedef enum {LEADER, ELECTION_RUNNING, NOT_LEADER, IDLE} state_t;

int leader_id = hardcoded_ID; // Start off by storing YOUR DEVICE ID
state_t state = ELECTION_RUNNING; // Starting in election state

TimerHandle_t coordinator_timer;
TimerHandle_t election_timer;


const char *TAG = "IR_TXRX";

//define hostIP
#define HOST_IP_ADDR "192.164.1.1"

//UDP
static int udp_sock1;
static int udp_sock2;
static int udp_sock3;
static int udp_sock4;
char send_buffer[128];
char rx_buffer[128];
struct sockaddr_in dest_addr1;
struct sockaddr_in dest_addr2;
struct sockaddr_in dest_addr3;
struct sockaddr_in dest_addr4;
struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
socklen_t socklen = sizeof(source_addr);

SemaphoreHandle_t data_mutex;


const char *MEMBERS[] = { // Using an array of IP addresses for simplicity
    "192.168.1.126",
    "192.168.1.114",
    "192.168.1.147", //delete your IP
    "192.168.1.132"
};


void reset_coordinator_timeout() {
    if (xTimerReset(coordinator_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to reset coordinator timeout timer");
    } else {
        ESP_LOGI(TAG, "Coordinator timeout timer reset");
    }
}

void reset_election_timeout() {
    if (xTimerReset(election_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to reset election timeout timer");
    } else {
        ESP_LOGI(TAG, "Election timeout timer reset");
    }
}


void send_message(char type, int id) {
    char send_buffer[32];
    snprintf(send_buffer, sizeof(send_buffer), "%c %d\n", type, id);
    // Send to all members
    for (int i = 0; i < sizeof(MEMBERS) / sizeof(MEMBERS[0]); i++) {
        //skip sending to self
        if (i == hardcoded_ID) {
            continue;
        }
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(MEMBERS[i]);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT1 + i);

        int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (udp_sock >= 0) {
            sendto(udp_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            ESP_LOGI(TAG, "Sent message: %s to %s", send_buffer, MEMBERS[i]);
            close(udp_sock);
        }
    }
}

//sends message to rasberry pi
void send_message_to_rasberrypi(char led_color, int id) {
  char send_buffer[32];
  snprintf(send_buffer, sizeof(send_buffer), "%c %d\n", led_color, id);
  
  struct sockaddr_in dest_addr; //changed it to the rasberry pi IP
  dest_addr.sin_addr.s_addr = inet_addr(RASPBERRY_IP);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(UDP_PORT);

  int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  //changed to send only from coordinator
  if (udp_sock >= 0 && hardcoded_ID == leader_id) {
    sendto(udp_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    ESP_LOGI(TAG, "Sent message: %s to Raspberry Pi at %s", send_buffer, RASPBERRY_IP);
    close(udp_sock);
  } else {
    ESP_LOGE(TAG, "Failed to create socket for Raspberry Pi");
  }
}

void send_message_single(char type, int id, int recv_id) {
    char send_buffer[32];
    snprintf(send_buffer, sizeof(send_buffer), "%c %d\n", type, id);
    // Send to only the receiving ID
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(MEMBERS[recv_id]);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT1 + recv_id);

    int udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock >= 0) {
        sendto(udp_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        ESP_LOGI(TAG, "Sent message: %s to %s", send_buffer, MEMBERS[recv_id]);
        close(udp_sock);
    }
}

void coordinator_timeout_handler() {
    if (state != LEADER) {
        ESP_LOGE(TAG, "Coordinator timeout! Rerunning election");
        leader_id = hardcoded_ID;
        state = ELECTION_RUNNING;
        reset_election_timeout();
    }
}



void election_timeout_handler() {
    if (leader_id == hardcoded_ID) {
        state = LEADER;
        printf("Leader ID: %d\n", leader_id);
        send_message('v', hardcoded_ID); // Announce victory
    } else {
        state = NOT_LEADER;
    }
}



void init_coordinator_timeout() {
    coordinator_timer = xTimerCreate(
        "CoordinatorTimeout",
        pdMS_TO_TICKS(COORDINATOR_TIMEOUT),
        pdFALSE,                  // One-shot timer, will only run once unless reset
        (void *)0,
        coordinator_timeout_handler // Function to call on timeout
    );

    if (coordinator_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create coordinator timeout timer");
    } else {
        ESP_LOGI(TAG, "Coordinator timeout timer created");
    }
}

void init_election_timeout() {
    election_timer = xTimerCreate(
        "ElectionTimeout",
        pdMS_TO_TICKS(ELECTION_TIMEOUT),
        pdFALSE,                  // One-shot timer, will only run once unless reset
        (void *)0,
        election_timeout_handler // Function to call on timeout
    );

    if (election_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create election timeout timer");
    } else {
        ESP_LOGI(TAG, "Election timeout timer created");
    }
}


void process_received_message(const char *message) {
    char type;
    int id;
    sscanf(message, "%c %d", &type, &id);

    switch (type) {
        case 'e': // Election message
            state = ELECTION_RUNNING;
            if (id > hardcoded_ID) {
              //send answer to any IDs larger than yours (smallest wins)
                send_message_single('a', hardcoded_ID, id);
            } else if (id < hardcoded_ID) {
                if (id < leader_id) {
                  printf("Leader ID: %d\n", leader_id);
                  leader_id = id;
                }
            }
            break;
        case 'a': // Answer message
            if (id < hardcoded_ID) {
              if (id < leader_id) {
                printf("Leader ID: %d\n", leader_id);
                leader_id = id;
              }
            }
            break;
        case 'v': // Victor message
            leader_id = id;
            state = (id == hardcoded_ID) ? LEADER : NOT_LEADER;
            break;
        case 'k': // Keep-alive message
            if (id == leader_id) {
                reset_coordinator_timeout();
            }
          break;
        case 'r':
        case 'g':
        case 'b':
          if (hardcoded_ID == leader_id) {
            //send directly to RPi
            send_message_to_rasberrypi(type, id);
            //send acknowledgement to the voter that we received their vote
            send_message_single('c', hardcoded_ID, id);
          } else {
            //send to the leader
            send_message_single(type, id, leader_id);
          }
          break;
        case 'c':
          //acknowledge received
          printf("acknowledgement received");
          break;
        default:
            ESP_LOGE(TAG, "Unknown message type");
            break;
    }
}

#define NUM_ESPS 4  // Adjust based on the number of ESPs
#define BASE_PORT 3001  // Starting port number for each ESP

int udp_socks[NUM_ESPS];  // Array to store sockets for each ESP

void receive_message_task(void *param) {
    int sock = *(int *)param;
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    char rx_buffer[128];  // Adjust size as needed

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            rx_buffer[len] = 0;  // Null-terminate received message
            ESP_LOGI(TAG, "Received message from %s: %s", inet_ntoa(source_addr.sin_addr), rx_buffer);
            process_received_message(rx_buffer);

            // // Send color to Raspberry Pi if this device is the leader
            // if (leader_id == hardcoded_ID) {
            //     send_message_to_rasberrypi(COLOR, leader_id); // Send color to Raspberry Pi
            // }
        } else {
            ESP_LOGW(TAG, "Failed to receive message or timeout occurred on socket %d", sock);
        }

        // Add a small delay to avoid busy-waiting
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void receive_message() {
    for (int i = 0; i < NUM_ESPS; i++) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT1 + i); // Adjust port as needed
        addr.sin_addr.s_addr = inet_addr(MEMBERS[i]);

        // Create socket for each IP address
        udp_socks[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (udp_socks[i] < 0) {
            ESP_LOGE(TAG, "Failed to create socket for %s", MEMBERS[i]);
            continue;
        }

        // Bind socket to the specific IP address
        if (bind(udp_socks[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "Socket binding failed for IP %s", MEMBERS[i]);
            close(udp_socks[i]);
            udp_socks[i] = -1;
        } else {
            ESP_LOGI(TAG, "Socket created and bound to IP %s", MEMBERS[i]);
        }

        // Create a separate task to listen on each socket
        xTaskCreate(receive_message_task, "receive_message_task", 4096, &udp_socks[i], 5, NULL);
    }
}




void state_machine() {
    init_election_timeout();
    init_coordinator_timeout();

    while (1) {
        switch (state) {
            case LEADER:
                // Stop the coordinator timeout timer since this device is now the coordinator
                if (xTimerStop(coordinator_timer, 0) != pdPASS) {
                    ESP_LOGE(TAG, "Failed to stop coordinator timeout timer in LEADER state");
                }
                send_message('k', hardcoded_ID); // Send keep-alive periodically
                vTaskDelay((COORDINATOR_TIMEOUT/2) / portTICK_PERIOD_MS);
                break;

            case ELECTION_RUNNING:
                send_message('e', hardcoded_ID); // Send election message
                if (xTimerIsTimerActive(election_timer) == pdFALSE) {
                    if (xTimerStart(election_timer, 0) != pdPASS) {
                        ESP_LOGE(TAG, "Failed to start election timeout timer in ELECTION_RUNNING state");
                    } else {
                        ESP_LOGE(TAG, "Started election timeout timer in ELECTION_RUNNING state");
                    }
                }
                vTaskDelay(ELECTION_TIMEOUT / portTICK_PERIOD_MS);
                break;

            case NOT_LEADER:
                // Start the coordinator timeout timer if it’s not already running
                if (xTimerIsTimerActive(coordinator_timer) == pdFALSE) {
                    if (xTimerStart(coordinator_timer, 0) != pdPASS) {
                        ESP_LOGE(TAG, "Failed to start coordinator timeout timer in NOT_LEADER state");
                    } else {
                        ESP_LOGE(TAG, "Started coordinator timeout timer in NOT_LEADER state");
                    }
                }
                vTaskDelay(COORDINATOR_TIMEOUT / portTICK_PERIOD_MS);
                break;

            case IDLE:
                // Start the coordinator timeout timer if it’s not already running
                if (xTimerIsTimerActive(coordinator_timer) == pdFALSE) {
                    if (xTimerStart(coordinator_timer, 0) != pdPASS) {
                        ESP_LOGE(TAG, "Failed to start coordinator timeout timer in IDLE state");
                    }
                }
                vTaskDelay(COORDINATOR_TIMEOUT / portTICK_PERIOD_MS);
                break;

            default:
                state = ELECTION_RUNNING; // Default to election if state undefined
                break;
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



// Variables for my ID, minVal and status plus string fragments
char start = 0x1B;              // START BYTE that UART looks for
char myID = (char) hardcoded_ID;
char myColor = (char) COLOR;
int len_out = 4;
volatile bool sendingEnabled = false;

// Mutex (for resources), and Queues (for button)
SemaphoreHandle_t mux = NULL; // 2023: no changes
static QueueHandle_t gpio_evt_queue = NULL; // 2023: Changed
// static xQueueHandle_t timer_queue; -- 2023: removed

// A simple structure to pass "events" to main task -- 2023: modified
typedef struct {
    uint64_t event_count;
} example_queue_element_t;

// Create a FIFO queue for timer-based events -- Modified
example_queue_element_t ele;
QueueHandle_t timer_queue;

// System tags for diagnostics -- 2023: modified
//static const char *TAG_SYSTEM = "ec444: system";       // For debug logs
static const char *TAG_TIMER = "ec444: timer";         // For timer logs
static const char *TAG_UART = "ec444: uart";           // For UART logs

// Button interrupt handler -- add to queue -- 2023: no changes
static void IRAM_ATTR gpio_isr_handler(void* arg){
  uint32_t gpio_num = (uint32_t) arg;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// Timmer interrupt handler -- Callback timer function -- 2023: modified
// Note we enabled time for auto-reload
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data) {
    BaseType_t high_task_awoken = pdFALSE;
    QueueHandle_t timer_queue1 = (QueueHandle_t)user_data;
    // Retrieve count value and send to queue
    example_queue_element_t ele = {
        .event_count = edata->count_value
    };
    xQueueSendFromISR(timer_queue1, &ele, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

// Utilities ///////////////////////////////////////////////////////////////////

// Checksum -- 2023: no changes
char genCheckSum(char *p, int len) {
  char temp = 0;
  for (int i = 0; i < len; i++){
    temp = temp^p[i];
  }
  // printf("%X\n",temp);  // Diagnostic

  return temp;
}
bool checkCheckSum(uint8_t *p, int len) {
  char temp = (char) 0;
  bool isValid;
  for (int i = 0; i < len-1; i++){
    temp = temp^p[i];
  }
  // printf("Check: %02X ", temp); // Diagnostic
  if (temp == p[len-1]) {
    isValid = true; }
  else {
    isValid = false; }
  return isValid;
}


// MCPWM Initialize -- 2023: this is to create 38kHz carrier
static void pwm_init() {

  // Create timer
  mcpwm_timer_handle_t pwm_timer = NULL;
  mcpwm_timer_config_t pwm_timer_config = {
      .group_id = 0,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = MCPWM_TIMER_RESOLUTION_HZ,
      .period_ticks = MCPWM_FREQ_PERIOD,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
  };
  ESP_ERROR_CHECK(mcpwm_new_timer(&pwm_timer_config, &pwm_timer));

  // Create operator
  mcpwm_oper_handle_t oper = NULL;
  mcpwm_operator_config_t operator_config = {
      .group_id = 0, // operator must be in the same group to the timer
  };
  ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

  // Connect timer and operator
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, pwm_timer));

  // Create comparator from the operator
  mcpwm_cmpr_handle_t comparator = NULL;
  mcpwm_comparator_config_t comparator_config = {
      .flags.update_cmp_on_tez = true,
  };
  ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

  // Create generator from the operator
  mcpwm_gen_handle_t generator = NULL;
  mcpwm_generator_config_t generator_config = {
      .gen_gpio_num = MCPWM_GPIO_NUM,
  };
  ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

  // set the initial compare value, so that the duty cycle is 50%
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator,132));
  // CANNOT FIGURE OUT HOW MANY TICKS TO COMPARE TO TO GET 50%

  // Set generator action on timer and compare event
  // go high on counter empty
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                  MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
  // go low on compare threshold
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                  MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

  // Enable and start timer
  ESP_ERROR_CHECK(mcpwm_timer_enable(pwm_timer));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(pwm_timer, MCPWM_TIMER_START_NO_STOP));

}


// Configure UART -- 2023: minor changes
static void uart_init() {
  // Basic configs
  const uart_config_t uart_config = {
      .baud_rate = 1200, // Slow BAUD rate
      .data_bits = UART_DATA_8_BITS,
      .parity    = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT
  };
  uart_param_config(UART_NUM_1, &uart_config);

  // Set UART pins using UART0 default pins
  uart_set_pin(UART_NUM_1, UART_TX_GPIO_NUM, UART_RX_GPIO_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  // Reverse receive logic line
  uart_set_line_inverse(UART_NUM_1,UART_SIGNAL_RXD_INV);

  // Install UART driver
  uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
}

// GPIO init for LEDs -- 2023: modified
static void led_init() {
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}

// Configure timer -- 2023: Modified
static void alarm_init() {

    gptimer_handle_t gptimer = NULL;
    gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000, // 1MHz, 1 tick=1us
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    // Set alarm callback
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, timer_queue));

    // Enable timer
    ESP_ERROR_CHECK(gptimer_enable(gptimer));

    ESP_LOGI(TAG_TIMER, "Start timer, update alarm value dynamically and auto reload");
    gptimer_alarm_config_t alarm_config = {
      .reload_count = 0, // counter will reload with 0 on alarm event
      .alarm_count = 10*1000000, // period = 10*1s = 10s
      .flags.auto_reload_on_alarm = true, // enable auto-reload
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

}

// Button interrupt init -- 2023: minor changes
static void button_init() {
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_intr_enable(GPIO_INPUT_IO_1 ); // 2023: retained

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
}


// Tasks
// Button task -- rotate through myIDs -- 2023: no changes
void button_task(){
  uint32_t io_num;
  while(1) {
    if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      xSemaphoreTake(mux, portMAX_DELAY);
      sendingEnabled = !sendingEnabled;
      // if (myID == 3) {
      //   myID = 1;
      // }
      // else {
      //   myID++;
      // }
      xSemaphoreGive(mux);
      printf("Button pressed.\n");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

}

// Send task -- sends payload | Start | myID | Start | myID -- 2023: no changes
void send_task(){
  while(1) {
    char *data_out = (char *) malloc(len_out);
    printf("sending enabled? %d\n", sendingEnabled);
    if (sendingEnabled){
      xSemaphoreTake(mux, portMAX_DELAY);
      data_out[0] = start;
      data_out[1] = (char) myColor;
      data_out[2] = (char) myID;
      data_out[3] = genCheckSum(data_out,len_out-1);
      // ESP_LOG_BUFFER_HEXDUMP(TAG_SYSTEM, data_out, len_out, ESP_LOG_INFO);

      uart_write_bytes(UART_NUM_1, data_out, len_out);
      xSemaphoreGive(mux);
    }
    free(data_out);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Receive task -- looks for Start byte then stores received values -- 2023: minor changes
void recv_task(){
  char recv_color;
  char recv_id;
  // Buffer for input data
  // Receiver expects message to be sent multiple times
  uint8_t *data_in = (uint8_t *) malloc(BUF_SIZE2);
  while (1) {
    int len_in = uart_read_bytes(UART_NUM_1, data_in, BUF_SIZE2, 100 / portTICK_PERIOD_MS);
    ESP_LOGE(TAG_UART, "Length: %d", len_in);
    if (len_in > 10) {
      int nn = 0;
      //ESP_LOGE(TAG_UART, "Length greater than 10");
      while (data_in[nn] != start) {
        nn++;
      }
      uint8_t copied[len_out];
      memcpy(copied, data_in + nn, len_out * sizeof(uint8_t));
      //printf("before checksum");
      //ESP_LOG_BUFFER_HEXDUMP(TAG_UART, copied, len_out, ESP_LOG_INFO);
      if (checkCheckSum(copied,len_out)) {
        //change the color to the one we received
        recv_color = copied[1];
        recv_id = copied[2];
        if (hardcoded_ID == leader_id) {
          //send directly to RPi
          send_message_to_rasberrypi(recv_color, recv_id);
          //send acknowledgement to the voter thru UDP
          send_message_single('c', hardcoded_ID, recv_id);
        } else {
          //send to leader
          send_message_single(recv_color, recv_id, leader_id);
        }
        printf("after checksum");
        ESP_LOG_BUFFER_HEXDUMP(TAG_UART, copied, len_out, ESP_LOG_INFO);
        uart_flush(UART_NUM_1);
      }
    }
    else{
      // printf("Nothing received.\n");
    }
    //vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  free(data_in);
}

//hello

// LED task to light LED based on traffic state -- 2023: no changes
void led_task(){
  while(1) {
    switch(state){
      case NOT_LEADER : // Red
        gpio_set_level(GREENPIN, 0);
        gpio_set_level(REDPIN, 1);
        gpio_set_level(BLUEPIN, 0);
        myColor = 'r';
        // printf("Current state: %c\n",status);
        break;
      case ELECTION_RUNNING : // Yellow
        gpio_set_level(GREENPIN, 0);
        gpio_set_level(REDPIN, 0);
        gpio_set_level(BLUEPIN, 1);
        myColor = 'b';
        // printf("Current state: %c\n",status);
        break;
      case LEADER : // Green
        gpio_set_level(GREENPIN, 1);
        gpio_set_level(REDPIN, 0);
        gpio_set_level(BLUEPIN, 0);
        myColor = 'g';
        // printf("Current state: %c\n",status);
        break;
      default: //set all to high
        gpio_set_level(GREENPIN, 1);
        gpio_set_level(REDPIN, 1);
        gpio_set_level(BLUEPIN, 1);
        myColor = 'x';
        break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// LED task to blink onboard LED based on ID -- 2023: no changes
// void id_task(){
//   while(1) {
//     for (int i = 0; i < (int) myID; i++) {
//       gpio_set_level(ONBOARD,1);
//       vTaskDelay(200 / portTICK_PERIOD_MS);
//       gpio_set_level(ONBOARD,0);
//       vTaskDelay(200 / portTICK_PERIOD_MS);
//     }
//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//   }
// }

// Timer task -- R (10 seconds), G (10 seconds), Y (10 seconds) -- 2023: modified
// static void timer_evt_task(void *arg) {
//   while (1) {
//     // Transfer from queue and do something if triggered
//     if (xQueueReceive(timer_queue, &ele, pdMS_TO_TICKS(2000))) {
//       printf("Action!\n");
//       if (myColor == 'R') {
//         myColor = 'G';
//       }
//       else if (myColor == 'G') {
//         myColor = 'Y';
//       }
//       else if (myColor == 'Y') {
//         myColor = 'R';
//       }
//     }
//   }
// }

void app_main() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    
    // Mutex for current values when sending -- no changes
    mux = xSemaphoreCreateMutex();

    // Timer queue initialize -- 2023: modified
    timer_queue = xQueueCreate(10, sizeof(example_queue_element_t));
    if (!timer_queue) {
        ESP_LOGE(TAG_TIMER, "Creating queue failed");
        return;
    }

    // Create task to handle timer-based events -- no changes
    //xTaskCreate(timer_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);

    // Initialize all the things -- no changes
    // rmt_tx_init();
    uart_init();
    led_init();
    alarm_init();
    button_init();
    pwm_init();
    receive_message();
    // Create tasks for receive, send, set gpio, and button -- 2023 -- no changes

    // *****Create one receiver and one transmitter (but not both)
    xTaskCreate(recv_task, "uart_rx_task", 1024*4, NULL, configMAX_PRIORITIES-1, NULL);
    // *****Create one receiver and one transmitter (but not both)
    xTaskCreate(send_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(state_machine, "state_machine_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(led_task, "set_traffic_task", 1024*2, NULL, 1, NULL);
    //xTaskCreate(id_task, "set_id_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
    xTaskCreate(button_task, "button_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
    while(1){
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
