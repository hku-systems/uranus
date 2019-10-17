

#ifndef PANOPLY_GETOPT_H
#define PANOPLY_GETOPT_H

#include <getopt.h>
#include <memory.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern void increase_ocall_count();

int wrapper_getopt(int argc, char** argv, const char *optstring);

void set_optind(int oi);

void set_opterr(int oe);

void set_optopt(int oo);

void set_optreset(int ors);

char* get_optarg();

#if defined(__cplusplus)
}
#endif
#endif
