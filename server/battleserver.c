#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

#define MAX_JOGADORES 2
#define TAMANHO_GRADE 8
#define LOG_FILE "battleship.log"
#define LOG_MSG_SIZE 256  // Tamanho fixo para mensagens de log

typedef enum {
    AGUARDANDO_CONEXAO,
    POSICIONANDO_NAVIOS,
    AGUARDANDO_READY,
    EM_JOGO,
    JOGO_TERMINADO
} GameState;

typedef struct {
    Player player[MAX_JOGADORES];
    int socket_jogador[MAX_JOGADORES];
    int jogador_da_vez;
    int jogadores_prontos;
    int navios_restantes[MAX_JOGADORES];
    GameState estado;
    pthread_mutex_t lock;
    int fim_do_jogo;
    int jogadores_conectados;
} GameController;

GameController controller;

void registrar_log(const char* mensagem) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                mensagem);
        fclose(log_file);
    }
}

void enviar_comando(int sock, const char* msg) {
    if (send(sock, msg, strlen(msg), 0) < 0) {
        perror("send failed");
    }
}

int obter_tamanho_navio_str(const char* tipo) {
    if (strcmp(tipo, "SUBMARINO") == 0) return 1;
    if (strcmp(tipo, "FRAGATA") == 0) return 2;
    if (strcmp(tipo, "DESTROYER") == 0) return 3;
    return -1;
}

void imprimir_campo(int indice) {
    printf("\nCampo do jogador %d - %s:\n", indice, controller.player[indice].nome);
    printf("  0 1 2 3 4 5 6 7\n");
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            printf("%c ", controller.player[indice].campo[i][j]);
        }
        printf("\n");
    }
}

static int navios_completos(int fragatas, int submarinos, int destroyers) {
    return (fragatas == 2 && submarinos == 1 && destroyers == 1);
}

int posicionar_navios(int sock, int indice_jogador) {
    int fragatas = 0, submarinos = 0, destroyers = 0;
    char buffer[MAX_MSG];
    char resposta[256];
    int navios_posicionados = 0;

    // Inicializa o campo
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            controller.player[indice_jogador].campo[i][j] = '~';
        }
    }

    while (!navios_completos(fragatas, submarinos, destroyers)) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            perror("recv failed or client disconnected");
            return -1;
        }

        buffer[bytes] = '\0';

        // Log truncado para caber no buffer
        char log_msg[LOG_MSG_SIZE];
        int max_data = LOG_MSG_SIZE - 50; // Reserva espaço para o texto fixo
        snprintf(log_msg, sizeof(log_msg), "RECEBIDO [%.*s] de %s", 
                 max_data, buffer, controller.player[indice_jogador].nome);
        registrar_log(log_msg);

        char tipo[16] = {0}, direcao;
        int x, y;

        if (sscanf(buffer, "POS %15s %d %d %c", tipo, &x, &y, &direcao) != 4) {
            enviar_comando(sock, "ERRO: Comando inválido. Use: POS TIPO X Y DIRECAO\n");
            continue;
        }

        // Normaliza para maiúsculas
        for (int i = 0; tipo[i]; i++) tipo[i] = toupper(tipo[i]);
        direcao = toupper(direcao);

        int tamanho = obter_tamanho_navio_str(tipo);
        if (tamanho == -1 || (direcao != 'H' && direcao != 'V')) {
            enviar_comando(sock, "ERRO: Tipo ou direção inválida. Use FRAGATA, SUBMARINO, DESTROYER e H/V.\n");
            continue;
        }

        // Verifica quantidade máxima
        if ((strcmp(tipo, "FRAGATA") == 0 && fragatas >= 2)) {
            enviar_comando(sock, "ERRO: Quantidade máxima de fragatas já posicionada (2).\n");
            continue;
        } else if (strcmp(tipo, "SUBMARINO") == 0 && submarinos >= 1) {
            enviar_comando(sock, "ERRO: Quantidade máxima de submarinos já posicionada (1).\n");
            continue;
        } else if (strcmp(tipo, "DESTROYER") == 0 && destroyers >= 1) {
            enviar_comando(sock, "ERRO: Quantidade máxima de destroyers já posicionada (1).\n");
            continue;
        }

        // Valida posição
        if (!validar_posicao(x, y, direcao, tamanho)) {
            char log_msg[100];
            snprintf(log_msg, sizeof(log_msg), 
                     "POS_INVALIDO: %s x=%d y=%d dir=%c tam=%d",
                     controller.player[indice_jogador].nome, x, y, direcao, tamanho);
            printf("%s\n", log_msg);
            registrar_log(log_msg);
            enviar_comando(sock, "ERRO: Fora dos limites do campo.\n");
            continue;
        }

        // Verifica sobreposição
        int sobreposto = 0;
        for (int i = 0; i < tamanho; i++) {
            int xi = x + (direcao == 'V' ? i : 0);
            int yi = y + (direcao == 'H' ? i : 0);
            
            if (xi < 0 || xi >= TAMANHO_GRADE || yi < 0 || yi >= TAMANHO_GRADE) {
                sobreposto = 1;
                break;
            }
            
            if (controller.player[indice_jogador].campo[xi][yi] != '~') {
                sobreposto = 1;
                break;
            }
        }

        if (sobreposto) {
            enviar_comando(sock, "ERRO: Sobreposição detectada. Escolha outra posição.\n");
            continue;
        }

        // Atualiza contadores
        if (strcmp(tipo, "FRAGATA") == 0) fragatas++;
        else if (strcmp(tipo, "SUBMARINO") == 0) submarinos++;
        else if (strcmp(tipo, "DESTROYER") == 0) destroyers++;

        // Posiciona o navio
        char navio_id = '1' + navios_posicionados;
        for (int i = 0; i < tamanho; i++) {
            int xi = x + (direcao == 'V' ? i : 0);
            int yi = y + (direcao == 'H' ? i : 0);
            controller.player[indice_jogador].campo[xi][yi] = navio_id;
        }
        
        navios_posicionados++;
        snprintf(resposta, sizeof(resposta), "OK: %s posicionado em %d,%d\n", tipo, x, y);
        enviar_comando(sock, resposta);
    }

    controller.navios_restantes[indice_jogador] = 4;
    enviar_comando(sock, "OK: Todos os navios posicionados!\n");
    imprimir_campo(indice_jogador);
    return 0;
}

