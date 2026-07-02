#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <unordered_set>

template <typename T>
class SessionManager {

private:
    std::unordered_set<T> items;

public:

    bool add(const T& item) {
        return items.insert(item).second;
    }

    bool remove(const T& item) {
        return items.erase(item) > 0;
    }

    bool contains(const T& item) const {
        return items.find(item) != items.end();
    }

    bool empty() const {
        return items.empty();
    }

    size_t size() const {
        return items.size();
    }

    void clear() {
        items.clear();
    }

    const std::unordered_set<T>& getItems() const {
        return items;
    }

};

#endif