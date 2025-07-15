# Protocolo de Comunicação

## Fluxo Principal
## 🔄 Protocolo de Comunicação – Diagrama e Explicação

### 1. Comando `JOIN`

```plaintext
[ Cliente A ]                          [ Servidor ]                          [ Cliente B ]
     | -------- "JOIN <nome_A>" ----------> |                                      |
     |                                      | <-------- "AGUARDE JOGADOR" -------- |
     |                                      | <-------- "JOIN <nome_B>" ---------- |
     |                                      | -------- "JOGO INICIADO" --------->  |
     | <-------- "JOGO INICIADO" ---------- | -------- "JOGO INICIADO" --------->  |
     | <-------- "Você é o Jogador 1" ----- | -------- "Você é o Jogador 2" -----> |
```

**Descrição:** Os clientes enviam `JOIN <nome>` para se registrarem no servidor. Quando dois clientes estão conectados, o servidor envia `JOGO INICIADO` e define os papéis (Jogador 1 e 2).

---

### 2. Comando `READY`

```plaintext
[ Cliente A ]                           [ Servidor ]                           [ Cliente B ]
     | -------- "READY" -------------------> |                                       |
     | <------ "AGUARDANDO ADVERSÁRIO" -----| -- "AGUARDANDO ADVERSÁRIO" --------> |
     |                                       | <------------------- "READY" --------|
     | <--------- "INÍCIO DO JOGO" --------- | --------- "INÍCIO DO JOGO" --------> |
     | <------ "Seu turno!" (Jogador 1) ---- | ---- "Aguarde o turno do outro" ---> |
```

**Descrição:** Após posicionar os navios, cada cliente envia `READY`. O jogo só começa quando ambos estiverem prontos. O servidor então informa quem começa o jogo.

---


## Comandos do Cliente
| Comando        | Parâmetros               | Descrição                          |
|----------------|--------------------------|------------------------------------|
| `JOIN <nome>`  | Nome do jogador          | Registra no servidor              |
| `POS`          | Tipo, x, y, direção      | Posiciona navio                   |
| `READY`        | -                        | Indica fim de posicionamento      |
| `FIRE <x> <y>` | Coordenadas (0-7)        | Realiza um ataque                 |

## Comandos do Servidor
| Comando                | Parâmetros          | Descrição                          |
|------------------------|---------------------|------------------------------------|
| `AGUARDE JOGADOR`      | -                   | Aguardando segundo jogador         |
| `JOGO INICIADO`        | -                   | Ambos jogadores conectados         |
| `VOCE_E_JOGADOR <n>`   | 1 ou 2              | Informa número do jogador          |
| `AGUARDANDO ADVERSÁRIO`| -                   | Aguardando outro jogador ficar pronto |
| `PLAY`                 | -                   | Indica que é seu turno             |
| `AGUARDE`              | -                   | Não é seu turno                    |
| `MISS`                 | -                   | Ataque na água                     |
| `HIT`                  | -                   | Acerto em navio                    |
| `SUNK <id>`            | ID do navio (1-4)   | Navio afundado                     |
| `ATACANTE x y RESULT`  | Coord. + resultado  | Notifica ataque adversário         |
| `WIN`                  | -                   | Vitória                            |
| `LOSE`                 | -                   | Derrota                            |
| `FIM`                  | -                   | Fim do jogo                        |
| `ERRO: <mensagem>`     | Mensagem de erro    | Erro no comando                    |