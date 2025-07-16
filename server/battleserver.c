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
#include "../common/protocol.h"

#define MAX_JOGADORES 2
#define TAMANHO_GRADE 8
#define LOG_FILE "battleship.log"

char comando[MAX_MSG];
int socket_jogador[MAX_JOGADORES];
int jogador_da_vez = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Player player[MAX_JOGADORES];
int jogadores_conectados = 0;
int players_ready = 0;
int fim_do_jogo = 0;

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
    printf("\nCampo do jogador %d - %s:\n", indice, player[indice].nome);
    printf("  0 1 2 3 4 5 6 7\n");
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            printf("%c ", player[indice].campo[i][j]);
        }
        printf("\n");
    }
}

// Função auxiliar para verificar contagem de navios
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
            player[indice_jogador].campo[i][j] = '~';
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
        if ((strcmp(tipo, "FRAGATA") == 0 && fragatas >= 2) {
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
                     player[indice_jogador].nome, x, y, direcao, tamanho);
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
            
            if (player[indice_jogador].campo[xi][yi] != '~') {
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
            player[indice_jogador].campo[xi][yi] = navio_id;
        }
        
        navios_posicionados++;
        snprintf(resposta, sizeof(resposta), "OK: %s posicionado em %d,%d\n", tipo, x, y);
        enviar_comando(sock, resposta);
    }

    player[indice_jogador].navios_restantes = 4; // 4 navios no total
    enviar_comando(sock, "OK: Todos os navios posicionados!\n");
    imprimir_campo(indice_jogador);
    return 0;
}

void broadcast(const char* msg) {
    for (int i = 0; i < MAX_JOGADORES; i++) {
        if (socket_jogador[i] > 0) {
            enviar_comando(socket_jogador[i], msg);
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
                 player[player_index].nome, x, y);
        registrar_log(log_msg);
        snprintf(resultado, sizeof(resultado), "ERRO: Coordenadas inválidas");
        enviar_comando(socket_jogador[player_index], resultado);
        return;
    }

    char celula = player[alvo_index].campo[x][y];
    
    if (celula == '~') {
        player[alvo_index].campo[x][y] = 'M';
        snprintf(resultado, sizeof(resultado), "MISS");
        snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d MISS", x, y);
    } else if (celula == 'M' || celula == 'X') {
        snprintf(resultado, sizeof(resultado), "ERRO: Posição já atacada");
    } else if (isdigit(celula)) {
        char navio_id = celula;
        player[alvo_index].campo[x][y] = 'X';
        
        // Verificar se navio foi afundado
        int afundado = 1;
        for (int i = 0; i < TAMANHO_GRADE && afundado; i++) {
            for (int j = 0; j < TAMANHO_GRADE; j++) {
                if (player[alvo_index].campo[i][j] == navio_id) {
                    afundado = 0;
                    break;
                }
            }
        }
        
        if (afundado) {
            player[alvo_index].navios_restantes--;
            snprintf(resultado, sizeof(resultado), "SUNK %c", navio_id);
            snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d SUNK %c", x, y, navio_id);
        } else {
            snprintf(resultado, sizeof(resultado), "HIT");
            snprintf(notificacao, sizeof(notificacao), "ATACANTE %d %d HIT", x, y);
        }
    }

    // Enviar resultados
    enviar_comando(socket_jogador[player_index], resultado);
    enviar_comando(socket_jogador[alvo_index], notificacao);
}

void aguardar_ready(int sock) {
    char buffer[MAX_MSG];
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    
    buffer[bytes] = '\0';
    
    if (strcmp(buffer, "READY") == 0) {
        players_ready++;
        if (players_ready < MAX_JOGADORES) {
            enviar_comando(sock, "AGUARDANDO ADVERSÁRIO");
        }
    }
}

void receber_join(int sock) {
    char buffer[MAX_MSG];
    ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    
    buffer[bytes] = '\0';
    
    if (strncmp(buffer, "JOIN", 4) == 0) {
        pthread_mutex_lock(&lock);
        int indice = jogadores_conectados;
        socket_jogador[indice] = sock;
        sscanf(buffer + 5, "%49s", player[indice].nome);
        printf("Jogador conectado: %s\n", player[indice].nome);
        jogadores_conectados++;
        pthread_mutex_unlock(&lock);
    }
}

