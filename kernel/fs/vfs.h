#ifndef VFS_H
#define VFS_H

#include <stdint.h>

typedef enum {
    VFS_NONE   = 0,
    VFS_RAMFS  = 1,
    VFS_FAT16  = 2,
} vfs_backend_t;

typedef struct {
    uint8_t       used;
    vfs_backend_t backend;
    uint32_t      first;      /* ramfs: byte offset; fat16: first cluster */
    uint32_t      size;
    uint32_t      pos;
    char          name[32];   /* nome original — necessário para escrita FAT16 */
} vfs_fd_t;

/* Abre arquivo pelo nome. Tenta ramfs primeiro, depois FAT16.
   Preenche *fd e retorna 0 em sucesso, -1 se não encontrado.  */
int  vfs_open (const char *name, vfs_fd_t *fd);

/* Abre o arquivo se já existir (ramfs ou FAT16); senão cria uma entrada
   vazia no FAT16 e abre. Retorna 0 em sucesso, -1 se disco indisponível
   ou sem espaço no root dir. */
int  vfs_create(const char *name, vfs_fd_t *fd);

/* Lê até len bytes a partir de fd->pos. Retorna bytes lidos ou -1. */
int  vfs_read (vfs_fd_t *fd, char *buf, uint32_t len);

/* Fecha o fd (marca como não usado). */
void vfs_close(vfs_fd_t *fd);

/* Escreve len bytes de buf no arquivo referenciado por fd (só FAT16).
   Retorna 0 em sucesso, -1 se backend não suporta escrita ou em erro. */
int  vfs_write(vfs_fd_t *fd, const char *buf, uint32_t len);

#endif
