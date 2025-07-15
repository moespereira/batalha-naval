# Regras do Jogo Batalha Naval

## Objetivo
Afundar todos os navios do adversário antes que ele afunde os seus.

## Tabuleiro
- Grade 8x8 posições
- Coordenadas de (0,0) a (7,7)

## Navios
| Tipo        | Quantidade | Tamanho | Representação |
|-------------|------------|---------|---------------|
| Submarino   | 1          | 1x1     | '1'           |
| Fragata     | 2          | 2x1     | '2' e '3'     |
| Destroyer   | 1          | 3x1     | '4'           |

## Fases do Jogo

### 1. Conexão
- Dois jogadores se conectam ao servidor com `JOIN <nome>`
- São atribuídos como Jogador 1 e Jogador 2

### 2. Posicionamento
Cada jogador posiciona seus navios com:
- Tipos válidos: `SUBMARINO`, `FRAGATA`, `DESTROYER`
- Coordenadas: 0-7
- Direção: `H` (Horizontal) ou `V` (Vertical)

### 3. Preparação
- Após posicionar todos os navios, jogadores enviam `READY`
- O jogo inicia quando ambos estão prontos

### 4. Batalha
- Turnos alternados
- No seu turno, envie: `FIRE <X> <Y>`
- Resultados possíveis:
  - `MISS`: Água (não acertou)
  - `HIT`: Acertou um navio
  - `SUNK`: Afundou um navio completo

### 5. Fim de Jogo
- Vitória: Afundar todos os navios adversários
- Derrota: Ter todos os navios afundados
- Ambos jogadores recebem `FIM` ao término