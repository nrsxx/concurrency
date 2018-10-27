#include <iostream>
#include <vector>
#include <atomic>
#include <array>
#include <thread>


class TreeMutex {
public:
    TreeMutex(std::size_t n_threads) {
        std::size_t degree = 1;
        //degree - ближайшая степень двойки
        while (degree < n_threads) {
            degree *= 2;
        }
        //размер дерева равен 2 * degree - 1, т. к.
        // degree - число листьев
        size_of_tree = 2 * degree - 1;
        //в каждом узле дерева храним peterson_mutex
        tree = std::vector<peterson_mutex>(size_of_tree);
    }

    void lock(std::size_t current_thread) {
        //номер листа, из которого стартует поток current_thread
        std::size_t cur_position = size_of_tree / 2 + current_thread / 2;
        std::size_t degree = 2;
        //последовательно вызываем lock в каждом мьютексе на пути до корня
        while (true) {
            //проверяем, какой номер имеет поток в текущем мьютексе(0 или 1)
            if (current_thread % degree < degree / 2) {
                tree[cur_position].lock(0);
            } else {
                tree[cur_position].lock(1);
            }
            degree *= 2;
            //если сделали lock в корне - останавливаемся
            if (cur_position == 0) {
                break;
            }
            //переходим в вершину-предка
            cur_position = (cur_position - 1) / 2;
        }
    }

    void unlock(std::size_t current_thread) {
        //стартуем из корня
        std::size_t cur_position = 0;
        //local - текущая "середина", сравниваем с ней номер потока,
        //чтобы понять, из правого или левого поддерева он шел во
        //время исполнения lock()
        std::size_t local = (size_of_tree + 1) / 2;
        int degree = local;
        while (degree > 0) {
            if (current_thread >= local) {
                tree[cur_position].unlock(1);
                //переход в правого потомка
                cur_position = cur_position * 2 + 2;
                //новая середина
                local += degree / 2;
            } else {
                tree[cur_position].unlock(0);
                //переход в левого потомка
                cur_position = 2 * cur_position + 1;
                //новая середина
                local -= degree / 2;
            }
            degree /= 2;
        }
        //последний unlock() происходит при degree = 1, т. е. в листе
    }

private:
    std::size_t size_of_tree;

    class peterson_mutex {
    public:
        peterson_mutex() {
            want[0].store(false);
            want[1].store(false);
            victim.store(0);
        }

        void lock(std::size_t t) {
            want[t].store(true);
            victim.store(t);
            while (want[1 - t].load() && victim.load() == t) {
                std::this_thread::yield();
            }
        }

        void unlock(std::size_t t) {
            want[t].store(false);
        }

    private:
        std::array<std::atomic<bool>, 2> want;
        std::atomic<std::size_t> victim;
    };

    std::vector<peterson_mutex> tree;
};