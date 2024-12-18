# tt-motor-ledc
2024-11-09

We've adopted the ledc (LED control) drive from the ESP32 distribution
to use as a simpler way to drive the TT motors.

## Basics

For a two-motor robot, we need two PWM signals (to vary input power
and thus speeed) from 0--100%. An additional GPIOs are used to control
directionusing an H-bridge. The H-bridge also serves to isolate the
power levels in the system (3.3V for the ESP and 5V for driving the
motors through the H-bridge).

We've tested this approach with the following parameters:

```
    .duty_resolution = LEDC_TIMER_10_BIT,
    .freq_hz = 50,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .clk_cfg = LEDC_AUTO_CLK,
 ```

For two motor control you will need to use LEDC_TIMER_0 and LEDC_TIMER_1.  You will need to try
different PWM duty cycles to identify good speed levels for an
appropriate robot forward motion.

The rest of this README relates to the LEDC Basic Example


# _LEDC Basic Example_

(abbreviated)

### Configure the project

The example uses fixed PWM frequency of 4 kHz, duty cycle in 50%, and
output GPIO pin. To change them, adjust `LEDC_FREQUENCY`, `LEDC_DUTY`,
`LEDC_OUTPUT_IO` macros at the top of ledc_basic_example_main.c.

Depending on the selected `LEDC_FREQUENCY`, you will need to change
the `LEDC_DUTY_RES`.

To dynamically set the duty and frequency, you can use the following
functions:

To set the frequency to 50 HZ i.e:

```c
ledc_set_freq(LEDC_MODE, LEDC_TIMER, 50);
```

Now set the duty to 50% i.e:

```c
ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512;
ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
```

To change the duty cycle you need to calculate the duty range
according to the duty resolution.

If duty resolution is 10 bits:

Duty range: `0 to (2 ** 10) = 1024` where 0 is 0% and 1024 is
100%. There is a warning in the code indicating that 100% is not
achievable so please limit the upper bound to 1023.


