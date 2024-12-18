//Leyandra Burke, Suhani Mitra, Margherita Piana, Kyla Wilson  10/25/2024

/*
  Adapted I2C example code to work with the Adafruit ADXL343 accelerometer. Ported and referenced a lot of code from the Adafruit_ADXL343 driver code.
  ----> https://www.adafruit.com/product/4097

  Emily Lam, Aug 2019 for BU EC444
*/
#include <stdio.h>
#include <math.h>
#include "driver/i2c.h"
#include "./ADXL343.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <string.h>
#include <time.h>

//Wifi Code Things - Suhani

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

static const char *TAG = "example";
static int udp_sock;
static const char *payload = "Message from ESP32 ";
static uint8_t s_led_state = 0;


#define HOST_IP_ADDR "192.168.1.143" // IP address of the Pi
#define PORT 3000

// Master I2C
#define I2C_EXAMPLE_MASTER_SCL_IO          22   // gpio number for i2c clk
#define I2C_EXAMPLE_MASTER_SDA_IO          23   // gpio number for i2c data
#define I2C_EXAMPLE_MASTER_NUM             I2C_NUM_0  // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ         100000     // i2c master clock freq
#define WRITE_BIT                          I2C_MASTER_WRITE // i2c master write
#define READ_BIT                           I2C_MASTER_READ  // i2c master read
#define ACK_CHECK_EN                       true // i2c master will check ack
#define ACK_CHECK_DIS                      false// i2c master will not check ack
#define ACK_VAL                            0x00 // i2c ack value
#define NACK_VAL                           0x01 // i2c nack value (Was FF)


#define ALPHA_ADDR                         0x70 // alphanumeric address
#define OSC                                0x21 // oscillator cmd
#define HT16K33_BLINK_DISPLAYON            0x01 // Display on cmd
#define HT16K33_BLINK_OFF                  0    // Blink off cmd
#define HT16K33_BLINK_CMD                  0x80 // Blink cmd
#define HT16K33_CMD_BRIGHTNESS             0xE0 // Brightness cmd

// Master I2C
#define I2C_EXAMPLE_MASTER_SCL_IO          22   // gpio number for i2c clk
#define I2C_EXAMPLE_MASTER_SDA_IO          23   // gpio number for i2c data
#define I2C_EXAMPLE_MASTER_NUM             I2C_NUM_0  // i2c port
#define I2C_EXAMPLE_MASTER_TX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_RX_BUF_DISABLE  0    // i2c master no buffer needed
#define I2C_EXAMPLE_MASTER_FREQ_HZ         100000     // i2c master clock freq
#define WRITE_BIT                          I2C_MASTER_WRITE // i2c master write
#define READ_BIT                           I2C_MASTER_READ  // i2c master read
#define ACK_CHECK_EN                       true // i2c master will check ack
#define ACK_CHECK_DIS                      false// i2c master will not check ack
#define ACK_VAL                            0x00 // i2c ack value
#define NACK_VAL                           0xFF // i2c nack value
// ADXL343
#define SLAVE_ADDR                         ADXL343_ADDRESS // 0x53

#define NO_OF_SAMPLES   64
#define DEFAULT_VREF    1100
#define SERIESRESISTOR 1000
#define THERMISTORNOMINAL 10000
#define BCOEFFICIENT 3435
#define TEMPERATURENOMINAL 25


//ADC
static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel2 = ADC_CHANNEL_9;
static const adc_atten_t atten = ADC_ATTEN_DB_11;        //0dB attenuation fix for ADC scale
static const adc_unit_t unit = ADC_UNIT_1;


// Hardware interrupt definitions

#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1
#define GPIO_INPUT_IO_1       21
#define BUTTON  21
#define SUBUZZ  15
#define MABUZZ  33
#define KYBUZZ  27
#define LEBUZZ  12

//can remove since we don't use flag var
int flag = 0;     // Global flag for signaling from ISR
int buttonState = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

char send_buffer[128];
char rx_buffer[128];
struct sockaddr_in dest_addr;
struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
socklen_t socklen = sizeof(source_addr);

char previousLeadState[128]= "Lead Incom...";  

//mutex
SemaphoreHandle_t data_mutex;


