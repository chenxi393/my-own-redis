#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// 这个语法和代码规范感觉及其别扭（掺杂C和C++） UIUC的代码风格会好很多
// 自己写的 还没写完 感觉面试可能让实现一个哈希表 所以自己试试
struct Hnode {
    Hnode* next = NULL;
    size_t hcode = 0; // data 域
};

struct HashTable { // 不严格搞c++ 面向对象那一套 随便点
    Hnode** tab = NULL;
    size_t mask = 0; // 类似于要模的数 也是表最大数
    size_t size = 0;

    HashTable(size_t num) // 构造函数 很久很久没写cpp了
    {
        assert(num > 0 && num & (num - 1) == 0);
        mask = num - 1;
        size = 0; // 为什么上面初始化为1 了 还要
        tab = (Hnode**)calloc(num, sizeof(Hnode*));
    }
    void insert(Hnode* node) // 给的节点显然以及分配内存
    {
        size_t pos = node->hcode & mask;
        node->next = tab[pos];
        tab[pos] = node;
        ++size;
    }
    Hnode** find(size_t key) // 文章的意思是 返回的是指针的指针 便于删去这个指针
    {
        size_t pos = key & mask;
        Hnode** head = &tab[pos]; // 这里就是操作原来分配的内存
        while (*head) {
            if ((*head)->hcode == key) {
                return head;
            }
            head = &(*head)->next; // 这一块指针的指针操作 很绕很绕
                                   // 有机会可以检查后续delete有没有内存泄漏 或者段错误
        }
    }

    Hnode* detach(Hnode** from)
    {
        Hnode* node = *from;
        *from = (*from)->next;
        size--;
        return node;
    }
};

int main()
{
    HashTable tab1(8);
}