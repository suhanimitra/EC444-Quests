//Leyandra Burke, Suhani Mitra, Margherita Piana, Kyla Wilson 09/20/2024

/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "math.h"
#include <string.h>
#include <time.h>

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel2 = ADC_CHANNEL_9;
static const adc_atten_t atten = ADC_ATTEN_DB_11;        //0dB attenuation fix for ADC scale
static const adc_unit_t unit = ADC_UNIT_1;

#define RED 15
#define BLUE 27
#define GREEN 33
#define TILT 21
#define BUTTON 12
#define  LUX_CONVERSION_CONSTANT 1940000
//hardware interrupt defines
#define GPIO_INPUT_IO_1       4
#define ESP_INTR_FLAG_DEFAULT 0
#define GPIO_INPUT_PIN_SEL    1ULL<<GPIO_INPUT_IO_1 // casting GPIO input to bitmap

static int time_taken = 0;
static int flag = 0;

enum Status {
    READY_TO_SWALLOW,
    NORMAL_SENSING,
    DONE_SENSING
};

enum Status status = READY_TO_SWALLOW;
static bool blue_state = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg){
  flag = 1;
}

static void button_init() {
  gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_POSEDGE; // interrupt of rising edge
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL; // bit mask of the pins, use GPIO4 here
    io_conf.mode = GPIO_MODE_INPUT;            // set as input mode
    io_conf.pull_up_en = 1;                    // enable resistor pull-up mode on pin
  gpio_config(&io_conf);                       // apply parameters
  gpio_intr_enable(GPIO_INPUT_IO_1 );          // enable interrupts on pin
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);   //install gpio isr service
  gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1); //hook isr handler for specific gpio pin
}

void button_task(){
  while(1) {                               // loop forever in this task
    if(flag) {
        if ( status == DONE_SENSING) {
            status = READY_TO_SWALLOW;
            gpio_set_level(GREEN, 1);
            gpio_set_level(RED, 0);
            gpio_set_level(BLUE, 0);
            blue_state = 0;
        }
      printf("Button pressed.\n");
      flag = 0;                            // set the flag to false
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);  // wait a bit
  }
}

static void configure_gpio(void)
{
    gpio_reset_pin(RED);
    gpio_reset_pin(GREEN);
    gpio_reset_pin(BLUE);
    gpio_reset_pin(TILT);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLUE, GPIO_MODE_OUTPUT);
    gpio_set_direction(TILT, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
}

static int read_tilt(void) 
{
    return gpio_get_level(TILT);
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

void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    configure_gpio();
    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(channel, atten);
        adc2_config_channel_atten(channel2, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }
    button_init();
    xTaskCreate(button_task, "button_task", 1024*2, NULL, 10, NULL);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //Time Starts
    clock_t t;
    t = clock();

    //Continuously sample ADC1
    while (1) {
        uint32_t adc_reading = 0;
        uint32_t adc_reading2 = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
                int raw;
                adc2_get_raw((adc2_channel_t)channel2, ADC_WIDTH_BIT_12, &raw);
                adc_reading2 += raw;
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        adc_reading2 /= NO_OF_SAMPLES;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);  //photocell
        uint32_t voltage2 = esp_adc_cal_raw_to_voltage(adc_reading2, adc_chars);    //thermistor

        //using code from the following source: https://www.e-tinkers.com/2019/10/using-a-thermistor-with-arduino-and-unexpected-esp32-adc-non-linearity/
        float Ro = 10000.0;
        float beta = 3435;
        
        //Finding celcius
        float thermistor_r = Ro * (3300.0 / voltage2 - 1.0);
        printf("Thermistor resistance: %f\n", thermistor_r);

        float To = 298.15; 
        float kelvin = 1.0/(1.0/To + log(thermistor_r/Ro)/beta);
        float celcius = kelvin - 273.15;

        //Conversion code: ttps://forums.adafruit.com/viewtopic.php?t=48551 
        //Finding lux
        //float lux = 27.565 * pow(10, voltage/1000);
        uint32_t voltage_toresistance;
        float Rt;
        float R1=10000.0; //voltage divider resistor value
         //voltage_toresistance = (voltage/1000) * 3300/1023.0; //3.3 supplied voltage and 1023 is adc max
        // Rt = R1 * ((3300- voltage)/voltage);
        Rt=R1*(3300.0/voltage-1.0);
         //T = 1/(1/To + log(Rt/Ro)/BCOEFFICIENT);  // Temperature in Kelvin
         //Tc = T - 273.15;                 // Celsius
        float lux = LUX_CONVERSION_CONSTANT / Rt;
        //Finding tilt
        char tilt[20];
        int tilt_int = read_tilt();
        if (tilt_int == 0) {
            strcpy(tilt, "vertical"); 
        } else {
            strcpy(tilt, "horizontal");
        }

        if ( status == READY_TO_SWALLOW && lux >= 100 )
        {
            gpio_set_level(GREEN, 1);
            gpio_set_level(RED, 0);
            gpio_set_level(BLUE, 0);
            blue_state = 0;
        } else if ( status == READY_TO_SWALLOW && lux < 100 ) {
            //transition to next state, NORMAL SENSING
            status = NORMAL_SENSING;
            gpio_set_level(GREEN, 0);
            gpio_set_level(RED, 0);
            gpio_set_level(BLUE, 1);
            blue_state = 1;
        } else if (status == NORMAL_SENSING && lux < 100) {
            gpio_set_level(GREEN, 0);
            gpio_set_level(RED, 0);
            blue_state = !blue_state;
            gpio_set_level(BLUE, (int)blue_state);
        } else if ( status == NORMAL_SENSING && lux >= 100 ) {
            status = DONE_SENSING;
            gpio_set_level(GREEN, 0);
            gpio_set_level(RED, 1);
            gpio_set_level(BLUE, 0);
            blue_state = 0;
        } 

        // //Finding time
        // //take_enter();
        // t = clock() - t;
        // double time_taken = ((double)t)/CLOCKS_PER_SEC; // calculate the elapsed time

        //Display output
        printf("Time: %d s, Temp: %lf C, Light: %f Lux, Battery1 (photo) %ld mV, Battery2 (temp) %ld mV, Tilt: %s\n", time_taken, celcius, lux, voltage, voltage2, tilt);

        vTaskDelay(pdMS_TO_TICKS(2000));        //report every two seconds
        time_taken += 2;
    }
}


//we expect Vs = (3.3/{10000 + 10000 (there are resistor / thermistor resistance values)}) * 10000 (r2) = 1.65 Volts