void* lidar_com_jogador(void* arg) {
    int sock = *((int*)arg);
    free(arg);
    
    int player_index = -1;
    
    receber_join(sock);
    
    // Identificar o índice do jogador
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_JOGADORES; i++) {
        if (socket_jogador[i] == sock) {
            player_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&lock);

    if (player_index == -1) {
        close(sock);
        return NULL;
    }

    if (jogadores_conectados < MAX_JOGADORES) {
        enviar_comando(sock, "AGUARDE JOGADOR");
    } else {
        broadcast("JOGO INICIADO");
        for (int i = 0; i < MAX_JOGADORES; i++) {
            char msg[50];
            snprintf(msg, sizeof(msg), "VOCE_E_JOGADOR %d", i + 1);
            enviar_comando(socket_jogador[i], msg);
        }
    }

    // Aguarda ambos conectados
    while (jogadores_conectados < MAX_JOGADORES) {
        sleep(1);
    }

    // Fase de posicionamento
    if (posicionar_navios(sock, player_index) < 0) {
        close(sock);
        return NULL;
    }

    // Fase de ready
    enviar_comando(sock, "AGUARDANDO READY");
    aguardar_ready(sock);

    // Aguarda ambos prontos
    while (players_ready < MAX_JOGADORES) {
        sleep(1);
    }

    // Inicia jogo
    broadcast("INICIO DO JOGO");
    if (player_index == 0) {
        enviar_comando(sock, "PLAY 1");
    } else {
        enviar_comando(sock, "AGUARDE");
    }

    // Loop principal do jogo
    fd_set read_fds;
    struct timeval tv;
    char buffer[MAX_MSG];
    
    while (!fim_do_jogo) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        
        tv.tv_sec = 30;
        tv.tv_usec = 0;

        int rv = select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (rv > 0) {
            if (FD_ISSET(sock, &read_fds)) {
                ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) {
                    // Cliente desconectado
                    char msg[] = "DESCONECTADO: O adversário saiu";
                    broadcast(msg);
                    fim_do_jogo = 1;
                    break;
                }
                
                buffer[bytes] = '\0';
                
                if (strncmp(buffer, CMD_FIRE, 4) == 0) {
                    int x, y;
                    if (sscanf(buffer + 5, "%d %d", &x, &y) == 2) {
                        pthread_mutex_lock(&lock);
                        
                        // Processa ataque
                        processar_ataque(player_index, x, y);
                        
                        // Verifica vitória
                        if (player[(player_index + 1) % MAX_JOGADORES].navios_restantes == 0) {
                            enviar_comando(socket_jogador[player_index], "WIN");
                            enviar_comando(socket_jogador[(player_index + 1) % MAX_JOGADORES], "LOSE");
                            broadcast("FIM");
                            fim_do_jogo = 1;
                        } else {
                            // Passa a vez
                            jogador_da_vez = (jogador_da_vez + 1) % MAX_JOGADORES;
                            enviar_comando(socket_jogador[jogador_da_vez], "PLAY");
                            enviar_comando(socket_jogador[(jogador_da_vez + 1) % MAX_JOGADORES], "AGUARDE");
                        }
                        
                        pthread_mutex_unlock(&lock);
                    }
                }
            }
        } else if (rv == 0) {
            // Timeout
            enviar_comando(sock, "ERRO: Tempo excedido");
            pthread_mutex_lock(&lock);
            jogador_da_vez = (player_index + 1) % MAX_JOGADORES;
            enviar_comando(socket_jogador[jogador_da_vez], "PLAY");
            pthread_mutex_unlock(&lock);
        }
    }

    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Inicializar sockets
    for (int i = 0; i < MAX_JOGADORES; i++) {
        socket_jogador[i] = 0;
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
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Servidor aguardando conexões na porta %d...\n", PORT);

    // Aceitar conexões
    while (jogadores_conectados < MAX_JOGADORES) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("Nova conexão aceita\n");

        // Criar thread para o jogador
        pthread_t thread_id;
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = new_socket;
        
        if (pthread_create(&thread_id, NULL, lidar_com_jogador, (void*)sock_ptr) < 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        
        pthread_detach(thread_id);
    }

    // Manter servidor ativo até fim do jogo
    while (!fim_do_jogo) {
        sleep(1);
    }

    printf("Fim de jogo. Encerrando servidor.\n");
    close(server_fd);
    return 0;
}
