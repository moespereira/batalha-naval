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
        perror("Erro ao enviar comando");
    }
}

void receber_resposta(int sock) {
    memset(resposta_servidor, 0, MAX_MSG);
    int bytes = recv(sock, resposta_servidor, MAX_MSG - 1, 0);
    if (bytes <= 0) {
        if (bytes == 0) {
            strcpy(resposta_servidor, "FIM: Conexão encerrada pelo servidor");
        } else {
            strcpy(resposta_servidor, "ERRO: Falha na conexão com o servidor");
        }
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
            if (c == '~' || c == 'M' || c == 'X') {
                printf("%c ", c);
            } else {
                printf("~ ");
            }
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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <IP_DO_SERVIDOR>\n", argv[0]);
        printf("Exemplo: %s 192.168.1.100\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    printf("Conectando ao servidor em %s:%d...\n", server_ip, PORT);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("Erro: Endereço IP inválido '%s'\n", server_ip);
        printf("Use um endereço IPv4 válido (ex: 192.168.1.100)\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro ao conectar ao servidor");
        printf("Verifique:\n");
        printf("1. Se o servidor está rodando\n");
        printf("2. Se o IP '%s' está correto\n", server_ip);
        printf("3. Se o firewall permite conexões na porta %d\n", PORT);
        close(sock);
        return 1;
    }

    printf("Conectado com sucesso ao servidor!\n");
    printf("Digite 'JOIN <seu_nome>' para entrar no jogo.\n");
    
    inicializar_tabuleiros();
    
    // Fase de conexão
    ler_comando_cmd(CMD_JOIN);
    enviar_comando(sock);
    
    // Receber confirmação de conexão
    while (1) {
        receber_resposta(sock);
        printf("Servidor: %s\n", resposta_servidor);
        
        if (strstr(resposta_servidor, CMD_VOCE_E_JOGADOR) != NULL) {
            break;
        }
        
        if (strcmp(resposta_servidor, "AGUARDE JOGADOR") == 0) {
            printf("Aguardando outro jogador conectar...\n");
            continue;
        }
        
        if (strcmp(resposta_servidor, "JOGO INICIADO") == 0) {
            printf("Jogo iniciado! Preparando para posicionar navios...\n");
            continue;
        }
    }

    // Fase de posicionamento
    printf("\n=== FASE DE POSICIONAMENTO ===\n");
    printf("Navios: 2x FRAGATA (tam=2), 1x SUBMARINO (tam=1), 1x DESTROYER (tam=3)\n");
    printf("Use: POS TIPO X Y DIRECAO (H/V)\nEx: POS FRAGATA 3 4 H\n\n");
    
    int fragatas = 0, submarinos = 0, destroyers = 0;
    int navios_posicionados = 0;
    
    while (!(fragatas == 2 && submarinos == 1 && destroyers == 1)) {
        exibir_tabuleiros();
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
        
        // Verifica quantidade máxima local
        if ((strcmp(tipo, "FRAGATA") == 0 && fragatas >= 2)) {
            printf("ERRO: Você já posicionou 2 fragatas.\n");
            continue;
        } else if (strcmp(tipo, "SUBMARINO") == 0 && submarinos >= 1) {
            printf("ERRO: Você já posicionou 1 submarino.\n");
            continue;
        } else if (strcmp(tipo, "DESTROYER") == 0 && destroyers >= 1) {
            printf("ERRO: Você já posicionou 1 destroyer.\n");
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

        // Envia o comando para o servidor
        enviar_comando(sock);
        receber_resposta(sock);
        printf("%s\n", resposta_servidor);

        // Se o servidor confirmou, atualiza o campo local
        if (strstr(resposta_servidor, "OK:") != NULL) {
            char navio_id = '1' + navios_posicionados;
            for (int i = 0; i < tamanho; i++) {
                int xi = x + (direcao == 'V' ? i : 0);
                int yi = y + (direcao == 'H' ? i : 0);
                meu_campo[xi][yi] = navio_id;
            }
            navios_posicionados++;
            
            // Atualiza contadores locais
            if (strcmp(tipo, "FRAGATA") == 0) fragatas++;
            else if (strcmp(tipo, "SUBMARINO") == 0) submarinos++;
            else if (strcmp(tipo, "DESTROYER") == 0) destroyers++;
        }
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
            // Marcar no campo do adversário como MISS
            int x, y;
            sscanf(comando + 5, "%d %d", &x, &y);
            campo_adversario[x][y] = 'M';
        }
        else if (strcmp(resposta_servidor, "HIT") == 0) {
            printf("Acerto! Você atingiu um navio.\n");
            int x, y;
            sscanf(comando + 5, "%d %d", &x, &y);
            campo_adversario[x][y] = 'X';
        }
        else if (strncmp(resposta_servidor, "SUNK", 4) == 0) {
            printf("Afundado! Você destruiu um navio.\n");
            int x, y;
            sscanf(comando + 5, "%d %d", &x, &y);
            campo_adversario[x][y] = 'X';
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
        else if (strcmp(resposta_servidor, "AGUARDE") == 0) {
            printf("Aguarde seu turno...\n");
        }
        else {
            printf("Servidor: %s\n", resposta_servidor);
        }
    }

    close(sock);
    printf("Conexão encerrada.\n");
    return 0;
}
