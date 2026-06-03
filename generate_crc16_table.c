#include <stdio.h>
#include <stdint.h>

/* Generate CRC16-CCITT-FALSE table (poly 0x1021, init 0xFFFF) */
int main(void) {
    uint16_t table[256];

    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0);
        }
        table[i] = crc;
    }

    /* Verify known vector */
    uint16_t crc = 0xFFFF;
    const char *test_str = "123456789";
    for (int i = 0; test_str[i]; i++) {
        uint8_t byte = (uint8_t)test_str[i];
        uint8_t index = (uint8_t)((crc ^ byte) >> 8);
        crc = (crc << 8) ^ table[index];
    }

    printf("Known vector test: crc16('123456789') = 0x%04X (expected 0x29B1)\n", crc);

    if (crc == 0x29B1) {
        printf("✓ Table is correct\n\n");
    } else {
        printf("✗ Table is INCORRECT\n\n");
        return 1;
    }

    /* Output table */
    printf("static const uint16_t g_crc16_table[256] = {\n");
    for (int i = 0; i < 256; i++) {
        if (i % 8 == 0) printf("    ");
        printf("0x%04X", table[i]);
        if (i < 255) printf(", ");
        if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("};\n");

    return 0;
}