// Define the sevensegfonttable
static const uint8_t sevensegfonttable[] = {
    0b00000000, // (space)
    0b10000110, // !
    0b00100010, // "
    0b01111110, // #
    0b01101101, // $
    0b11010010, // %
    0b01000110, // &
    0b00100000, // '
    0b00101001, // (
    0b00001011, // )
    0b00100001, // *
    0b01110000, // +
    0b00010000, // ,
    0b01000000, // -
    0b10000000, // .
    0b01010010, // /
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111, // 9
    0b00001001, // :
    0b00001101, // ;
    0b01100001, // <
    0b01001000, // =
    0b01000011, // >
    0b11010011, // ?
    0b01011111, // @
    0b01110111, // A
    0b01111100, // B
    0b00111001, // C
    0b01011110, // D
    0b01111001, // E
    0b01110001, // F
    0b00111101, // G
    0b01110110, // H
    0b00110000, // I
    0b00011110, // J
    0b01110101, // K
    0b00111000, // L
    0b00010101, // M
    0b00110111, // N
    0b00111111, // O
    0b01110011, // P
    0b01101011, // Q
    0b00110011, // R
    0b01101101, // S
    0b01111000, // T
    0b00111110, // U
    0b00111110, // V
    0b00101010, // W
    0b01110110, // X
    0b01101110, // Y
    0b01011011, // Z
    0b00111001, // [
    0b01100100, //
    0b00001111, // ]
    0b00100011, // ^
    0b00001000, // _
    0b00000010, // `
    0b01011111, // a
    0b01111100, // b
    0b01011000, // c
    0b01011110, // d
    0b01111011, // e
    0b01110001, // f
    0b01101111, // g
    0b01110100, // h
    0b00010000, // i
    0b00001100, // j
    0b01110101, // k
    0b00110000, // l
    0b00010100, // m
    0b01010100, // n
    0b01011100, // o
    0b01110011, // p
    0b01100111, // q
    0b01010000, // r
    0b01101101, // s
    0b01111000, // t
    0b00011100, // u
    0b00011100, // v
    0b00010100, // w
    0b01110110, // x
    0b01101110, // y
    0b01011011, // z
    0b01000110, // {
    0b00110000, // |
    0b01110000, // }
    0b00000001, // ~
    0b00000000, // del
};

// Used ChatGPT for assistance with this function
// Function to encode a character to the corresponding 14-segment display value
uint16_t encode_char_to_segment(char c) {
    // Handle special cases for characters out of bounds
    if (c >= ' ' && c <= '~') {  // Characters within printable ASCII range
        return sevensegfonttable[c - ' '];
    }
    return 0b00000000;  // Return blank for unsupported characters
}


// Function to initiate i2c -- note the MSB declaration!
static void i2c_master_init(){
  // Debug
  //printf("\n>> i2c Config\n");
  int err;

  // Port configuration
  int i2c_master_port = I2C_EXAMPLE_MASTER_NUM;

  /// Define I2C configurations
  i2c_config_t conf;
  conf.mode = I2C_MODE_MASTER;                              // Master mode
  conf.sda_io_num = I2C_EXAMPLE_MASTER_SDA_IO;              // Default SDA pin          //change to Ž3
  conf.sda_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
  conf.scl_io_num = I2C_EXAMPLE_MASTER_SCL_IO;              // Default SCL pin          //change to 22
  conf.scl_pullup_en = GPIO_PULLUP_ENABLE;                  // Internal pullup
  conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;       // CLK frequency

    // conf.master.clk_speed = I2C_EXAMPLE_MASTER_FREQ_HZ;       // CLK frequency
    conf.clk_flags = 0;                                       // <-- ADD THIS LINE
    // err = i2c_param_config(i2c_master_port, &conf);           // Configure


  err = i2c_param_config(i2c_master_port, &conf);           // Configure
 // if (err == ESP_OK) {printf("- parameters: ok\n");}

  // Install I2C driver
  err = i2c_driver_install(i2c_master_port, conf.mode,
                     I2C_EXAMPLE_MASTER_RX_BUF_DISABLE,
                     I2C_EXAMPLE_MASTER_TX_BUF_DISABLE, 0);
  //if (err == ESP_OK) {printf("- initialized: yes\n");}

  // Data in MSB mode
  i2c_set_data_mode(i2c_master_port, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
}

// Utility  Functions //////////////////////////////////////////////////////////

// Utility function to test for I2C device address -- not used in deploy
int testConnection(uint8_t devAddr, int32_t timeout) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  int err = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return err;
}


