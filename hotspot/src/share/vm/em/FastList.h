//
// Created by max on 3/11/18.
//

#ifndef HOTSPOT_FASTLIST_H
#define HOTSPOT_FASTLIST_H

#include <cstring>

template<typename M>
class FastList {
public:
#define DEFAULT_FASTLIST_SIZE 20
    M* arr;
    int count;
    int capacity;

    FastList(int _cap = DEFAULT_FASTLIST_SIZE) {
        arr = new M[_cap];
        count = 0;
        capacity = _cap;
    }
    void push_back(M m) {
        if (count >= capacity) {
            expand(2 * capacity);
        }
        arr[count++] = m;
    }
    void expand(int new_cap) {
        M *tmp = new M[new_cap];
        memcpy(tmp, arr, count * sizeof(M));
        delete[] arr;
        arr = tmp;
        capacity = new_cap;
    }
    int size() { return count; }
    void clear() {
        count = 0;
    }
};


#endif //HOTSPOT_FASTLIST_H
