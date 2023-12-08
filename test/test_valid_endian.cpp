#include <stdio.h>
#include <string.h>

// 用来验证小端还是大端
// 小端是低位在低位 大端是高位在前（低位）
void printByteOrder(const unsigned int value)
{
    unsigned char* bytes = (unsigned char*)&value;
    // memcpy(&len, &rbuf[0], 4); 或者用这个 对应了06里的

    printf("Value: 0x%08X\n", value);
    printf("Byte Order: ");

    for (int i = 0; i < sizeof(unsigned int); ++i) {
        printf("%02X ", bytes[i]);
    }

    printf("\n");
}

void Test()
{
    // 0x34 0x31
    __uint8_t rbuf[] = { 52, 49, 0, 0, 0x51, 0x10, 0x00, 0x00 };
    __uint32_t len;

    // 使用memcpy将rbuf中的前4个字节复制到len中
    memcpy(&len, &rbuf[0], 4);

    // 打印结果
    printf("len: %u\n", len);
    printf("%x\n", len);
    printf("%d\n", 0x3431);
    printf("%d\n", 0x3134);
}

int main()
{
    unsigned int value = 0x12345678;
    printByteOrder(value);

    Test();
    return 0;
}