// Utility function to scan for i2c device
static void i2c_scanner() {
  int32_t scanTimeout = 1000;
 // printf("\n>> I2C scanning ..."  "\n");
  uint8_t count = 0;
  for (uint8_t i = 1; i < 127; i++) {
    // printf("0x%X%s",i,"\n");
    if (testConnection(i, scanTimeout) == ESP_OK) {
     // printf( "- Device found at address: 0x%X%s", i, "\n");
      count++;
    }
  }
 // if (count == 0) {printf("- No I2C devices found!" "\n");}
}

// Turn on oscillator for alpha display
int alpha_oscillator() {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( ALPHA_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, OSC, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

// Set blink rate to off
int no_blink() {
  int ret;
  i2c_cmd_handle_t cmd2 = i2c_cmd_link_create();
  i2c_master_start(cmd2);
  i2c_master_write_byte(cmd2, ( ALPHA_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd2, HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (HT16K33_BLINK_OFF << 1), ACK_CHECK_EN);
  i2c_master_stop(cmd2);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd2, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd2);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

// Set Brightness
int set_brightness_max(uint8_t val) {
  int ret;
  i2c_cmd_handle_t cmd3 = i2c_cmd_link_create();
  i2c_master_start(cmd3);
  i2c_master_write_byte(cmd3, ( ALPHA_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd3, HT16K33_CMD_BRIGHTNESS | val, ACK_CHECK_EN);
  i2c_master_stop(cmd3);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd3, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd3);
  vTaskDelay(200 / portTICK_PERIOD_MS);
  return ret;
}

////////////////////////////////////////////////////////////////////////////////

// ADXL343 Functions ///////////////////////////////////////////////////////////

// Get Device ID
int getDeviceID(uint8_t *data) {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, ADXL343_REG_DEVID, ACK_CHECK_EN);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, data, ACK_CHECK_DIS);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}

//used chatGPT for the following functions

// Write one byte to register
int writeRegister(uint8_t reg, uint8_t data) {
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}

// Read register
uint8_t readRegister(uint8_t reg) {
  uint8_t data = 0;
  int ret;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, ( SLAVE_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, &data, ACK_CHECK_DIS);
  i2c_master_stop(cmd);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd);
  return data;
}

// read 16 bits (2 bytes)
// read 16 bits (2 bytes)
int16_t read16(uint8_t reg) {
  uint8_t dataL = readRegister(reg);
  uint8_t dataH = readRegister(reg + 1);
  return (int16_t)((dataH << 8) | dataL);
}


void setRange(range_t range) {
  /* Red the data format register to preserve bits */
  uint8_t format = readRegister(ADXL343_REG_DATA_FORMAT);

  /* Update the data rate */
  format &= ~0x0F;
  format |= range;

  /* Make sure that the FULL-RES bit is enabled for range scaling */
  format |= 0x08;

  /* Write the register back to the IC */
  writeRegister(ADXL343_REG_DATA_FORMAT, format);

}

range_t getRange(void) {
  /* Red the data format register to preserve bits */
  return (range_t)(readRegister(ADXL343_REG_DATA_FORMAT) & 0x03);
}

dataRate_t getDataRate(void) {
  return (dataRate_t)(readRegister(ADXL343_REG_BW_RATE) & 0x0F);
}

////////////////////////////////////////////////////////////////////////////////

// function to get acceleration
void getAccel(float * xp, float *yp, float *zp) {
  *xp = ((float)read16(ADXL343_REG_DATAX0)) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  *yp = ((float)read16(ADXL343_REG_DATAY0)) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
  *zp = ((float)read16(ADXL343_REG_DATAZ0)) * ADXL343_MG2G_MULTIPLIER * SENSORS_GRAVITY_STANDARD;
 //printf("X: %.2f \t Y: %.2f \t Z: %.2f\n", *xp, *yp, *zp);
}

//used chatGPT for the following function
// function to print roll and pitch
void calcRP(float x, float y, float z, float *rollp, float *pitchp) {
  *rollp = atan2(y, z) * 57.3;
  *pitchp = atan2((-x), sqrt(y * y + z * z)) * 57.3;
  printf("roll: %.2f \t pitch: %.2f \n", *rollp, *pitchp);
}


