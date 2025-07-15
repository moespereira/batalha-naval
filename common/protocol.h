#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 8080
#define MAX_MSG 1024
#define MAX_JOGADORES 2
#define TAMANHO_GRADE 8

// Comandos
#define CMD_JOIN "JOIN"
#define CMD_READY "READY"
#define CMD_POS "POS"
#define CMD_FIRE "FIRE"
#define CMD_HIT "HIT"
#define CMD_MISS "MISS"
#define CMD_SUNK "SUNK"
#define CMD_WIN "WIN"
#define CMD_LOSE "LOSE"
#define CMD_PLAY "PLAY"
#define CMD_FIM "FIM"
#define CMD_ATACANTE "ATACANTE"
#define CMD_VOCE_E_JOGADOR "VOCE_E_JOGADOR"

typedef struct {
    char nome[50];
    char campo[TAMANHO_GRADE][TAMANHO_GRADE];
    int navios_restantes;
} Player;

// Função para validar posição (compartilhada entre cliente e servidor)
static int validar_posicao(int x, int y, char direcao, int tamanho) {
    if (x < 0 || y < 0 || x >= TAMANHO_GRADE || y >= TAMANHO_GRADE) 
        return 0;
    
    if (direcao == 'H' && (y + tamanho) > TAMANHO_GRADE)
        return 0;
    
    if (direcao == 'V' && (x + tamanho) > TAMANHO_GRADE)
        return 0;
    
    return 1;
}

#endif