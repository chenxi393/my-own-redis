#include <cassert>
#include <cstdlib>
#include <iostream>
#define ZSKIPLIST_MAXLEVEL 32
#define ZSKIPLIST_P 0.25
// redis有自己的 内存分配与回收方案 这里同意改造层malloc 和 free
// https://darjun.github.io/2018/05/24/redis-skiplist/
// 源码阅读连接  还是先看看简单的实现 再看这个
struct zskiplistNode {
    // redis中通用对象，用来保存节点数据
    // zset没有用模板实现 所以需要通用对象
    std::string* obj; // 这里改为string
    // 跳跃表只保存指向 member 和 score 的指针

    // 分数
    double score;
    // 后退指针，只有第0层有效
    // 逆序操作会用到
    zskiplistNode* backward;
    // 各层的前进指针及与下一个节点的间隔
    struct zskiplistLevel {
        zskiplistNode* forward;
        unsigned int span; // 跨度实际上是用来计算排位的
    } level[];
    // 文中说 层高是概概率生成的 为什么？？

    // 实际上 结点数量就是元素数量+1 有头结点
    // 每个结点可能会有很多层
    // 而头结点默认32层
};

struct zskiplist {
    // 表头节点和表尾节点
    zskiplistNode *header, *tail;
    // 表中节点的数量
    unsigned long length;
    // 表中层数最大的节点的层数（不包括header结点）
    int level;
};

// 创建跳跃表
// 创建一个层高为level的跳跃表节点，以score为分数，obj为数据
zskiplistNode* zslCreateNode(int level, double score, std::string* obj)
{
    // zmalloc 是redis自己实现的内存分配策略 基于系统调用 这里使用malloc替换
    // zskiplistNode* zn = zmalloc(sizeof(*zn) + level * sizeof(struct zskiplistLevel));
    zskiplistNode* zn = (zskiplistNode*)malloc(sizeof(zskiplistNode) + level * sizeof(zskiplistNode::zskiplistLevel));
    zn->score = score;
    zn->obj = obj;
    return zn;
}

zskiplist* zslCreate(void)
{
    int j;
    zskiplist* zsl;

    // 分配空间
    zsl = (zskiplist*)malloc(sizeof(zskiplist));
    // 层高初始化为1，插入数据时可能会增加
    zsl->level = 1;
    // 长度为0
    zsl->length = 0;
    // 分配头部节点，层高ZSKIPLIST_MAXLEVEL（当前值为32，对于2^32个元素足够了）
    zsl->header = zslCreateNode(ZSKIPLIST_MAXLEVEL, 0, NULL);
    // 初始化头部各层的指针和间隔
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    zsl->header->backward = NULL;
    zsl->tail = NULL;
    return zsl;
}

// 销毁跳跃表
void zslFreeNode(zskiplistNode* node)
{
    // obj是redis对象，使用引用计数管理的
    // decrRefCount(node->obj);
    free(node);
}

void zslFree(zskiplist* zsl)
{
    // 跟踪第0层（最下面一层）的forward指针可以遍历所有元素
    zskiplistNode *node = zsl->header->level[0].forward, *next;

    // 释放头部节点
    free(zsl->header);
    while (node) {
        next = node->level[0].forward;
        // 释放当前节点
        zslFreeNode(node);
        node = next;
    }
    // 释放zskiplist结构
    free(zsl);
}

