#include <atomic>
#include <thread>

template <typename T>
class LockFreeStack {

public:
    LockFreeStack() {
    }

    ~LockFreeStack() {
        Node* current_head = head.load();
        while (current_head != nullptr) {
            Node* removable = current_head;
            current_head = current_head->child.load();
            delete removable;
        }
        current_head = trash_head.load();
        while (current_head != nullptr) {
            Node* removable = current_head;
            current_head = current_head->child.load();
            delete removable;
        }
    }

    void Push(T element) {
        Node* current_head = head.load();
        Node* new_head = new Node(element);
        new_head->child.store(current_head);
        while (!head.compare_exchange_strong(current_head, new_head)) {
            new_head->child.store(current_head);
        }
    }

    bool Pop(T& element) {
        Node* current_head = head.load();
        while (current_head != nullptr) {
            if (head.compare_exchange_strong(current_head, current_head->child.load())) {
                element = current_head->element;
                Node* current_trash_head = trash_head.load();
                current_head->child.store(current_trash_head);
                while (!trash_head.compare_exchange_strong(current_trash_head, current_head)) {
                    current_head->child.store(current_trash_head);
                }
                return true;
            }
        }
        return false;
    }

private:
    struct Node {
        T element;
        std::atomic<Node*> child;

        Node(T element) {
            this->element = element;
            this->child = nullptr;
        }
    };

    std::atomic<Node*> head {nullptr};
    std::atomic<Node*> trash_head {nullptr};
};

template <typename T>
using ConcurrentStack = LockFreeStack<T>;