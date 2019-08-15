//
// Created by jianyu on 11/24/18.
//

#ifndef HOTSPOT_C0MAPOOPSET_HPP
#define HOTSPOT_C0MAPOOPSET_HPP

#include "C0MapOopEntry.hpp"
#include <map>

class C0MapOopSet {
public:
    std::map<int, C0MapOopEntry*> map_set;
    C0MapOopEntry* get_entry(int bci) { return map_set[bci]; }
    C0MapOopEntry* put_entry(int bci, C0MapOopEntry* entry) { map_set[bci] = entry; }
    void print() {
        for (std::map<int, C0MapOopEntry*>::iterator itr = map_set.begin();itr != map_set.end();++itr) {
            printf("bci: %d\n", itr->first);
            for (int i = 0;i < itr->second->n_mask;i++) {
                printf("mask %d %lx\n", i, itr->second->_mask[i]);
            }
        }
    }
    ~C0MapOopSet() {
        for (std::map<int, C0MapOopEntry*>::iterator itr = map_set.begin();itr != map_set.end();itr++) {
            delete itr->second;
        }
    }
};


#endif //HOTSPOT_C0MAPOOPSET_HPP