void broadcast(const char* msg) {
    for (int i = 0; i < MAX_JOGADORES; i++) {
        if (controller.socket_jogador[i] > 0) {
            enviar_comando(controller.socket_jogador[i], msg);
        }
    }
}

void processar_ataque(int player_index, int x, int y) {
    int alvo_index = (player_index + 1) % MAX_JOGADORES;
    char resultado[MAX_MSG] = {0};
    char notificacao[MAX_MSG] = {0};

    if (x < 0 || x >= TAMANHO_GRADE || y < 0 || y >= TAMANHO_GRADE) {
        char log_msg[100];
        snprintf(log_msg, sizeof(log_msg),
                 "ATAQUE_INVALIDO: %s atacou (%d,%d)",
                 controller.player[player_index].nome, x, y);
        printf("%s\n", log_msg);
        registrar_log(log_msg);
        snprintf(resultado, sizeof(resultado), "ERRO: Coordenadas inválidas");
        enviar_comando(controller.socket_jogador[player_index], resultado);
        return;
    }

    char celula = controller.player[alvo_index].campo[x][y];
    
    if (celula == '~') {
        controller.player[alvo_index].campo[x][y] = 'M';
        snprintf(resultado, sizeof(resultado), "MISS");
        snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d MISS", x, y);
    } else if (celula == 'M' || celula == 'X') {
        snprintf(resultado, sizeof(resultado), "ERRO: Posição já atacada");
    } else if (isdigit(celula)) {
        char navio_id = celula;
        controller.player[alvo_index].campo[x][y] = 'X';
        
        // Verificar se navio foi afundado
        int afundado = 1;
        for (int i = 0; i < TAMANHO_GRADE && afundado; i++) {
            for (int j = 0; j < TAMANHO_GRADE; j++) {
                if (controller.player[alvo_index].campo[i][j] == navio_id) {
                    afundado = 0;
                    break;
                }
            }
        }
        
        if (afundado) {
            controller.navios_restantes[alvo_index]--;
            snprintf(resultado, sizeof(resultado), "SUNK %c", navio_id);
            snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d SUNK %c", x, y, navio_id);
        } else {
            snprintf(resultado, sizeof(resultado), "HIT");
            snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d HIT", x, y);
        }
    }

    // Enviar resultados
    enviar_comando(controller.socket_jogador[player_index], resultado);
    enviar_comando(controller.socket_jogador[alvo_index], notificacao);
}

