#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h" // Biblíoteca para usar o adc, sensores e joystick
#include "globals.h" // "biblioteca" para externalizar variaveis
#include "pico/cyw43_arch.h" // Wifi
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

#define CANAL_JOYSTICK_Y 1
#define PINO_JOYSTICK_Y 27
#define CANAL_SENSOR_INTERNO 4
#define AJUSTE_TEMPERATURA 5.0f // Maximo de variacao de temperatura pelo joystick eixo y

// Variaveis globals.h
float limite_quente = 45.0f;
float limite_frio = 29.99f;
float temp_atual = 0.0f;
char logs_eventos[5][50] = {"Sistema Online", "", "", "", ""};

// Criando servidor http com funcao do wifi_server.c
void start_http_server(void);

int main() {
    stdio_init_all();

    adc_init(); // Inicializa o ADC
    adc_gpio_init(PINO_JOYSTICK_Y); // Configura o ADC para o pino do joystick eixo y
    adc_set_temp_sensor_enabled(true); // Habilita o sensor de temperatura interno
    
    // Conecta Wifi e inicia servidor http
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar chip Wi-Fi\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s...\n", "brisa-BeachPadua");
    int tentativas = 0;
    int conectado = -1;
    while (conectado != 0 && tentativas < 3) {
        printf("Tentativa %d de 3...\n", tentativas + 1);
        conectado = cyw43_arch_wifi_connect_timeout_ms("brisa-BeachPadua", "beachpadua1", CYW43_AUTH_WPA2_AES_PSK, 15000);
        
        if (conectado != 0) {
            printf("Falha temporaria (Erro: %d). Tentando novamente em 2s...\n", conectado);
            sleep_ms(2000);
            tentativas++;
        }
    }
    if (conectado == 0) {
        printf("CONECTADO COM SUCESSO!\n");
        printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
        start_http_server();
    } else {
        printf("Desisto!\n");
    }

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

        temp_atual = temp_c_final; // Variavel global recebe o valor da temperatura

        printf("Temp: %.2f C\n", temp_c_final); // Mostrar o valor da temperatura final

        sleep_ms(1000);
    }
}