#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Retorna 1 se um disco foi detectado, 0 caso contrário. */
int  ata_init(void);

/* Lê/escreve um setor de 512 bytes no LBA indicado.
   Retorna 0 em sucesso, -1 em erro.                  */
int  ata_read_sector (uint32_t lba, void *buf);
int  ata_write_sector(uint32_t lba, const void *buf);

#endif
