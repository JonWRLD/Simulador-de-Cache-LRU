// lru.c
// Projeto Final - Estrutura de Dados 2026-1
// Simulador de Cache LRU (Hash + Lista Duplamente Ligada)
//
// Compile:
//   gcc -std=c11 -Wall -Wextra -O2 lru.c -o lru
//
// Execute:
//   ./lru < teste_base.txt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ====== CONSTANTS / TYPES ======

typedef struct Node {
    int key;
    int value;
    struct Node* prev;
    struct Node* next;
} Node;

typedef struct HashEntry {
    int key;
    Node* node;               // aponta para o Node real na lista LRU
    struct HashEntry* next;   // encadeamento do balde
} HashEntry;

typedef struct Hash {
    int nbuckets;
    HashEntry** buckets;
} Hash;

typedef struct Cache {
    int capacity;
    int size;

    int hits;
    int misses;
    int evictions;

    Hash* h;

    // Sentinelas: head <-> [MRU ... LRU] <-> tail
    Node head;
    Node tail;
} Cache;

// ====== DOUBLY LINKED LIST (LRU) ======
// Invariante:
// head.next aponta para o MRU (ou tail se vazia)
// tail.prev aponta para o LRU (ou head se vazia)

static void list_init(Cache* c) {
    c->head.prev = NULL;
    c->head.next = &c->tail;
    c->tail.prev = &c->head;
    c->tail.next = NULL;
}

static void list_remove(Node* n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->prev = NULL;
    n->next = NULL;
}

static void list_push_front(Cache* c, Node* n) {
    n->next = c->head.next;
    n->prev = &c->head;
    c->head.next->prev = n;
    c->head.next = n;
}

static Node* list_pop_back(Cache* c) {
    Node* lru = c->tail.prev;
    if (lru == &c->head) return NULL; // lista vazia
    list_remove(lru);
    return lru;
}

// ====== HASH TABLE (SEPARATE CHAINING) ======

static int hash_fn(int key, int nbuckets) {
    // garante indice positivo mesmo para chaves negativas
    return (int)((unsigned int)key % (unsigned int)nbuckets);
}

static Hash* hash_create(int nbuckets) {
    Hash* h = malloc(sizeof(Hash));
    if (!h) return NULL;
    h->nbuckets = nbuckets;
    h->buckets = calloc(nbuckets, sizeof(HashEntry*));
    if (!h->buckets) { free(h); return NULL; }
    return h;
}

static void hash_destroy(Hash* h) {
    if (!h) return;
    for (int i = 0; i < h->nbuckets; i++) {
        HashEntry* e = h->buckets[i];
        while (e) {
            HashEntry* next = e->next;
            free(e);
            e = next;
        }
    }
    free(h->buckets);
    free(h);
}

static Node* hash_get(Hash* h, int key) {
    int idx = hash_fn(key, h->nbuckets);
    HashEntry* e = h->buckets[idx];
    while (e) {
        if (e->key == key) return e->node;
        e = e->next;
    }
    return NULL;
}

// retorna 1 se inseriu nova chave, 0 se atualizou existente
static int hash_put(Hash* h, int key, Node* node) {
    int idx = hash_fn(key, h->nbuckets);
    HashEntry* e = h->buckets[idx];
    while (e) {
        if (e->key == key) {
            e->node = node; // atualiza ponteiro
            return 0;
        }
        e = e->next;
    }
    // nova entrada
    HashEntry* ne = malloc(sizeof(HashEntry));
    if (!ne) return -1;
    ne->key  = key;
    ne->node = node;
    ne->next = h->buckets[idx];
    h->buckets[idx] = ne;
    return 1;
}

// retorna 1 se removeu, 0 se nao existia
static int hash_remove(Hash* h, int key) {
    int idx = hash_fn(key, h->nbuckets);
    HashEntry** pp = &h->buckets[idx];
    while (*pp) {
        if ((*pp)->key == key) {
            HashEntry* dead = *pp;
            *pp = dead->next;
            free(dead);
            return 1;
        }
        pp = &(*pp)->next;
    }
    return 0;
}

// ====== LRU CACHE API ======

static Cache* cache_create(int capacity, int nbuckets) {
    Cache* c = malloc(sizeof(Cache));
    if (!c) return NULL;
    c->capacity  = capacity;
    c->size      = 0;
    c->hits      = 0;
    c->misses    = 0;
    c->evictions = 0;
    c->h = hash_create(nbuckets);
    if (!c->h) { free(c); return NULL; }
    list_init(c);
    return c;
}

