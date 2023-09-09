#include <stdio.h>

// 用来验证小端还是大端
void printByteOrder(const unsigned int value) {
    unsigned char* bytes = (unsigned char*)&value;

    printf("Value: 0x%08X\n", value);
    printf("Byte Order: ");

    for (int i = 0; i < sizeof(unsigned int); ++i) {
        printf("%02X ", bytes[i]);
    }

    printf("\n");
}

int main() {
    unsigned int value = 0x12345678;
    printByteOrder(value);

    return 0;
}