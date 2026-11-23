#ifndef ATA_H
#define ATA_H

#include "os.h"

#define ATA_SECTOR_SIZE 512

typedef struct {
    int present;
    int qemu_virtual;
    uint32_t sector_count;
    char model[41];
} ata_device_info_t;

extern ata_device_info_t ata_primary_master_info;

void ata_init(void);
int ata_read_sector(uint32_t lba, uint16_t* buffer);
int ata_write_sector(uint32_t lba, const uint16_t* buffer);

#endif
