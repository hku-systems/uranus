//
// Created by Maxxie Jiang on 4/1/2019.
//

#ifndef HOTSPOT_QUEUE_HPP
#define HOTSPOT_QUEUE_HPP

#include <stdlib.h>
namespace enclave {
    template<class T>
    class queue {
    public:
        T* arr;
        int n;
	int capacity;
        int end_cur;
        int prev_cur;
        queue() {
		int s = 10;
		arr = new T[s];
		capacity = s;
		n = 0;
		end_cur = 0;
		prev_cur = 0;
        }
        queue(int s) {
            arr = new T[s];
            capacity = s;
	    n = 0;
            end_cur = 0;
            prev_cur = 0;
        }
        void empty() { end_cur = 0; prev_cur = 0; n = 0; }
        inline void push(T t) {
            if (n >= capacity) {
                resize(capacity * 2);
            }
            int put_cur = end_cur % capacity;
            end_cur = (end_cur + 1) % capacity;
            arr[put_cur] = t;
            n++;
        }

        inline void pop() {
		    prev_cur = (prev_cur + 1) % capacity;
		    n--;
	    }
        inline T& front() { return arr[prev_cur]; }
        inline int size() { return n; }
        void resize(int new_cap) {
            T* new_arr = new T[new_cap];
            memcpy(new_arr, arr, sizeof(T) * capacity);
	        prev_cur = 0;
	        end_cur = n;
            capacity = new_cap;
            delete arr;
            arr = new_arr;
        };
    };
}


#endif //HOTSPOT_QUEUE_HPP