void aguardar_ready(int sock) {
    char buffer[MAX_MSG];
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    
    buffer[bytes] = '\0';
    
    // Log truncado para caber no buffer
    char log_msg[LOG_MSG_SIZE];
    int max_data = LOG_MSG_SIZE - 40; // Reserva espaço para o texto fixo
    snprintf(log_msg, sizeof(log_msg), "RECEBIDO [%.*s] durante AGUARDAR_READY", 
             max_data, buffer);
    registrar_log(log_msg);
    
    if (strcmp(buffer, "READY") == 0) {
        pthread_mutex_lock(&controller.lock);
        controller.jogadores_prontos++;
        pthread_mutex_unlock(&controller.lock);
        
        if (controller.jogadores_prontos < MAX_JOGADORES) {
            enviar_comando(sock, "AGUARDANDO ADVERSÁRIO");
        }
    }
}

void receber_join(int sock) {
    char buffer[MAX_MSG];
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        return;
    }
    
    buffer[bytes] = '\0';
    
    // Log truncado para caber no buffer
    char log_msg[LOG_MSG_SIZE];
    int max_data = LOG_MSG_SIZE - 20; // Reserva espaço para o texto fixo
    snprintf(log_msg, sizeof(log_msg), "RECEBIDO [%.*s] em JOIN", 
             max_data, buffer);
    registrar_log(log_msg);
    
    if (strncmp(buffer, "JOIN", 4) == 0) {
        pthread_mutex_lock(&controller.lock);
        int indice = -1;
        for (int i = 0; i < MAX_JOGADORES; i++) {
            if (controller.socket_jogador[i] == 0) {
                indice = i;
                break;
            }
        }
        
        if (indice != -1) {
            controller.socket_jogador[indice] = sock;
            sscanf(buffer + 5, "%49s", controller.player[indice].nome);
            controller.jogadores_conectados++;
            printf("Jogador conectado: %s (socket %d, total: %d)\n", 
                   controller.player[indice].nome, sock, controller.jogadores_conectados);
            
            // Envia mensagem apropriada
            if (indice == 0) {
                enviar_comando(sock, "AGUARDE JOGADOR");
            } else {
                enviar_comando(sock, "JOGO INICIADO");
                enviar_comando(controller.socket_jogador[0], "JOGO INICIADO");
                
                // Envia identificação de jogador para ambos
                char msg[50];
                snprintf(msg, sizeof(msg), "VOCE_E_JOGADOR 1");
                enviar_comando(controller.socket_jogador[0], msg);
                
                snprintf(msg, sizeof(msg), "VOCE_E_JOGADOR 2");
                enviar_comando(sock, msg);
                
                controller.estado = POSICIONANDO_NAVIOS;
            }
        }
        pthread_mutex_unlock(&controller.lock);
    }
}

