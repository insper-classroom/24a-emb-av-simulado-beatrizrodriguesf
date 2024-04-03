/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/pwm.h"
#include "hardware/adc.h"

QueueHandle_t xQueueADC;
QueueHandle_t xQueueRGB;

const int LED_R = 18;
const int LED_G = 17;
const int LED_B = 16;

typedef struct color_x {
    int red;
    int green;
    int blue;
} color;


void wheel(int WheelPos, int *r, int *g, int *b ) {
  WheelPos = 255 - WheelPos;

  if ( WheelPos < 85 ) {
    *r = 255 - WheelPos * 3;
    *g =0;
    *b = WheelPos * 3;
  } else if( WheelPos < 170 ) {
    WheelPos -= 85;
    *r = 0;
    *g = WheelPos * 3;
    *b = 255 - WheelPos * 3;
  } else {
    WheelPos -= 170;
    *r = WheelPos * 3;
    *g = 255 - WheelPos * 3;
    *b = 0;
  }
}

void init_pwm(int pwm_pin_gp, uint resolution, uint *slice_num, uint *chan_num) {
    gpio_set_function(pwm_pin_gp, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pwm_pin_gp);
    uint chan = pwm_gpio_to_channel(pwm_pin_gp);
    pwm_set_clkdiv(slice, 125); // pwm clock should now be running at 1MHz
    pwm_set_wrap(slice, resolution);
    pwm_set_chan_level(slice, PWM_CHAN_A, 0);
    pwm_set_enabled(slice, true);

    *slice_num = slice;
    *chan_num = chan;
}

void led_task(void *p) {

    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);

    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);

    int pwm_r_slice, pwm_r_chan;
    int pwm_g_slice, pwm_g_chan;
    int pwm_b_slice, pwm_b_chan;

    init_pwm(LED_R, 256, &pwm_r_slice, &pwm_r_chan);
    init_pwm(LED_G, 256, &pwm_g_slice, &pwm_g_chan);
    init_pwm(LED_B, 256, &pwm_b_slice, &pwm_b_chan);

    color rgb_received;

    while (true) {
        if (xQueueReceive(xQueueRGB, &rgb_received, pdMS_TO_TICKS(200))) {

            printf("R: %d, G: %d, B: %d \n", rgb_received.red, rgb_received.green, rgb_received.blue);

            pwm_set_chan_level(pwm_r_slice, pwm_r_chan, rgb_received.red);
            pwm_set_chan_level(pwm_g_slice, pwm_g_chan, rgb_received.green);
            pwm_set_chan_level(pwm_b_slice, pwm_b_chan, rgb_received.blue);
      }
    }
}

bool timer_0_callback(repeating_timer_t *rt) {
    adc_select_input(0); // Select ADC input 1 (GPIO26)
    int result;
    result = adc_read();
    result = result/16;
    xQueueSendFromISR(xQueueADC, &result, 0);
    return true;
}

void adc_task() {
    adc_init();
    adc_gpio_init(26);

    int timer_0_hz = 5;
    repeating_timer_t timer_0;

    if (!add_repeating_timer_us(1000000 / timer_0_hz, 
                                timer_0_callback,
                                NULL, 
                                &timer_0)) {
        printf("Failed to add timer\n");
    }

    while (1) {
        int resistencia;
        if (xQueueReceive(xQueueADC, &resistencia, pdMS_TO_TICKS(200))) {
            printf("ADC: %d \n", resistencia);

            color rgb;
            wheel(resistencia, &rgb.red, &rgb.green , &rgb.blue);

            xQueueSend(xQueueRGB, &rgb, 0);
        }
    }
}

int main() {
    stdio_init_all();
    printf("Start LED color\n");

    xQueueRGB = xQueueCreate(32, sizeof(color));
    xQueueADC = xQueueCreate(32, sizeof(int));

    xTaskCreate(adc_task, "ADC_Task", 4096, NULL, 1, NULL);
    xTaskCreate(led_task, "LED_Task", 4096, NULL, 2, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
