#include <cstddef>
#include <iostream>
#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) ); })

//实测去掉typeof 那一行也是可以的 

struct HNode {
    HNode* next = NULL;
    uint64_t hcode = 0;
};

struct Entry {
    struct HNode node;
    std::string key;
    std::string val;
};

int main()
{

    Entry* s = new Entry();
    s->val = "fwebd";
    HNode* tt = &s->node;
    Entry* ee = container_of(tt, Entry, node);
    std::cout << ee->val << std::endl;

    //下面这里直接计算也是可以的
    Entry* direct = (Entry*) ((char*)tt-offsetof(Entry,node));
    std::cout << direct->val << std::endl;
    return 0;
}