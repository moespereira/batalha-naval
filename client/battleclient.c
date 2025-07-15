#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <ctype.h>
#include "../common/protocol.h"

#define TAMANHO_GRADE 8

char resposta_servidor[MAX_MSG];
char comando[MAX_MSG];
char meu_campo[TAMANHO_GRADE][TAMANHO_GRADE];
char campo_adversario[TAMANHO_GRADE][TAMANHO_GRADE];

void limpar_entrada() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void ler_comando_cmd(const char* protocolo) {
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(comando, MAX_MSG, stdin) == NULL) {
            continue;
        }
        
        comando[strcspn(comando, "\n")] = '\0';
        
        if (strlen(comando) == 0) {
            continue;
        }
        
        if (strstr(comando, protocolo) != NULL) {
            break;
        } else {
            printf("Comando inválido. Deve começar com '%s'. Tente novamente.\n", protocolo);
        }
    }
}

void enviar_comando(int sock) {
    if (send(sock, comando, strlen(comando), 0) < 0) {
        perror("send failed");
    }
}

void receber_resposta(int sock) {
    memset(resposta_servidor, 0, MAX_MSG);
    int bytes = recv(sock, resposta_servidor, MAX_MSG - 1, 0);
    if (bytes <= 0) {
        strcpy(resposta_servidor, "FIM");
    } else {
        resposta_servidor[bytes] = '\0';
    }
}

void exibir_tabuleiros() {
    printf("\nSEU CAMPO:\n  0 1 2 3 4 5 6 7\n");
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            printf("%c ", meu_campo[i][j]);
        }
        printf("\n");
    }
    
    printf("\nCAMPO ADVERSÁRIO:\n  0 1 2 3 4 5 6 7\n");
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            char c = campo_adversario[i][j];
            // Não revela navios não descobertos
            printf("%c ", (c >= '1' && c <= '9') ? '~' : c);
        }
        printf("\n");
    }
}

