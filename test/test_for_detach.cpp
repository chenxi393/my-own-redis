#include <stdio.h>
#include <stdlib.h>
// gpt生成的 还没仔细测试
typedef struct HNode {
    int key;
    struct HNode *next;
} HNode;

typedef struct HTab {
    HNode **tab;
    size_t size;
    size_t mask;
} HTab;

HTab* h_init(size_t capacity) {
    HTab *htab = (HTab*)malloc(sizeof(HTab));
    htab->tab =(HNode**) calloc(capacity, sizeof(HNode *));
    htab->size = 0;
    htab->mask = capacity - 1;
    return htab;
}

void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->key & htab->mask;
    node->next = htab->tab[pos];
    htab->tab[pos] = node;
    htab->size++;
}

HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;
    *from = (*from)->next;
    htab->size--;
    return node;
}

void h_delete(HTab *htab, int key) {
    size_t pos = key & htab->mask;
    HNode **from = &(htab->tab[pos]);

    while (*from) {
        if ((*from)->key == key) {
            HNode *deleted_node = h_detach(htab, from);
            free(deleted_node);
            return;
        }
        from = &(*from)->next;
    }
}

void h_destroy(HTab *htab) {
    for (size_t i = 0; i <= htab->mask; i++) {
        HNode *node = htab->tab[i];
        while (node) {
            HNode *next = node->next;
            free(node);
            node = next;
        }
    }
    free(htab->tab);
    free(htab);
}

int main() {
    HTab *htab = h_init(8);

    HNode *node1 =(HNode*) malloc(sizeof(HNode));
    node1->key = 10;
    node1->next = NULL;

    HNode *node2 = (HNode*)malloc(sizeof(HNode));
    node2->key = 18;
    node2->next = NULL;

    h_insert(htab, node1);
    h_insert(htab, node2);

    int key_to_delete = 18;
    h_delete(htab, key_to_delete);

    h_destroy(htab);

    return 0;
}