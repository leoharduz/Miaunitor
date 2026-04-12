#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h" // Biblíoteca para usar o adc, sensores e joystick

#define CANAL_JOYSTICK_Y 1
#define PINO_JOYSTICK_Y 27
#define CANAL_SENSOR_INTERNO 4
#define AJUSTE_TEMPERATURA 5.0f // Maximo de variacao de temperatura pelo joystick eixo y

int main() {
    stdio_init_all();

    adc_init(); // Inicializa o ADC
    adc_gpio_init(PINO_JOYSTICK_Y); // Configura o ADC para o pino do joystick eixo y
    adc_set_temp_sensor_enabled(true); // Habilita o sensor de temperatura interno

    while (true) {
        adc_select_input(CANAL_JOYSTICK_Y); // Seta o ADC para o joystick
        uint16_t joy_y = adc_read(); // Dá um valor inteiro referente a posicao do joystick eixo y

        adc_select_input(CANAL_SENSOR_INTERNO); // Seta o ADC para o sensor de temperatura interno
        uint16_t temp_raw = adc_read(); // Dá um valor inteiro referente a temperatura

        // Converter o valor inteiro para graus Celcius
        const float conversion_factor = 3.3f / (1 << 12);
        float voltage = temp_raw * conversion_factor;
        float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

        // Recebe um valor entre -(AJUSTE_TEMPERATURA) e +(AJUSTE TEMPERATURA), pela posicao do joystick eixo y, e soma com a temperatura
        float temp_c_joy_y = ((((float)joy_y - 2048.0f) / 2048.0f) * AJUSTE_TEMPERATURA);
        float temp_c_final = temp_c + temp_c_joy_y;

        printf("Temp: %.2f C\n", temp_c_final); // Mostrar o valor da temperatura final

        sleep_ms(1000);
    }
}