#pragma once

#include "avl.h"
#include "hashtable.h"


struct ZSet {
    AVLNode *tree = NULL;
    HMap hmap;
};

// ⭐⭐这里体现了侵入式数据结构的优点
// 使用两种数据结构 不必把数据分别存入两种数据结果中
// 优点：通用性 可以随意更换数据结构 不用改造原来的代码
//       减少内存管理 多个数据结构 只需要delete一次
struct ZNode {
    AVLNode tree;
    HNode hmap;
    double score = 0;
    size_t len = 0;
    char name[0]; //使用柔性数据 放在最后 希望节省开销
};

bool zset_add(ZSet *zset, const char *name, size_t len, double score);
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);
ZNode *zset_pop(ZSet *zset, const char *name, size_t len);
ZNode *zset_query(
    ZSet *zset, double score, const char *name, size_t len, int64_t offset
);
void zset_dispose(ZSet *zset);
void znode_del(ZNode *node);