static void cache_destroy(Cache* c) {
    if (!c) return;
    // liberar todos os nos da lista
    Node* cur = c->head.next;
    while (cur != &c->tail) {
        Node* next = cur->next;
        free(cur);
        cur = next;
    }
    hash_destroy(c->h);
    free(c);
}

// retorna 1 hit, 0 miss
static int cache_get(Cache* c, int key, int* out_value) {
    Node* n = hash_get(c->h, key);
    if (!n) {
        c->misses++;
        return 0;
    }
    // HIT: move para MRU
    c->hits++;
    *out_value = n->value;
    list_remove(n);
    list_push_front(c, n);
    return 1;
}

// retorna 1 se houve eviction, 0 caso contrario
// evicted_key: chave removida (valido apenas se retornou 1)
static int cache_put(Cache* c, int key, int value, int* evicted_key) {
    Node* existing = hash_get(c->h, key);
    if (existing) {
        // UPDATE: atualiza valor e move para MRU
        existing->value = value;
        list_remove(existing);
        list_push_front(c, existing);
        return 0; // sem eviction, sem novo no
    }

    int evicted = 0;
    if (c->size == c->capacity) {
        // EVICTION: remove LRU
        Node* lru = list_pop_back(c);
        *evicted_key = lru->key;
        hash_remove(c->h, lru->key);
        free(lru);
        c->size--;
        c->evictions++;
        evicted = 1;
    }

    // INSERT: cria novo no e insere como MRU
    Node* n = malloc(sizeof(Node));
    if (!n) return -1;
    n->key   = key;
    n->value = value;
    n->prev  = NULL;
    n->next  = NULL;
    list_push_front(c, n);
    hash_put(c->h, key, n);
    c->size++;
    return evicted;
}

static void cache_dump(Cache* c) {
    printf("CACHE:");
    Node* cur = c->head.next;
    while (cur != &c->tail) {
        printf(" %d:%d", cur->key, cur->value);
        cur = cur->next;
    }
    printf("\n");
}

static void cache_stats(Cache* c) {
    printf("STATS: hits=%d misses=%d evictions=%d size=%d capacity=%d\n",
           c->hits, c->misses, c->evictions, c->size, c->capacity);
}

// ====== COMMAND PARSER / MAIN ======

static char* ltrim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

int main(void) {
    Cache* cache = NULL;

    char line[256];
    while (fgets(line, sizeof(line), stdin)) {
        char* s = ltrim(line);
        if (*s == '\0' || *s == '\n' || *s == '#') continue;

        char cmd = s[0];

        if (cmd == 'X') {
            break;
        }

        if (cmd == 'C') {
            if (cache) {
                printf("ERROR CACHE ALREADY CREATED\n");
                continue;
            }
            int cap = 0;
            if (sscanf(s + 1, "%d", &cap) != 1 || cap <= 0) {
                printf("ERROR INVALID CAPACITY\n");
                break; // encerra o programa
            }
            int nbuckets = cap * 2 + 1;
            if (nbuckets < 17) nbuckets = 17;
            cache = cache_create(cap, nbuckets);
            continue;
        }

        if (!cache) {
            printf("ERROR CACHE NOT CREATED\n");
            continue;
        }

        if (cmd == 'G') {
            int key = 0;
            if (sscanf(s + 1, "%d", &key) != 1) continue;
            int value = 0;
            if (cache_get(cache, key, &value)) {
                printf("HIT %d\n", value);
            } else {
                printf("MISS\n");
            }
            continue;
        }

        if (cmd == 'P') {
            int key = 0, value = 0;
            if (sscanf(s + 1, "%d %d", &key, &value) != 2) continue;

            // verificar se e atualizacao antes de chamar put
            int is_update = (hash_get(cache->h, key) != NULL);

            int evicted_key = 0;
            int evicted = cache_put(cache, key, value, &evicted_key);

            if (evicted) {
                printf("EVICTED %d INSERTED\n", evicted_key);
            } else if (is_update) {
                printf("UPDATED\n");
            } else {
                printf("INSERTED\n");
            }
            continue;
        }

        if (cmd == 'D') {
            cache_dump(cache);
            continue;
        }

        if (cmd == 'S') {
            cache_stats(cache);
            continue;
        }

        // comando desconhecido: ignorar
    }

    if (cache) {
        cache_destroy(cache);
    }
    return 0;
}