int zslRandomLevel(void)
{
    int level = 1;
    // level每次循环有1/4概率增加层高
    // 按位与 保证在16位整数之间
    while ((random() & 0xFFFF) < (ZSKIPLIST_P * 0xFFFF))
        level += 1;
    return (level < ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

int compareStringObjects(std::string* s1, std::string* s2)
{
    if (*s1 < *s2) {
        return -1;
    } else if (*s1 > *s2) {
        return 1;
    }
    return 0;
}

zskiplistNode* zslInsert(zskiplist* zsl, double score, std::string* obj)
{
    // update记录的是各层新节点插入位置的前一个节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned int rank[ZSKIPLIST_MAXLEVEL];
    int i, level;

    // 分数不能是NaN
    // assert(!isnan(score));

    x = zsl->header;
    // 从最高层向下处理，每次遇到使score和obj介于其中的x和x->forward，则新节点必定在x和x->forward之间
    // 到下一层更精准的定位插入位置。下一层继续从位置x前进查找，因为由上一层得知新节点必然在x之后。
    // 记录节点x在跳跃表中的rank。
    for (i = zsl->level - 1; i >= 0; i--) {
        // x节点的rank由上一层计算得到
        rank[i] = i == (zsl->level - 1) ? 0 : rank[i + 1];
        // score相同时，比较两个obj
        while (x->level[i].forward && (x->level[i].forward->score < score || (x->level[i].forward->score == score && compareStringObjects(x->level[i].forward->obj, obj) < 0))) {
            // 计算rank
            rank[i] += x->level[i].span;
            // 移向下一个继续判断
            x = x->level[i].forward;
        }
        // 新节点要插入x与x->level[i]->forward之间，x->level[i]需要做相应调整（span，forward等字段）
        update[i] = x;
    }

    // 因为允许重复score，所以这里并没有做重复判断。应该由调用者测试新插入的对象是否已经存在。

    // 随机新节点层高
    level = zslRandomLevel();
    if (level > zsl->level) {
        // 新节点比当前层高大
        for (i = zsl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = zsl->header;
            // 没有节点，span是整个跳跃表的长度
            update[i]->level[i].span = zsl->length;
        }
    }
    // 创建跳跃表节点
    x = zslCreateNode(level, score, obj);
    for (i = 0; i < level; i++) {
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;
        // 更新span
        // (rank[0] - rank[i])是第i层中新节点与前一个节点的距离（严格说应该是第0层新节点的前一个节点与第i层新节点的前一个节点间距离）
        // 新节点将update[i]后面的间隔分成两部分。
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        // +1算上新节点
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    // 未更新的层需要增加新节点之前的节点span，因为后面多了一个新节点
    for (i = level; i < zsl->level; i++) {
        update[i]->level[i].span++;
    }

    // 更新后退节点
    x->backward = (update[0] == zsl->header) ? NULL : update[0];
    if (x->level[0].forward)
        x->level[0].forward->backward = x;
    else
        zsl->tail = x;
    zsl->length++;
    return x;
}

// 删除节点
void zslDeleteNode(zskiplist* zsl, zskiplistNode* x, zskiplistNode** update)
{
    int i;
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            // 待删除的节点在该层存在，调整forward指针和span
            // 两个间隔合并为一个，同时减少了一个节点
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            // 该层没有这个节点，只需要减少span
            update[i]->level[i].span -= 1;
        }
    }
    if (x->level[0].forward) {
        // 更新前进节点的后退指针
        x->level[0].forward->backward = x->backward;
    } else {
        // x是跳跃表尾部
        zsl->tail = x->backward;
    }
    // 如果某一层只有该节点，该节点删除后已经没有节点了，直接移除这一层。但是跳跃表至少有一层。
    while (zsl->level > 1 && zsl->header->level[zsl->level - 1].forward == NULL) {
        zsl->level--;
    }
    zsl->length--;
}

int zslDelete(zskiplist* zsl, double score, std::string* obj)
{
    // 各层需要修改的节点
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    x = zsl->header;
    // 与zslInsert操作一致
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (x->level[i].forward->score < score || (x->level[i].forward->score == score && compareStringObjects(x->level[i].forward->obj, obj) < 0)))
            x = x->level[i].forward;
        update[i] = x;
    }

    // 可能有多个节点拥有相同score，所以这里需要比较对象
    x = x->level[0].forward;
    if (x && x->score == score && !compareStringObjects(x->obj, obj)) {
        // 从跳跃表中删除该节点
        zslDeleteNode(zsl, x, update);
        // 释放节点
        zslFreeNode(x);
        return 1;
    }
    // 没找到
    return 0;
}

// 测试函数
void testZSkipList()
{
    // 创建一个跳跃表
    zskiplist* zsl = new zskiplist;
    zsl->header = zslCreateNode(32, 0, nullptr);
    zsl->tail = nullptr;
    zsl->length = 0;
    zsl->level = 1;

    // 插入节点
    std::string* obj0 = new std::string("David");
    zskiplistNode* node0 = zslInsert(zsl, 78.0, obj0);

    std::string* obj1 = new std::string("Charles");
    zskiplistNode* node1 = zslInsert(zsl, 65.5, obj1);

    std::string* obj2 = new std::string("Alice");
    zskiplistNode* node2 = zslInsert(zsl, 87.5, obj2);

    std::string* obj3 = new std::string("Fred");
    zskiplistNode* node3 = zslInsert(zsl, 87.5, obj3);

    std::string* obj4 = new std::string("Bob");
    zskiplistNode* node4 = zslInsert(zsl, 89, obj4);

    std::string* obj5 = new std::string("Emily");
    zskiplistNode* node5 = zslInsert(zsl, 93.5, obj5);
    // 删除节点
    int deleteResult = zslDelete(zsl, 87.5, obj2);
    if (deleteResult)
        std::cout << "Node with score 3.2 and object 'Object 2' deleted." << std::endl;
    else
        std::cout << "Node not found for deletion." << std::endl;

    // 释放跳跃表内存
    zslFree(zsl);
}

int main()
{
    testZSkipList();
    return 0;
}