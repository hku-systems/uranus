//
// Created by max on 12/30/17.
//

#ifndef HOTSPOT_SGX_DLFCN_STRUCT_H
#define HOTSPOT_SGX_DLFCN_STRUCT_H

#include "sgx_stdio_struct.h"

#define __need_size_t

# ifndef R_OK			/* Verbatim from <unistd.h>.  Ugh.  */
/* Values for the second argument to access.
   These may be OR'd together.  */
#  define R_OK	4		/* Test for read permission.  */
#  define W_OK	2		/* Test for write permission.  */
#  define X_OK	1		/* Test for execute permission.  */
#  define F_OK	0		/* Test for existence.  */
# endif

/* Collect various system dependent definitions and declarations.  */
#include "../sgx/bits/dlfcn_ocall.h"

/* If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
   the run-time address of the symbol called NAME in the next shared
   object is returned.  The "next" relation is defined by the order
   the shared objects were loaded.  */
# define RTLD_NEXT	((void *) -1l)

/* If the first argument to `dlsym' or `dlvsym' is set to RTLD_DEFAULT
   the run-time address of the symbol called NAME in the global scope
   is returned.  */
# define RTLD_DEFAULT	((void *) 0)


/* Type for namespace indeces.  */
typedef long int Lmid_t;

/* Special namespace ID values.  */
# define LM_ID_BASE	0	/* Initial namespace.  */
# define LM_ID_NEWLM	-1	/* For dlmopen: request new namespace.  */


/* Structure containing information about object searched using
   `dladdr'.  */
typedef struct
{
    const char *dli_fname;	/* File name of defining object.  */
    void *dli_fbase;		/* Load address of that object.  */
    const char *dli_sname;	/* Name of nearest symbol.  */
    void *dli_saddr;		/* Exact value of nearest symbol.  */
} Dl_info;



/* This is the type of elements in `Dl_serinfo', below.
   The `dls_name' member points to space in the buffer passed to `dlinfo'.  */
typedef struct
{
    char *dls_name;		/* Name of library search path directory.  */
    unsigned int dls_flags;	/* Indicates where this directory came from. */
} Dl_serpath;



/* This is the structure that must be passed (by reference) to `dlinfo' for
   the RTLD_DI_SERINFO and RTLD_DI_SERINFOSIZE requests.  */
typedef struct
{
    size_t dls_size;		/* Size in bytes of the whole buffer.  */
    unsigned int dls_cnt;		/* Number of elements in `dls_serpath'.  */
    Dl_serpath dls_serpath[1];	/* Actually longer, dls_cnt elements.  */
} Dl_serinfo;

#endif //HOTSPOT_SGX_DLFCN_STRUCT_H
