
#include "mpxrt.h"

int main() {
	enable_mpx();
	void* heap_top = 0;
	void* bounds[2];
	bounds[0] = (void*)100;
	bounds[1] = (void*)150;
	asm("movq %%rax, %0; \
        bndmk (%%rax), %%bnd0;"
        :
        :"m"(bounds));

    asm("bndcl %0, %%bnd0;"
        :
        :"m"(heap_top));
    asm("bndcu %0, %%bnd0;"
        :
        :"m"(heap_top));
	return 0;
}
