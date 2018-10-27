#include <algorithm>
#include <atomic>
#include <forward_list>
#include <mutex>
#include <vector>

template <typename T>
class StripedHashSet {

public:
    explicit StripedHashSet(const size_t concurrency_level,
                            const size_t growth_factor = 3,
                            const double load_factor = 0.75) {
        number_of_mutexes = concurrency_level;
        this->growth_factor = growth_factor;
        this->load_factor = load_factor;
        mutexes = std::vector<std::mutex>(concurrency_level);
        hash_set = std::vector<std::forward_list<T>>(concurrency_level);
        //важно, чтобы начальный размер таблицы делился на количество страйпов,
        //иначе нарушится инвариантность
        size_of_table.store(concurrency_level);
        size_of_set.store(0);
    }

    bool Insert(const T& element) {
        size_t hash_value = hash(element);
        //т. к. для каждого элемента номер страйпа инвариантен,
        //в процессе перехеширования hash_value не поменяет свое значение
        //и мы залочим нужный мьютекс
        size_t current_mutex = GetStripeIndex(hash_value);
        mutexes[current_mutex].lock();
        size_t current_bucket = GetBucketIndex(hash_value);
        //если element уже содержится в множестве
        if (std::find(hash_set[current_bucket].begin(), hash_set[current_bucket].end(), element)
            != hash_set[current_bucket].end()) {
            mutexes[current_mutex].unlock();
            return false;
        }
        hash_set[current_bucket].push_front(element);
        size_of_set.fetch_add(1);
        if ((size_of_set.load() * 1.0) / size_of_table.load() > load_factor) {
            //отпускаем текущий мьютекс, иначе может возникнуть взаимная блокировка
            mutexes[current_mutex].unlock();
            //расширяем таблицу
            expansion();
            return true;
        }
        mutexes[current_mutex].unlock();
        return true;
    }

    bool Remove(const T& element) {
        size_t hash_value = hash(element);
        size_t current_mutex = GetStripeIndex(hash_value);
        mutexes[current_mutex].lock();
        size_t current_bucket = GetBucketIndex(hash_value);
        if (std::find(hash_set[current_bucket].begin(), hash_set[current_bucket].end(), element)
            == hash_set[current_bucket].end()) {
            mutexes[current_mutex].unlock();
            return false;
        }
        hash_set[current_bucket].remove(element);
        size_of_set.fetch_sub(1);
        mutexes[current_mutex].unlock();
        return true;
    }

    bool Contains(const T& element) {
        size_t hash_value = hash(element);
        size_t current_mutex = GetStripeIndex(hash_value);
        mutexes[current_mutex].lock();
        size_t current_bucket = GetBucketIndex(hash_value);
        if (std::find(hash_set[current_bucket].begin(), hash_set[current_bucket].end(), element)
            == hash_set[current_bucket].end()) {
            mutexes[current_mutex].unlock();
            return false;
        }
        mutexes[current_mutex].unlock();
        return true;
    }

    size_t Size() {
        return size_of_set.load();
    }

private:
    std::hash<T> hash;
    size_t number_of_mutexes;
    size_t growth_factor;
    std::atomic<size_t> size_of_table;
    std::atomic<size_t> size_of_set;
    double load_factor;
    std::vector<std::forward_list<T>> hash_set;
    std::vector<std::mutex> mutexes;

    size_t GetBucketIndex(const size_t hash_value) {
        return hash_value % size_of_table.load();
    }

    size_t GetStripeIndex(const size_t hash_value) {
        return hash_value % number_of_mutexes;
    }

    void expansion() {
        //последовательно захватываем все мьютексы
        for (size_t i = 0; i < number_of_mutexes; ++i) {
            mutexes[i].lock();
        }
        //проверяем, что расширение таблицы все еще необходимо
        if ((size_of_set.load() * 1.0) / size_of_table.load() <= load_factor) {
            for (int i = number_of_mutexes - 1; i >= 0; --i) {
                mutexes[i].unlock();
            }
            return;
        }
        size_t new_size_of_table = size_of_table.load() * growth_factor;
        std::vector<std::forward_list<T>> new_hash_set(new_size_of_table);
        //перехеширование
        for (size_t i = 0; i < size_of_table.load(); ++i) {
            for (T element : hash_set[i]) {
                size_t hash_value = hash(element);
                size_t current_bucket = hash_value % new_size_of_table;
                new_hash_set[current_bucket].push_front(element);
            }
        }
        hash_set.swap(new_hash_set);
        size_of_table.store(new_size_of_table);
        for (int i = number_of_mutexes - 1; i >= 0; --i) {
            mutexes[i].unlock();
        }
        return;
    }

};

template <typename T> using ConcurrentSet = StripedHashSet<T>;