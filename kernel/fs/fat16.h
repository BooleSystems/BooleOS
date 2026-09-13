#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

/* Lê BPB do setor 0, cacheia a FAT na heap.
   Retorna 1 se disco presente e FAT16 válido, 0 caso contrário. */
int fat16_init(void);

/* Busca arquivo pelo nome no root directory (case-insensitive, suporta "a.b").
   Preenche *first_cluster e *size. Retorna 1 se encontrado, 0 se não
   existe, -1 se um erro de I/O impediu confirmar (chamador NÃO deve
   tratar -1 como "não existe", ou risca criar entradas duplicadas). */
int fat16_find(const char *name, uint32_t *first_cluster, uint32_t *size);

/* Lê len bytes do arquivo a partir do byte offset pos na cadeia de clusters.
   Retorna bytes lidos, ou -1 em erro.                                        */
int fat16_read_at(uint32_t first_cluster, uint32_t pos,
                  char *buf, uint32_t len, uint32_t file_size);

/* Retorna o próximo cluster na cadeia (>= 0xFFF8 = fim). */
uint32_t fat16_next_cluster(uint32_t cluster);

/* Tamanho de um cluster em bytes. */
uint32_t fat16_bytes_per_cluster(void);

/* 1 se o driver foi inicializado com sucesso. */
int fat16_available(void);

/* Cria uma entrada vazia (size=0) para name no root dir, se ainda não existir.
   Idempotente: se o arquivo já existe, retorna 0 sem alterar nada.
   Retorna 0 em sucesso, -1 se o root directory está cheio ou disco indisponível. */
int fat16_create(const char *name);

/* Escreve buf (len bytes) no arquivo name (deve existir no root dir).
   Libera cadeia antiga, aloca nova, escreve dados, atualiza FAT e dir entry.
   Retorna 0 em sucesso, -1 em erro.                                          */
int fat16_write_file(const char *name, const char *buf, uint32_t len);

/* Itera entradas válidas do root directory (0-based).
   Preenche name (até 12 chars + '\0') e *size.
   Retorna 1 se encontrou entry no índice idx, 0 se acabou. */
int fat16_readdir(uint32_t idx, char name[13], uint32_t *size);

#endif