void* game_thread(void* arg) {
    printf("Iniciando controle do jogo...\n");
    
    // Fase de posicionamento
    for (int i = 0; i < MAX_JOGADORES; i++) {
        if (posicionar_navios(controller.socket_jogador[i], i) < 0) {
            fprintf(stderr, "Erro no posicionamento de navios do jogador %d\n", i);
            return NULL;
        }
    }
    
    // Aguardar ambos prontos
    for (int i = 0; i < MAX_JOGADORES; i++) {
        enviar_comando(controller.socket_jogador[i], "AGUARDANDO READY");
        aguardar_ready(controller.socket_jogador[i]);
    }
    
    // Iniciar jogo
    pthread_mutex_lock(&controller.lock);
    controller.estado = EM_JOGO;
    controller.jogador_da_vez = 0;
    pthread_mutex_unlock(&controller.lock);
    
    broadcast("INICIO DO JOGO");
    enviar_comando(controller.socket_jogador[0], "PLAY");
    enviar_comando(controller.socket_jogador[1], "AGUARDE");
    
    // Loop principal do jogo
    while (!controller.fim_do_jogo) {
        int current_player = controller.jogador_da_vez;
        int sock = controller.socket_jogador[current_player];
        
        // Aguardar jogada do jogador atual
        char buffer[MAX_MSG];
        ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            // Cliente desconectado
            char msg[] = "DESCONECTADO: O adversário saiu";
            broadcast(msg);
            controller.fim_do_jogo = 1;
            break;
        }
        
        buffer[bytes] = '\0';
        
        // Log truncado para caber no buffer
        char log_msg[LOG_MSG_SIZE];
        int max_data = LOG_MSG_SIZE - 30; // Reserva espaço para o texto fixo
        snprintf(log_msg, sizeof(log_msg), "RECEBIDO [%.*s] de %s", 
                 max_data, buffer, controller.player[current_player].nome);
        registrar_log(log_msg);
        
        if (strncmp(buffer, CMD_FIRE, 4) == 0) {
            int x, y;
            if (sscanf(buffer + 5, "%d %d", &x, &y) == 2) {
                pthread_mutex_lock(&controller.lock);
                
                // Processa ataque
                processar_ataque(current_player, x, y);
                
                // Verifica vitória
                if (controller.navios_restantes[(current_player + 1) % MAX_JOGADORES] == 0) {
                    enviar_comando(controller.socket_jogador[current_player], "WIN");
                    enviar_comando(controller.socket_jogador[(current_player + 1) % MAX_JOGADORES], "LOSE");
                    broadcast("FIM");
                    controller.fim_do_jogo = 1;
                    pthread_mutex_unlock(&controller.lock);
                    break;
                }
                
                // Passa a vez
                controller.jogador_da_vez = (controller.jogador_da_vez + 1) % MAX_JOGADORES;
                pthread_mutex_unlock(&controller.lock);
                
                // Notifica os jogadores
                enviar_comando(controller.socket_jogador[controller.jogador_da_vez], "PLAY");
                enviar_comando(controller.socket_jogador[(controller.jogador_da_vez + 1) % MAX_JOGADORES], "AGUARDE");
            }
        }
    }
    
    return NULL;
}

void* lidar_com_jogador(void* arg) {
    int sock = *((int*)arg);
    free(arg);
    
    receber_join(sock);
    
    // Se ambos jogadores conectados, iniciar thread de jogo
    pthread_mutex_lock(&controller.lock);
    if (controller.jogadores_conectados == MAX_JOGADORES && 
        controller.estado == POSICIONANDO_NAVIOS) {
        pthread_t game_tid;
        if (pthread_create(&game_tid, NULL, game_thread, NULL) != 0) {
            perror("Falha ao criar thread de jogo");
        } else {
            pthread_detach(game_tid);
        }
    }
    pthread_mutex_unlock(&controller.lock);
    
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Inicializar controlador
    memset(&controller, 0, sizeof(controller));
    controller.estado = AGUARDANDO_CONEXAO;
    controller.fim_do_jogo = 0;
    controller.jogador_da_vez = 0;
    controller.jogadores_conectados = 0;
    pthread_mutex_init(&controller.lock, NULL);
    
    for (int i = 0; i < MAX_JOGADORES; i++) {
        controller.socket_jogador[i] = 0;
        controller.navios_restantes[i] = 4;
    }

    // Criar socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configurar socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        printf("Erro ao vincular ao endereço %s:%d\n", 
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor aguardando conexões na porta %d...\n", PORT);

    // Aceitar conexões
    while (!controller.fim_do_jogo) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, sizeof(client_ip));
        printf("Conexão aceita de %s:%d\n", client_ip, ntohs(address.sin_port));

        // Criar thread para o jogador
        pthread_t thread_id;
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = new_socket;
        
        if (pthread_create(&thread_id, NULL, lidar_com_jogador, (void*)sock_ptr) < 0) {
            perror("pthread_create");
            close(new_socket);
            free(sock_ptr);
        } else {
            pthread_detach(thread_id);
        }
    }

    printf("Fim de jogo. Encerrando servidor.\n");
    close(server_fd);
    pthread_mutex_destroy(&controller.lock);
    return 0;
}
