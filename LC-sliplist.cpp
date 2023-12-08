#include <iostream>
#include <vector>
using namespace std;
#define MAX_LEVEL 32
#define SKIPLIST_P 0.25

// 无span 无双向链表的跳表
// 查找：每层去找

struct Node {
    int val = -1;
    // 32层
    Node* next[MAX_LEVEL] = { nullptr };
};

class Skiplist {
private:
    int currentLevel;
    Node* head;
    // 生成随机层数 1/2返回2 1/4返回3 1/8返回4...
    // 这里概率的计算不太懂
    // 为了插入时的时间复杂度为O(logn)
    int randomLevel()
    {
        int level = 1;
        //
        while ((rand() & 0xFFFF) < (SKIPLIST_P * 0xFFFF) && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

public:
    Skiplist()
    {
        currentLevel = 1;
        // 这里初始化为1层
        // 初始化一个虚拟头节点
        head = new Node();
    }

    bool search(int target)
    {
        Node* p = head;
        for (int i = currentLevel - 1; i >= 0; i--) {
            // 找到所有层的前驱节点
            while (p->next[i] != nullptr && p->next[i]->val < target) {
                /* code */
                p = p->next[i];
            }
            // 如果找到了
            if (p->next[i] != nullptr && p->next[i]->val == target) {
                return true;
            }
        }
        return false;
    }

    void add(int num)
    {
        // 存放每层需要更新的节点 初始为是头节点
        vector<Node*> update(MAX_LEVEL, head);
        // 从最高层开始查找
        Node* p = head;
        for (int i = currentLevel - 1; i >= 0; i--) {
            // 找到所有层的前驱节点
            while (p->next[i] != nullptr && p->next[i]->val < num) {
                /* code */
                p = p->next[i];
            }
            // 记录每层的前驱节点
            update[i] = p;
        }
        int randomLevel = this->randomLevel();
        // 如果随机层数大于当前层数
        if (randomLevel > currentLevel) {
            // 更新当前层数
            currentLevel = randomLevel;
        }
        // 新建一个节点
        Node* newNode = new Node();
        newNode->val = num;
        // 从最高层开始插入
        for (int i = randomLevel - 1; i >= 0; i--) {
            // 更新每层的指针
            newNode->next[i] = update[i]->next[i];
            update[i]->next[i] = newNode;
        }
    }

    bool erase(int num)
    {
        // 存放每层需要更新的节点 假设是头节点
        vector<Node*> update(MAX_LEVEL, head);
        // 从最高层开始查找
        Node* p = head;
        for (int i = currentLevel - 1; i >= 0; i--) {
            // 找到所有层的前驱节点
            while (p->next[i] != nullptr && p->next[i]->val < num) {
                /* code */
                p = p->next[i];
            }
            // 记录每层的前驱节点
            update[i] = p;
        }
        // 如果找到了
        if (p->next[0] != nullptr && p->next[0]->val == num) {
            // 从最高层开始删除
            for (int i = currentLevel - 1; i >= 0; i--) {
                // 如果当前层的节点是要删除的节点
                if (update[i]->next[i] != nullptr && update[i]->next[i]->val == num) {
                    // 更新指针
                    update[i]->next[i] = update[i]->next[i]->next[i];
                }
                // 如果当前层的节点是头节点 并且当前层已经没有节点了
                if (head->next[i] == nullptr) {
                    // 更新当前层数
                    currentLevel--;
                }
            }
            return true;
        }
        return false;
    }
};

int main()
{
    Skiplist* obj = new Skiplist();
    obj->add(1);
    obj->add(2);
    printf("%d\n", obj->search(0));
    printf("%d\n", obj->search(1));
    obj->add(3);
    printf("%d\n", obj->erase(0));
    obj->add(4);
}
/**
 * Your Skiplist object will be instantiated and called as such:
 * Skiplist* obj = new Skiplist();
 * bool param_1 = obj->search(target);
 * obj->add(num);
 * bool param_3 = obj->erase(num);
 */