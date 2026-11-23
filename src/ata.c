#include "ata.h"

#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA0       0x1F3
#define ATA_LBA1       0x1F4
#define ATA_LBA2       0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80
#define ATA_LBA28_MAX   0x0FFFFFFF
#define ATA_TIMEOUT     1000000

ata_device_info_t ata_primary_master_info;

static void ata_delay_400ns(void) {
    inb(0x3F6);
    inb(0x3F6);
    inb(0x3F6);
    inb(0x3F6);
}

static int ata_wait_data(void) {
    for (int timeout = 0; timeout < ATA_TIMEOUT; timeout++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF || (status & ATA_STATUS_ERR)) return 0;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) return 1;
    }
    return 0;
}

static int ata_wait_ready(void) {
    for (int timeout = 0; timeout < ATA_TIMEOUT; timeout++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0 || status == 0xFF || (status & ATA_STATUS_ERR)) return 0;
        if (!(status & ATA_STATUS_BSY)) return 1;
    }
    return 0;
}

static void ata_select_lba(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_delay_400ns();
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
}

void ata_init(void) {
    ata_primary_master_info.present = 0;
    ata_primary_master_info.qemu_virtual = 0;
    ata_primary_master_info.sector_count = 0;
    ata_primary_master_info.model[0] = '\0';

    uint16_t identify[256];
    outb(ATA_DRIVE, 0xA0);
    ata_delay_400ns();
    outb(ATA_SECTOR_CNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_COMMAND, 0xEC);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0 || status == 0xFF) return;
    if (!ata_wait_data()) return;

    for (int i = 0; i < 256; i++) identify[i] = inw(ATA_DATA);

    uint32_t sectors = (uint32_t)identify[60] | ((uint32_t)identify[61] << 16);
    if (sectors < 3) return;

    for (int i = 0; i < 20; i++) {
        uint16_t word = identify[27 + i];
        ata_primary_master_info.model[i * 2] = (char)(word >> 8);
        ata_primary_master_info.model[i * 2 + 1] = (char)(word & 0xFF);
    }
    ata_primary_master_info.model[40] = '\0';
    for (int i = 0; i < 37; i++) {
        if (ata_primary_master_info.model[i] == 'Q' &&
            ata_primary_master_info.model[i + 1] == 'E' &&
            ata_primary_master_info.model[i + 2] == 'M' &&
            ata_primary_master_info.model[i + 3] == 'U') {
            ata_primary_master_info.qemu_virtual = 1;
            break;
        }
    }

    ata_primary_master_info.sector_count = sectors;
    ata_primary_master_info.present = 1;
}

int ata_read_sector(uint32_t lba, uint16_t* buffer) {
    if (!buffer || !ata_primary_master_info.present ||
        lba >= ata_primary_master_info.sector_count || lba > ATA_LBA28_MAX) return 0;

    ata_select_lba(lba);
    outb(ATA_COMMAND, 0x20);
    if (!ata_wait_data()) return 0;
    for (int i = 0; i < 256; i++) buffer[i] = inw(ATA_DATA);
    return 1;
}

int ata_write_sector(uint32_t lba, const uint16_t* buffer) {
    if (!buffer || !ata_primary_master_info.present ||
        lba >= ata_primary_master_info.sector_count || lba > ATA_LBA28_MAX) return 0;

    ata_select_lba(lba);
    outb(ATA_COMMAND, 0x30);
    if (!ata_wait_data()) return 0;
    for (int i = 0; i < 256; i++) outw(ATA_DATA, buffer[i]);

    outb(ATA_COMMAND, 0xE7);
    return ata_wait_ready();
}