void inicializar_tabuleiros() {
    for (int i = 0; i < TAMANHO_GRADE; i++) {
        for (int j = 0; j < TAMANHO_GRADE; j++) {
            meu_campo[i][j] = '~';
            campo_adversario[i][j] = '~';
        }
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("address conversion failed");
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        return 1;
    }

    printf("Conectado ao servidor!\n");
    printf("Digite 'JOIN <seu_nome>' para entrar no jogo.\n");
    
    inicializar_tabuleiros();
    
    // Fase de conexão
    ler_comando_cmd(CMD_JOIN);
    enviar_comando(sock);
    
    // Receber confirmação de conexão
    while (1) {
        receber_resposta(sock);
        printf("%s\n", resposta_servidor);
        
        if (strstr(resposta_servidor, CMD_VOCE_E_JOGADOR) != NULL) {
            break;
        }
    }

    // Fase de posicionamento
    printf("\n=== FASE DE POSICIONAMENTO ===\n");
    printf("Navios: 2x FRAGATA (tam=2), 1x SUBMARINO (tam=1), 1x DESTROYER (tam=3)\n");
    printf("Use: POS TIPO X Y DIRECAO (H/V)\nEx: POS FRAGATA 3 4 H\n\n");
    
    exibir_tabuleiros();
    
    while (1) {
        ler_comando_cmd(CMD_POS);
        
        // Validação local antes de enviar
        char tipo[16] = {0}, direcao;
        int x, y;
        if (sscanf(comando, "POS %15s %d %d %c", tipo, &x, &y, &direcao) != 4) {
            printf("ERRO: Formato inválido. Use: POS TIPO X Y DIRECAO\n");
            continue;
        }

        // Normalizar entrada
        for (int i = 0; tipo[i]; i++) tipo[i] = toupper(tipo[i]);
        direcao = toupper(direcao);
        
        int tamanho = 0;
        if (strcmp(tipo, "FRAGATA") == 0) tamanho = 2;
        else if (strcmp(tipo, "SUBMARINO") == 0) tamanho = 1;
        else if (strcmp(tipo, "DESTROYER") == 0) tamanho = 3;
        else {
            printf("ERRO: Tipo inválido. Use FRAGATA, SUBMARINO ou DESTROYER\n");
            continue;
        }
        
        if (direcao != 'H' && direcao != 'V') {
            printf("ERRO: Direção inválida. Use H ou V\n");
            continue;
        }
        
        // Validação unificada
        if (!validar_posicao(x, y, direcao, tamanho)) {
            printf("ERRO: Posição inválida! Fora dos limites do campo.\n");
            continue;
        }
        
        // Verificação local de sobreposição
        int sobreposto = 0;
        for (int i = 0; i < tamanho; i++) {
            int xi = x + (direcao == 'V' ? i : 0);
            int yi = y + (direcao == 'H' ? i : 0);
            
            if (xi < 0 || xi >= TAMANHO_GRADE || yi < 0 || yi >= TAMANHO_GRADE) {
                sobreposto = 1;
                break;
            }
            
            if (meu_campo[xi][yi] != '~') {
                sobreposto = 1;
                break;
            }
        }
        
        if (sobreposto) {
            printf("ERRO: Posição sobreposta ou inválida!\n");
            continue;
        }

        // Atualiza campo localmente
        char navio_id = '1' + (strstr(resposta_servidor, "OK:") ? 1 : 0); // Aproximação
        for (int i = 0; i < tamanho; i++) {
            int xi = x + (direcao == 'V' ? i : 0);
            int yi = y + (direcao == 'H' ? i : 0);
            meu_campo[xi][yi] = navio_id;
        }
        
        enviar_comando(sock);
        receber_resposta(sock);
        printf("%s\n", resposta_servidor);

        if (strstr(resposta_servidor, "Todos os navios") != NULL) {
            break;
        }
        
        exibir_tabuleiros();
    }

    // Fase de preparação
    printf("\nDigite 'READY' quando estiver pronto\n");
    ler_comando_cmd(CMD_READY);
    enviar_comando(sock);
    receber_resposta(sock);
    printf("%s\n", resposta_servidor);

    // Loop principal do jogo
    printf("\n=== FASE DE ATAQUE ===\n");
    
    while (1) {
        receber_resposta(sock);
        
        // Comando PLAY indica que é sua vez
        if (strstr(resposta_servidor, CMD_PLAY) != NULL) {
            exibir_tabuleiros();
            printf("\nSUA VEZ! Digite 'FIRE X Y' (ex: FIRE 3 4)\n");
            ler_comando_cmd(CMD_FIRE);
            
            // Validar coordenadas de ataque localmente
            int x, y;
            if (sscanf(comando, "FIRE %d %d", &x, &y) != 2) {
                printf("ERRO: Formato inválido. Use FIRE X Y\n");
                continue;
            }
            if (x < 0 || x >= TAMANHO_GRADE || y < 0 || y >= TAMANHO_GRADE) {
                printf("ERRO: Coordenadas fora do tabuleiro (0 a %d)\n", TAMANHO_GRADE-1);
                continue;
            }
            
            enviar_comando(sock);
        }
        // Comando ATACANTE indica resultado do adversário
        else if (strstr(resposta_servidor, CMD_ATACANTE) != NULL) {
            int x, y;
            char resultado[10];
            sscanf(resposta_servidor + 9, "%d %d %s", &x, &y, resultado);
            
            if (strcmp(resultado, "MISS") == 0) {
                campo_adversario[x][y] = 'M';
            } else if (strcmp(resultado, "HIT") == 0) {
                campo_adversario[x][y] = 'X';
            } else if (strncmp(resultado, "SUNK", 4) == 0) {
                campo_adversario[x][y] = 'X';
            }
            
            exibir_tabuleiros();
            printf("Adversário atacou: %s\n", resposta_servidor);
        }
        // Resultados de seu próprio ataque
        else if (strcmp(resposta_servidor, "MISS") == 0) {
            printf("Água! Nenhum navio atingido.\n");
        }
        else if (strcmp(resposta_servidor, "HIT") == 0) {
            printf("Acerto! Você atingiu um navio.\n");
        }
        else if (strncmp(resposta_servidor, "SUNK", 4) == 0) {
            printf("Afundado! Você destruiu um navio.\n");
        }
        // Fim de jogo
        else if (strcmp(resposta_servidor, "WIN") == 0) {
            printf("\nPARABÉNS! VOCÊ VENCEU!\n");
            break;
        }
        else if (strcmp(resposta_servidor, "LOSE") == 0) {
            printf("\nVOCÊ PERDEU! TENTE NOVAMENTE.\n");
            break;
        }
        else if (strcmp(resposta_servidor, "FIM") == 0) {
            printf("\nFim de jogo!\n");
            break;
        }
        else if (strstr(resposta_servidor, "ERRO:") != NULL) {
            printf("ERRO: %s\n", resposta_servidor);
        }
        else {
            printf("Servidor: %s\n", resposta_servidor);
        }
    }

    close(sock);
    printf("Conexão encerrada.\n");
    return 0;
}