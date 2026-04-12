#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h" // Biblíoteca para usar o adc, sensores e joystick
#include "pico/util/datetime.h"
#include "hardware/adc.h" // Biblioteca para usar o ADC, sensores e joystick
#include "hardware/pwm.h" // Biblioteca para usar o PWM
#include "hardware/gpio.h" // Interrupcoes
#include "ws2812.pio.h" // Matriz de LEDs

#define CANAL_JOYSTICK_Y 1
#define PINO_JOYSTICK_Y 27
#define CANAL_SENSOR_INTERNO 4
#define AJUSTE_TEMPERATURA 15.0f // Maximo de variacao de temperatura pelo joystick eixo y
#define LED_VERMELHO 13
#define LED_AZUL 12
#define BUZZER_A 21
#define BUZZER_B 10
#define TEMP_ALTA 45.0f // Temperatura maxima sem acionar alarme
#define TEMP_BAIXA 24.99f // Temperatura minima sem acionar alarme
#define BOTAO_A 5
#define BOTAO_B 6

// Variaveis globais
uint slice_a; // Guardar valor correspondente do slice do BUZZER A
uint slice_b; // Guardar valor correspondente do slice do BUZZER B
uint16_t volume_buzzer_a = 0; // Variar de 0 a 30000, pelo wrap de 60000
uint16_t volume_buzzer_b = 0; // Variar de 0 a 20000, pelo wrap de 40000
volatile uint16_t ganho_volume = 50; // Para calcular o Duty do BUZZER

// Desligar/religar matriz de LEDs
volatile bool matriz_acesa = false;
volatile uint32_t tempo_ultima_interacao = 0;

// MATRIZ LEDs
// Cada bloco de 5 números é uma linha (da esquerda para a direita)
// As duas primeiras colunas são 0 para encostar o número na direita (3x5)
const uint8_t numeros[11][25] = {
    {0,0,1,1,1, 0,0,1,0,1, 0,0,1,0,1, 0,0,1,0,1, 0,0,1,1,1}, // 0
    {0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,1, 0,0,0,0,1}, // 1 (apenas uma linha)
    {0,0,1,1,1, 0,0,0,0,1, 0,0,1,1,1, 0,0,1,0,0, 0,0,1,1,1}, // 2
    {0,0,1,1,1, 0,0,0,0,1, 0,0,1,1,1, 0,0,0,0,1, 0,0,1,1,1}, // 3
    {0,0,1,0,1, 0,0,1,0,1, 0,0,1,1,1, 0,0,0,0,1, 0,0,0,0,1}, // 4
    {0,0,1,1,1, 0,0,1,0,0, 0,0,1,1,1, 0,0,0,0,1, 0,0,1,1,1}, // 5
    {0,0,1,1,1, 0,0,1,0,0, 0,0,1,1,1, 0,0,1,0,1, 0,0,1,1,1}, // 6
    {0,0,1,1,1, 0,0,0,0,1, 0,0,0,1,0, 0,0,1,0,0, 0,0,1,0,0}, // 7
    {0,0,1,1,1, 0,0,1,0,1, 0,0,1,1,1, 0,0,1,0,1, 0,0,1,1,1}, // 8
    {0,0,1,1,1, 0,0,1,0,1, 0,0,1,1,1, 0,0,0,0,1, 0,0,1,1,1}, // 9
    {1,0,1,1,1, 1,0,1,0,1, 1,0,1,0,1, 1,0,1,0,1, 1,0,1,1,1}  // 10 (usa tudo)
};
// Converte coordenadas X e Y para o índice do LED na BitDogLab (zigue-zague)
int get_index(int x, int y) {
    // Invertemos o X para corrigir o efeito espelho (4 - x)
    // A lógica da serpente permanece para não embaralhar as linhas
    if (y % 2 == 0) {
        return (4 - y) * 5 + (4 - x); 
    } else {
        return (4 - y) * 5 + x;
    }
}
void atualizar_matriz_volume(int nivel) {
    uint32_t cor = 0x0000FF00; // Azul (Formato GRB)
    
    // Criamos um buffer temporário de 25 leds
    uint32_t buffer[25] = {0};

    // Preenche o buffer com base no desenho do número
    for (int i = 0; i < 25; i++) {
        int x = i % 5;
        int y = i / 5;
        int idx = get_index(x, y);
        
        if (numeros[nivel][i]) {
            buffer[idx] = cor;
        }
    }

    // Envia o buffer para o PIO (Pino GP07) [cite: 1]
    for (int i = 0; i < 25; i++) {
        pio_sm_put_blocking(pio0, 0, buffer[i] << 8u);
    }
}
// MATRIZ LEDs

