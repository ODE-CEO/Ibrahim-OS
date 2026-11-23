#include "ata.h"
#include "ibfs_disk.h"

static uint16_t sector[ATA_SECTOR_SIZE / 2];
static const char readme_contents[] = "IbrahimFS-512 hazir. Tek dosya alani: 512 bayt.";

static void clear_sector(void) {
    for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) sector[i] = 0;
}

static void write_u16(uint8_t* bytes, int offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t* bytes, int offset, uint32_t value) {
    write_u16(bytes, offset, (uint16_t)value);
    write_u16(bytes, offset + 2, (uint16_t)(value >> 16));
}

int ibfs_disk_format(void) {
    if (!ata_primary_master_info.present ||
        !ata_primary_master_info.qemu_virtual ||
        ata_primary_master_info.sector_count < 3) return 0;

    uint8_t* bytes = (uint8_t*)sector;
    clear_sector();
    bytes[0] = 'I';
    bytes[1] = 'B';
    bytes[2] = 'F';
    bytes[3] = 'S';
    write_u16(bytes, 4, 1);
    write_u16(bytes, 6, ATA_SECTOR_SIZE);
    write_u32(bytes, 8, ata_primary_master_info.sector_count);
    write_u32(bytes, 12, 1);
    write_u32(bytes, 16, 2);
    write_u16(bytes, 20, ATA_SECTOR_SIZE);
    bytes[22] = 'I';
    bytes[23] = 'B';
    bytes[24] = 'R';
    bytes[25] = 'A';
    bytes[26] = 'H';
    bytes[27] = 'I';
    bytes[28] = 'M';
    bytes[29] = 'F';
    bytes[30] = 'S';
    bytes[31] = '-';
    bytes[32] = '5';
    bytes[33] = '1';
    bytes[34] = '2';

    uint16_t superblock[ATA_SECTOR_SIZE / 2];
    for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) superblock[i] = sector[i];

    clear_sector();
    bytes = (uint8_t*)sector;
    bytes[0] = 'F';
    bytes[1] = 'I';
    bytes[2] = 'L';
    bytes[3] = 'E';
    const char name[] = "README.TXT";
    for (int i = 0; name[i]; i++) bytes[4 + i] = (uint8_t)name[i];
    write_u32(bytes, 36, sizeof(readme_contents) - 1);
    write_u32(bytes, 40, 2);

    uint16_t directory[ATA_SECTOR_SIZE / 2];
    for (int i = 0; i < ATA_SECTOR_SIZE / 2; i++) directory[i] = sector[i];

    clear_sector();
    bytes = (uint8_t*)sector;
    for (int i = 0; readme_contents[i]; i++) bytes[i] = (uint8_t)readme_contents[i];

    if (!ata_write_sector(2, sector)) return 0;
    if (!ata_write_sector(1, directory)) return 0;
    if (!ata_write_sector(0, superblock)) return 0;
    return 1;
}
