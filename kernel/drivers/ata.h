#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Returns 1 if a disk was detected, 0 otherwise. */
int  ata_init(void);

/* Reads/writes a 512-byte sector at the given LBA.
   Returns 0 on success, -1 on error.                 */
int  ata_read_sector (uint32_t lba, void *buf);
int  ata_write_sector(uint32_t lba, const void *buf);

#endif
