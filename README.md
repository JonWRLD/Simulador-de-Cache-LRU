# Simulador de Cache LRU

Projeto Final — Estrutura de Dados 2026-1

Simulador de cache com política **LRU (Least Recently Used)** implementado em C, combinando uma **tabela hash com encadeamento separado** e uma **lista duplamente ligada com sentinelas** para garantir operações de get e put em tempo O(1) amortizado.

---

## Compilação e execução

```bash
gcc -std=c11 -Wall -Wextra -O2 lru.c -o lru
./lru < teste_base.txt
```

Os comandos são lidos da entrada padrão, um por linha. Linhas em branco e linhas começando com `#` são ignoradas.

---

## Estrutura interna

```
Cache
├── Hash (tabela hash — acesso O(1))
│   └── buckets[]
│       └── HashEntry → HashEntry → ...   (encadeamento separado)
│               └── .node → aponta para o Node na lista
│
└── Lista duplamente ligada (ordem de uso)
        head ↔ [MRU] ↔ ... ↔ [LRU] ↔ tail
```

A lista mantém a invariante de que o nó mais recentemente usado fica logo após `head` (MRU) e o candidato à remoção fica logo antes de `tail` (LRU). A tabela hash guarda ponteiros diretos para os nós da lista, evitando qualquer busca linear.

---

## Comandos disponíveis

Todos os comandos são lidos linha a linha. A primeira letra da linha determina a operação.

| Comando | Sintaxe | Descrição |
|---|---|---|
| `C` | `C <capacidade>` | Cria a cache com a capacidade informada. Deve ser o primeiro comando. |
| `G` | `G <chave>` | Busca um valor pela chave. Imprime `HIT <valor>` ou `MISS`. |
| `P` | `P <chave> <valor>` | Insere ou atualiza um par chave-valor. Pode acionar eviction. |
| `D` | `D` | Exibe o conteúdo atual da cache, do MRU ao LRU. |
| `S` | `S` | Exibe estatísticas acumuladas. |
| `X` | `X` | Encerra o programa. |

---

## Saídas possíveis

```
HIT <valor>          → chave encontrada em G
MISS                 → chave não encontrada em G
INSERTED             → nova chave inserida em P
UPDATED              → chave já existia, valor atualizado em P
EVICTED <chave> INSERTED → cache cheia, LRU removido antes da inserção
CACHE: <k>:<v> ...   → resultado do comando D (ordem MRU → LRU)
STATS: hits=... misses=... evictions=... size=... capacity=...
ERROR CACHE ALREADY CREATED
ERROR CACHE NOT CREATED
ERROR INVALID CAPACITY
```

---

## Exemplo de uso

Entrada:

```
C 3
P 1 10
P 2 20
P 3 30
G 1
P 4 40
D
S
X
```

Saída:

```
INSERTED
INSERTED
INSERTED
HIT 10
EVICTED 2 INSERTED
CACHE: 4:40 1:10 3:30
STATS: hits=1 misses=0 evictions=1 size=3 capacity=3
```

Após o `G 1`, a chave 1 se torna MRU. Quando a chave 4 é inserida com a cache cheia, a chave 2 (LRU) é removida.

---

## Detalhes de implementação

**Tabela hash**
- Tamanho inicial: `max(capacidade × 2 + 1, 17)`, sempre ímpar para reduzir colisões.
- Função de hash: `(unsigned int)key % nbuckets`, garantindo índice positivo para chaves negativas.
- Colisões resolvidas por encadeamento separado (`HashEntry*` em cada balde).

**Lista duplamente ligada**
- Dois nós sentinela (`head` e `tail`) eliminam casos especiais de borda (lista vazia, inserção no início/fim).
- `list_push_front` → insere como MRU em O(1).
- `list_pop_back` → remove o LRU em O(1).
- `list_remove` → desencadeia qualquer nó em O(1), usado no hit (move para MRU) e na eviction.

**Política LRU**
- `GET` com hit: nó é desencadeado e reinserido na frente — passa a ser MRU.
- `PUT` nova chave com cache cheia: nó da cauda (LRU) é removido da lista e da hash antes da inserção.
- `PUT` chave existente: apenas atualiza o valor e move para MRU, sem eviction e sem alocação.

---

## Complexidade

| Operação | Tempo | Espaço extra |
|---|---|---|
| GET | O(1) amortizado | — |
| PUT (inserção) | O(1) amortizado | O(1) por nó |
| PUT (atualização) | O(1) | — |
| Eviction | O(1) | — |
| Dump | O(n) | — |

---

## Arquivos

```
lru.c       — código-fonte completo (único arquivo)
README.md   — este documento
```