// Incrementando ou decrementando a variavel usada para calcular o Duty, aumentando ou diminuindo o volume dos BUZZERS
void gpio_irq_handler(uint gpio, uint32_t events) {
    // Debounce
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if (tempo_atual - tempo_ultima_interacao < 200) return;
    
    // Acende a matriz de LEDs, caso esteja desligada, e nao muda valor de ganho_volume
    if (!matriz_acesa) {
        matriz_acesa = true;
        tempo_ultima_interacao = tempo_atual;
        atualizar_matriz_volume(ganho_volume / 10);
        return;
    }
    
    // Muda o valor de ganho_volume
    if (gpio == BOTAO_A) {
        if (ganho_volume >= 10) ganho_volume -= 10; // Diminui 10%
    } else if (gpio == BOTAO_B) {
        if (ganho_volume <= 90) ganho_volume += 10; // Aumenta 10%
    }

    tempo_ultima_interacao = tempo_atual;
    atualizar_matriz_volume(ganho_volume / 10);
}

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

    // Inicializa o Botao A
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    // Inicializa o Botao B
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    // Configura interrupcoes
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);

    // Iniciar matriz de LEDs
    uint offset = pio_add_program(pio0, &ws2812_program);
    ws2812_program_init(pio0, 0, offset, 7, 800000, false);
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
        // Calcula o valor para o Duty do BUZZER, de acordo com o volume selecionado
        volume_buzzer_a = (30000 * ganho_volume) / 100;
        volume_buzzer_b = 0; // Pro BUZZER B fazer um efeito de bipe
        pwm_set_chan_level(slice_a, PWM_CHAN_B, volume_buzzer_a);
        pwm_set_chan_level(slice_b, PWM_CHAN_A, volume_buzzer_b);
        sleep_ms(500); // Pro BUZZER B fazer um efeito de bipe
        volume_buzzer_a = (30000 * ganho_volume) / 100;
        volume_buzzer_b = (20000 * ganho_volume) / 100;
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


    // Variáveis para controlar o tempo, e poder retirar o sleep
    uint32_t ultima_leitura = 0;
    const uint32_t intervalo = 1000;
    iniciar_saidas();


    while (true) {
        uint32_t agora = to_ms_since_boot(get_absolute_time()); // Captura o valor de um instante no tempo
        
        // O if executa o codigo se ja tiver passado o intervalo definido, entre a captura de "agora", e a leitura atual
        if (agora - ultima_leitura >= intervalo) {
            ultima_leitura = agora;
            adc_select_input(CANAL_JOYSTICK_Y); // Seta o ADC para o joystick
            uint16_t joy_y = adc_read(); // Dá um valor inteiro referente a posicao do joystick eixo y

            adc_select_input(CANAL_SENSOR_INTERNO); // Seta o ADC para o sensor de temperatura interno
            uint16_t temp_raw = adc_read(); // Dá um valor inteiro referente a temperatura

            // Converter o valor inteiro para graus Celcius
            const float conversion_factor = 3.3f / (1 << 12);
            float voltage = temp_raw * conversion_factor;
            float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

            // Desligar a matriz de LEDs apos 3 segundos do ultimo clique em um botao
            uint32_t agora = to_ms_since_boot(get_absolute_time()); // 
            if (matriz_acesa && (agora - tempo_ultima_interacao > 3000)) {
            // Apaga a matriz enviando 25 pixels pretos (0)
            for (int i = 0; i < 25; i++) {
                pio_sm_put_blocking(pio0, 0, 0);
            }
            matriz_acesa = false;
            }

            // Recebe um valor entre -(AJUSTE_TEMPERATURA) e +(AJUSTE TEMPERATURA), pela posicao do joystick eixo y, e soma com a temperatura
            float temp_c_joy_y = ((((float)joy_y - 2048.0f) / 2048.0f) * AJUSTE_TEMPERATURA);
            float temp_c_final = temp_c + temp_c_joy_y;

            printf("Temp: %.2f C\n", temp_c_final); // Mostrar o valor da temperatura final
            alarme_temp_verifica(temp_c_final);

        }
    }
}