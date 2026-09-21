#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* Returns 1 if a disk was detected, 0 otherwise. */
int  ata_init(void);

/* Reads/writes a 512-byte sector at the given LBA.
   Returns 0 on success, -1 on error.                 */
int  ata_read_sector (uint32_t lba, void *buf);
int  ata_write_sector(uint32_t lba, const void *buf);

/* Polling-only, interrupt-independent sector I/O for the CRASH path
   (crashdump.c): they touch no lock, no scheduler, no IRQ machinery and no heap,
   and every wait is bounded by a spin count, so they work inside an exception
   handler (interrupts off, scheduler unusable, controller possibly left in the
   middle of a command). 0 on success, -1 on error/timeout. */
int  ata_crash_read_sector (uint32_t lba, void *buf);
int  ata_crash_write_sector(uint32_t lba, const void *buf);

#endif
