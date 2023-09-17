#include <stdio.h>
#include <stdlib.h>

int main() {
    int n = 5;
    int *arr1, *arr2;

    // 使用 calloc 分配并初始化内存块
    arr1 = (int *)calloc(n, sizeof(int));
    printf("calloc:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr1[i]);
    }
    printf("\n");

    // 使用 malloc 分配内存块，需要手动初始化
    arr2 = (int *)malloc(n * sizeof(int));
    printf("malloc:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", arr2[i]); //linux会发现都是0 但是mingw的gcc 就不一定
    }
    printf("\n");

    // 释放内存
    free(arr1);
    free(arr2);
    return 0;
}
/*
1. 参数不同： calloc 函数接受两个参数，分别是需要分配的元素数量和每个元素的大小；
而 malloc 函数只接受一个参数，即需要分配的内存块的大小（以字节为单位）。
2. 初始化： calloc 在分配内存块时会将其初始化为零，而 malloc 不会进行初始化。
分配的内存块中的内容在使用前需要手动进行初始化操作。
3. 返回值： calloc 和 malloc 的返回值都是指向新分配内存块的指针。
如果分配失败，两个函数都返回 NULL。
4. 性能： 由于 calloc 在分配内存块时需要进行初始化操作，可能会导致性能稍低于 malloc。
如果不需要初始化内存块，使用 malloc 可能更高效。
*/