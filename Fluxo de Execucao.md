# Instalação e Execução

## Requisitos
- Sistema Linux
- GCC instalado
- Git (opcional)

## Compilação
1. Clone o repositório:
```bash
git clone https://github.com/moespereira/battleship_project.git
cd batalha-naval
```
2. Terminal Linux:
```bash
Abra 3 janelas do terminal
Na janela para o servidor
    > mkdir -p bin
    > make
    > ./bin/battleserver

Nas janelas para os clientes (jogadores)
Em cada janela, use o comando
    > ./bin/battleclient
```

### 4. exemplo_execucao
```plaintext
=== SERVIDOR ===
$ ./bin/battleserver
Servidor aguardando conexões na porta 8080...
Nova conexão aceita
Jogador conectado: Alice
Nova conexão aceita
Jogador conectado: Bob
Jogo iniciado entre Alice e Bob
Alice posicionou SUBMARINO em 2,3
Alice posicionou FRAGATA em 1,1 V
...
Bob posicionou DESTROYER em 4,5 H
Ambos jogadores prontos! Iniciando partida...
Turno de Alice
Alice atacou (3,4): MISS
Turno de Bob
Bob atacou (1,1): HIT
...
Alice atacou (4,5): SUNK 4
Alice venceu!
Encerrando conexões...

=== CLIENTE 1 (Alice) ===
$ ./bin/battleclient
Conectado ao servidor!
> JOIN Alice
Servidor: AGUARDE JOGADOR
Servidor: JOGO INICIADO
Servidor: VOCE_E_JOGADOR 1

> POS SUBMARINO 2 3 H
Servidor: OK: SUBMARINO posicionado em 2,3

> POS FRAGATA 1 1 V
Servidor: OK: FRAGATA posicionado em 1,1

> POS FRAGATA 5 5 H
Servidor: OK: FRAGATA posicionado em 5,5

> POS DESTROYER 3 4 V
Servidor: OK: DESTROYER posicionado em 3,4

> READY
Servidor: AGUARDANDO ADVERSÁRIO
Servidor: INÍCIO DO JOGO
Servidor: PLAY

> FIRE 3 4
Servidor: MISS
Servidor: ATACANTE 1 1 HIT

[.. vários turnos ..]

> FIRE 4 5
Servidor: SUNK 4
Servidor: WIN
Servidor: FIM

=== CLIENTE 2 (Bob) ===
$ ./bin/battleclient
Conectado ao servidor!
> JOIN Bob
Servidor: JOGO INICIADO
Servidor: VOCE_E_JOGADOR 2

> POS SUBMARINO 0 0 H
[...]
> READY
Servidor: INÍCIO DO JOGO
Servidor: AGUARDE

Servidor: ATACANTE 3 4 MISS
> FIRE 1 1
Servidor: HIT
Servidor: PLAY

[.. vários turnos ..]

Servidor: ATACANTE 4 5 SUNK 4
Servidor: LOSE
Servidor: FIM