static void clear_display(){
  int ret;
  uint16_t displaybuffer[8];
  displaybuffer[0] = 0b0000000000000000;  // Empty third part of display
  displaybuffer[1] = 0b0000000000000000;  // Empty fourth part of display
  displaybuffer[2] = 0b0000000000000000;  // Empty third part of display
  displaybuffer[3] = 0b0000000000000000;  // Empty fourth part of display
  // Send the 4 characters to the display over I2C
  i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
  i2c_master_start(cmd4);
  i2c_master_write_byte(cmd4, ( ALPHA_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
  i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
  for (int i = 0; i < 4; i++) {
    i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
    i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
  }
  i2c_master_stop(cmd4);
  ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
  i2c_cmd_link_delete(cmd4);
}

//button things
static int counter = 0;
static bool button_on = false;
static char activity_string[5]= "actv ";
static char activity_Time[4] = "123S";
static char leading_string[15] = "Lead: Suhani";

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    counter = 0;     // Reset counter on button press
    button_on = !button_on; // Start counting when button is pressed
}

void incTime() {
  seconds++;
  if (seconds >= 60)
  {
    minutes++;
    seconds -= 60;
  }
  if (minutes >= 60) {
    hours++;
    minutes -= 60;
  }
}


//temperature things
float getTemp(){
        uint32_t adc_reading2 = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading2 += adc1_get_raw((adc1_channel_t)channel);
                int raw;
                adc2_get_raw((adc2_channel_t)channel2, ADC_WIDTH_BIT_12, &raw);
                adc_reading2 += raw;
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading2 += raw;
            }
        }
        adc_reading2 /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV

        uint32_t voltage2 = esp_adc_cal_raw_to_voltage(adc_reading2, adc_chars);    //thermistor

        //using code from the following source: https://www.e-tinkers.com/2019/10/using-a-thermistor-with-arduino-and-unexpected-esp32-adc-non-linearity/
        float Tc, T, Rt;
        float To = 298.15;
        float R1=10000.0; //Voltage divider resistor value
        float Ro = 10000.0;   // Resistance of Thermistor at 25 degree Celsius
     
        Rt=R1*(3300.0/voltage2-1.0);//Formula to convert voltage to resistance
        T = 1/(1/To + log(Rt/Ro)/BCOEFFICIENT);  // Temperature in Farheneds from resistance
        Tc = T - 273.15;// Conversion of temperature in Celsius
        return Tc;
}

static int classifyActivity(float x, float y, float z, float roll, float pitch, int *activTimeS){
  int state = 1; //0 inactive, 1 active, 2 highly active
  static int inacTimeS, acTimeS, hiacTimeS;
  static int lastState;

//   printf("roll: %f, pitch: %f\n", roll, pitch);
    if (roll > 10 || pitch > 10) {
      state = 2;
      if (lastState != state){
        hiacTimeS = 0;
      }
     // activity_string = "hact";
        activity_string[0]='h';
        activity_string[1]='a';
        activity_string[2]='c';
        activity_string[3]='t';
        incTime();
      hiacTimeS = hiacTimeS + 1;
      *activTimeS = hiacTimeS;
    }
    else if ((roll > -5 && roll < 2) && (pitch > -3 && pitch < 3)) {
      state = 0;  //asleep
      //activity_string = "aslp";
        activity_string[0]='a';
        activity_string[1]='s';
        activity_string[2]='l';
        activity_string[3]='p';
     
      if (lastState != state){
        inacTimeS = 0;
      }
      incTime();
      inacTimeS = inacTimeS + 1;
      *activTimeS = inacTimeS;
    } else {
      state = 1;  //active
       if (lastState != state){
          acTimeS = 0;
       }
      //activity_string = "actv";
        activity_string[0]='a';
        activity_string[1]='c';
        activity_string[2]='t';
        activity_string[3]='v';
      incTime();
      acTimeS = acTimeS + 1;
      *activTimeS = acTimeS;
    }
    char timeStr[4];  
    sprintf(timeStr, "%d", *activTimeS);  

    activity_Time[0]= timeStr[0];
    activity_Time[1]= timeStr[1];
    activity_Time[2]= timeStr[2];
    
  lastState = state;

  return state;
}

#include <string.h>

