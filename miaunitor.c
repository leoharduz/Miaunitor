#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h" // Biblioteca para usar o ADC, sensores e joystick
#include "hardware/pwm.h" // Biblioteca para usar o PWM

#define CANAL_JOYSTICK_Y 1
#define PINO_JOYSTICK_Y 27
#define CANAL_SENSOR_INTERNO 4
#define AJUSTE_TEMPERATURA 5.0f // Maximo de variacao de temperatura pelo joystick eixo y
#define LED_VERMELHO 13
#define LED_AZUL 12
#define BUZZER_A 21
#define BUZZER_B 10
#define TEMP_ALTA 40.0f // Temperatura maxima sem acionar alarme
#define TEMP_BAIXA 29.99f // Temperatura minima sem acionar alarme

// Variaveis globais
uint slice_a; // Guardar valor correspondente do slice do BUZZER A
uint slice_b; // Guardar valor correspondente do slice do BUZZER B
uint16_t volume_buzzer_a = 0; // Variar de 0 a 5000, pelo wrap de 10000
uint16_t volume_buzzer_b = 0; // Variar de 0 a 1000, pelo wrap de 2000

// Funcao para inicializar as saidas
void iniciar_saidas(){
    // Inicializar os LEDS
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);

    // Inicializar os BUZZERS
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    // Configura BUZZER A
    slice_a = pwm_gpio_to_slice_num(BUZZER_A); // Setando PWM do BUZZER A
    pwm_set_wrap(slice_a, 60000); // Define a frequencia
    pwm_set_chan_level(slice_a, PWM_CHAN_B, volume_buzzer_a); // Comeca desligado (volume 0)
    pwm_set_enabled(slice_a, true);
    // Configura BUZZER B
    slice_b = pwm_gpio_to_slice_num(BUZZER_B); // Setando PWM do BUZZER B
    pwm_set_wrap(slice_b, 40000); // Define a frequencia
    pwm_set_chan_level(slice_b, PWM_CHAN_A, volume_buzzer_b); // Comeca desligado (volume 0)
    pwm_set_enabled(slice_b, true);
}

// Liga ou desligado o alarme
void alarme_temp_estado(bool estado, bool quente){
    if(estado){
        if(quente){
        gpio_put(LED_VERMELHO, 1);
        gpio_put(LED_AZUL, 0);
        }
        else{
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_AZUL, 1);
        }
        volume_buzzer_a = 30000;
        volume_buzzer_b = 0;
        pwm_set_chan_level(slice_a, PWM_CHAN_B, volume_buzzer_a);
        pwm_set_chan_level(slice_b, PWM_CHAN_A, volume_buzzer_b);
        sleep_ms(1000);
        volume_buzzer_a = 30000;
        volume_buzzer_b = 20000;
        pwm_set_chan_level(slice_a, PWM_CHAN_B, volume_buzzer_a);
        pwm_set_chan_level(slice_b, PWM_CHAN_A, volume_buzzer_b);
    }
    else{
        gpio_put(LED_VERMELHO, 0);
        gpio_put(LED_AZUL, 0);

        volume_buzzer_a = 0;
        volume_buzzer_b = 0;
        pwm_set_chan_level(slice_a, PWM_CHAN_B, volume_buzzer_a);
        pwm_set_chan_level(slice_b, PWM_CHAN_A, volume_buzzer_b);
    }
}

// Verifica a temperatura e entrega os parametros para a funcao alarme_temp_estado
void alarme_temp_verifica(float temp_atual){
    if (temp_atual > TEMP_ALTA) {
        alarme_temp_estado(true, true);
    } 
    else if (temp_atual < TEMP_BAIXA) {
        alarme_temp_estado(true, false);
    } 
    else {
        alarme_temp_estado(false, false);
    }
}


int main() {
    stdio_init_all();

    adc_init(); // Inicializa o ADC
    adc_gpio_init(PINO_JOYSTICK_Y); // Configura o ADC para o pino do joystick eixo y
    adc_set_temp_sensor_enabled(true); // Habilita o sensor de temperatura interno

    iniciar_saidas();


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

        alarme_temp_verifica(temp_c_final);

        printf("Temp: %.2f C\n", temp_c_final); // Mostrar o valor da temperatura final

        sleep_ms(1000);
    }
}