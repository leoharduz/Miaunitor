#include "globals.h" // Importante para enxergar temperatura e logs
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

char http_response[2048]; 

// Função que monta a página HTML apenas para visualização
void create_http_response() {
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"2\">" 
        "<title>MiauMonitor IoT</title>"
        "<style>body{font-family:sans-serif; text-align:center; background-color:#f4f4f9;}"
        ".card{background:white; padding:20px; border-radius:15px; display:inline-block; margin-top:50px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); min-width:300px;}"
        "h1{color:#333;} .temp{font-size:3.5em; color:#ff6b6b; margin:20px 0;}"
        ".logs{text-align:left; background:#fff5f5; padding:10px; border-radius:10px; border:1px solid #ffe3e3;}"
        "ul{list-style:none; padding:0;} li{margin-bottom:5px; color:#666; font-size:0.9em;}"
        "</style></head><body>"
        "<div class='card'>"
        "<h1>🐱 Miaunitor</h1>"
        "<p>Temperatura Atual:</p>"
        "<div class='temp'><b>%.2f °C</b></div>"
        
        "<hr><h3>Histórico de Alertas:</h3>"
        "<div class='logs'><ul>"
        "<li>• %s</li>"
        "<li>• %s</li>"
        "<li>• %s</li>"
        "</ul></div>"
        "<p><a href=\"/\" style='text-decoration:none; color:#007bff;'>🔄 Atualizar Agora</a></p>"
        "</div></body></html>\r\n",
        temp_atual, 
        logs_eventos[0], logs_eventos[1], logs_eventos[2]);
}

// Callback processa apenas a requisição de visualização
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Montagem da resposta (limites agora são apenas leitura)
    create_http_response();

    // Envio dos dados com o "empurrão" do tcp_output
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb); 

    pbuf_free(p);
    return ERR_OK;
}

// Inicia o servidor na porta 80
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) return;
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}