static void classifyLeading(const char* leadState) {
    printf("lead state: %s\n",leadState);
    printf("lead state: %s\n",previousLeadState);


    xSemaphoreTake(data_mutex, portMAX_DELAY);

    // Check if leadState has changed
    if (strcmp(leadState, previousLeadState) != 0) {
        // Buzz everyone to indicate a change
        printf("ciao\n");

        gpio_set_level(SUBUZZ, 1);
        gpio_set_level(MABUZZ, 1);
        gpio_set_level(KYBUZZ, 1);
        gpio_set_level(LEBUZZ, 1);
     
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Buzzing on");        //this is printing but the buzzers are not buzzing
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("ciao under buzzer\n");

        // Update leading string and set specific leader buzzer
        if (strcmp(leadState, "SUHANI") == 0) {
            strcpy(leading_string, "LEAD: SUHANI");
            // gpio_set_level(SUBUZZ, 1);
        } 
        else if (strcmp(leadState, "MARGHE") == 0) {
            strcpy(leading_string, "LEAD: MARGHE");
            // gpio_set_level(MABUZZ, 1);
        } 
        else if (strcmp(leadState, "KYLA") == 0) {
            strcpy(leading_string, "LEAD: KYLA");
            // gpio_set_level(KYBUZZ, 1);
        } 
        else if (strcmp(leadState, "LEYANDRA") == 0) {
            strcpy(leading_string, "LEAD: LEYANDRA");
            // gpio_set_level(LEBUZZ, 1);
        } 
        else {
            strcpy(leading_string, "LEAD INCOM...");
        }

        // Update previous lead state to the current one
        strcpy(previousLeadState, leadState);

        //debugging
        printf("previousLeadState: %s",previousLeadState);
    } else {
        gpio_set_level(SUBUZZ, 0);
        gpio_set_level(MABUZZ, 0);
        gpio_set_level(KYBUZZ, 0);
        gpio_set_level(LEBUZZ, 0);
    }

    xSemaphoreGive(data_mutex);
}


