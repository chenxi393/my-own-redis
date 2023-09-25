#pragma once

#include <stddef.h>
#include <stdint.h>


struct HeapItem {
    uint64_t val = 0;
    size_t *ref = NULL;  // 这里也是侵入式数据结构 保存了Entry里的地址
};

void heap_update(HeapItem *a, size_t pos, size_t len);