//display the name OTTO
static void display_info(int* activTimeS){
  if (button_on == 1) {
    classifyLeading(rx_buffer);
    
    int lead_len = strlen(leading_string);

  for (int offset = 0; offset <= lead_len - 4; offset++) {
            // Write the next 4 characters to the display
            uint16_t displaybuffer[4];
            for (int i = 0; i < 4; i++) {
                displaybuffer[i] = encode_char_to_segment(leading_string[offset + i]);
                // printf("%04x\n", displaybuffer[i]);  // Debugging: print the encoded value
            }
  

    int ret;
    int button_state = gpio_get_level(BUTTON);
    i2c_cmd_handle_t cmd4 = i2c_cmd_link_create();
    i2c_master_start(cmd4);
    i2c_master_write_byte(cmd4, ( ALPHA_ADDR << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd4, (uint8_t)0x00, ACK_CHECK_EN);
    for (int i = 0; i < 4; i++) {
      i2c_master_write_byte(cmd4, displaybuffer[i] & 0xFF, ACK_CHECK_EN);
      i2c_master_write_byte(cmd4, displaybuffer[i] >> 8, ACK_CHECK_EN);
    }
    i2c_master_stop(cmd4);
    ret = i2c_master_cmd_begin(I2C_EXAMPLE_MASTER_NUM, cmd4, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd4);
    float temp = getTemp();
    int state;
    float xVal, yVal, zVal, roll, pitch;
    getAccel(&xVal, &yVal, &zVal);
    calcRP(xVal, yVal, zVal, &roll, &pitch);
    state = classifyActivity(xVal, yVal, zVal, roll, pitch, activTimeS);
    //printf("Activity state: %d, Activity Time: %d, Button state %d\n",state, *activTimeS, button_on);
    snprintf(send_buffer, sizeof(send_buffer), "%d\n", state);
    xSemaphoreTake(data_mutex, portMAX_DELAY);
      if (udp_sock >= 0) {
          int err = sendto(udp_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
          if (err < 0) {
              ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
          } else {
              ESP_LOGI(TAG, "Data sent: %s", send_buffer);
          }
      }
      xSemaphoreGive(data_mutex);
    int len = recvfrom(udp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    if (len < 0) {
      ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
      break;
    } else {
      rx_buffer[len] = 0; // Null-terminate the received string
      ESP_LOGI(TAG, "Received %d bytes from server: %s", len, rx_buffer);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for scrolling effect (1 second)
  }
  }
}

//From the docs/design-patterns/docs/dp-interrupts.md file, using the first design implementation
// Intialize the GPIO to detect button press as interrupt
static void button_init() {
  gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE; // interrupt of rising edge
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL; // bit mask of the pins, use GPIO4 here
    io_conf.mode = GPIO_MODE_INPUT;            // set as input mode
    io_conf.pull_up_en = 1;                    // enable resistor pull-up mode on pin
  gpio_config(&io_conf);                       // apply parameters
  gpio_intr_enable(GPIO_INPUT_IO_1);          // enable interrupts on pin
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);   //install gpio isr service
  gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1); //hook isr handler for specific gpio pin
}

// Task to continuously poll acceleration and calculate roll and pitch
static void test_adxl343() {
  //printf("\n>> Polling ADAXL343\n");

  int ret;
    //printf(">> Test Alphanumeric Display: \n");

    // Set up routines
    // Turn on alpha oscillator
    ret = alpha_oscillator();
    //if(ret == ESP_OK) {printf("- oscillator: ok \n");}
    // Set display blink off
    ret = no_blink();
    //if(ret == ESP_OK) {printf("- blink: off \n");}
    ret = set_brightness_max(0xF);
    //if(ret == ESP_OK) {printf("- brightness: max \n");}

    int state; //0 inactive, 1 active, 2 highly active
    // char send_buffer[128];
    // struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);


  while (1) {
    float xVal, yVal, zVal, roll, pitch;
    getAccel(&xVal, &yVal, &zVal);
    calcRP(xVal, yVal, zVal, &roll, &pitch);
    float temp = getTemp();
    int activTimeS;
    int button_state = gpio_get_level(BUTTON);  
      
      state = classifyActivity(xVal, yVal, zVal, roll, pitch, &activTimeS);

      if (state == 0){
        //printf("State: Inactive\n");
      }
      else if (state == 1){
       // printf("State: Active\n");
      }
      else if (state == 2){
        //printf("State: Highly Active\n");
      }
        
    //printf("Activity state: %d, Activity Time: %d, Button state %d\n",state, activTimeS, button_on);
    printf("Activity Time: %d Seconds\n", activTimeS);

     // Send data over UDP
      snprintf(send_buffer, sizeof(send_buffer), "%d", state);
      if (udp_sock >= 0) {
          int err = sendto(udp_sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
          if (err < 0) {
              ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
          } else {
              ESP_LOGI(TAG, "Data sent: %s", send_buffer);
          }
      }

    int len = recvfrom(udp_sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
    if (len < 0) {
        ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
        break;
    } else {
        rx_buffer[len] = 0; // Null-terminate the received string
        ESP_LOGI(TAG, "Received %d bytes from server: %s", len, rx_buffer);
    }
    printf("Received: %s\n", rx_buffer);
    classifyLeading(rx_buffer);
      
    // Display name
    if (button_on){
      display_info(&activTimeS);
    }
    else if (!button_on){
      clear_display();
      gpio_set_level(SUBUZZ, 0);
      gpio_set_level(MABUZZ, 0);
      gpio_set_level(KYBUZZ, 0);
      gpio_set_level(LEBUZZ, 0);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
  }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        //printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        //printf("Characterized using eFuse Vref\n");
    } else {
        //printf("Characterized using Default Vref\n");
    }
}


//wifi things
static void udp_client_init(void) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);
    }

    // Set timeout
    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(udp_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

// Updated app_main with all initializations and tasks
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // Initialize UDP client
    udp_client_init();

    // Semaphore
    data_mutex = xSemaphoreCreateMutex();

    // Routine
    i2c_master_init();
    i2c_scanner();

    gpio_reset_pin(BUTTON);
    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    button_init(); // Initialize button config

    // Configure buzzers
    gpio_reset_pin(SUBUZZ);
    gpio_set_direction(SUBUZZ, GPIO_MODE_OUTPUT);
    gpio_reset_pin(MABUZZ);
    gpio_set_direction(MABUZZ, GPIO_MODE_OUTPUT);
    gpio_reset_pin(KYBUZZ);
    gpio_set_direction(KYBUZZ, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LEBUZZ);
    gpio_set_direction(LEBUZZ, GPIO_MODE_OUTPUT);

    gpio_set_level(SUBUZZ, 0);
    gpio_set_level(MABUZZ, 0);
    gpio_set_level(KYBUZZ, 0);
    gpio_set_level(LEBUZZ, 0);

    // Check for ADXL343
    uint8_t deviceID;
    getDeviceID(&deviceID);
    if (deviceID == 0xE5) {
        ESP_LOGI(TAG, "Found ADAXL343");
    }

    // Disable interrupts
    writeRegister(ADXL343_REG_INT_ENABLE, 0);

    // Set range and enable measurements
    setRange(ADXL343_RANGE_2_G);
    writeRegister(ADXL343_REG_POWER_CTL, 0x08);

    // Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
        adc2_config_channel_atten(channel2, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    // Create task to poll ADXL343 and send data
    xTaskCreate(test_adxl343, "test_adxl343", 4096, NULL, 5, NULL);
}
