/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "classfile/classLoader.hpp"
#include "classfile/classLoaderData.inline.hpp"
#include "classfile/javaClasses.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "interpreter/oopMapCache.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/universe.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/symbol.hpp"
#include "runtime/handles.hpp"
#include "runtime/handles.inline.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_aix
# include "os_aix.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif


// Entry points in zip.dll for loading zip/jar file entries

typedef void * * (JNICALL *ZipOpen_t)(const char *name, char **pmsg);
typedef void (JNICALL *ZipClose_t)(jzfile *zip);
typedef jzentry* (JNICALL *FindEntry_t)(jzfile *zip, const char *name, jint *sizeP, jint *nameLen);
typedef jboolean (JNICALL *ReadEntry_t)(jzfile *zip, jzentry *entry, unsigned char *buf, char *namebuf);
typedef jboolean (JNICALL *ReadMappedEntry_t)(jzfile *zip, jzentry *entry, unsigned char **buf, char *namebuf);
typedef jzentry* (JNICALL *GetNextEntry_t)(jzfile *zip, jint n);
typedef jint     (JNICALL *Crc32_t)(jint crc, const jbyte *buf, jint len);

static ZipOpen_t         ZipOpen            = NULL;
static ZipClose_t        ZipClose           = NULL;
static FindEntry_t       FindEntry          = NULL;
static ReadEntry_t       ReadEntry          = NULL;
static ReadMappedEntry_t ReadMappedEntry    = NULL;
static GetNextEntry_t    GetNextEntry       = NULL;
static canonicalize_fn_t CanonicalizeEntry  = NULL;
static Crc32_t           Crc32              = NULL;

// Globals

class PackageInfo: public BasicHashtableEntry<mtClass> {
public:
    const char* _pkgname;       // Package name
    int _classpath_index;       // Index of directory or JAR file loaded from

    PackageInfo* next() {
      return (PackageInfo*)BasicHashtableEntry<mtClass>::next();
    }

    const char* pkgname()           { return _pkgname; }
    void set_pkgname(char* pkgname) { _pkgname = pkgname; }

    const char* filename() {
      return ClassLoader::classpath_entry(_classpath_index)->name();
    }

    void set_index(int index) {
      _classpath_index = index;
    }
};

class PackageHashtable : public BasicHashtable<mtClass> {
private:
    inline unsigned int compute_hash(const char *s, int n) {
      unsigned int val = 0;
      while (--n >= 0) {
        val = *s++ + 31 * val;
      }
      return val;
    }

    PackageInfo* bucket(int index) {
      return (PackageInfo*)BasicHashtable<mtClass>::bucket(index);
    }

    PackageInfo* get_entry(int index, unsigned int hash,
                           const char* pkgname, size_t n) {
      for (PackageInfo* pp = bucket(index); pp != NULL; pp = pp->next()) {
        if (pp->hash() == hash &&
            strncmp(pkgname, pp->pkgname(), n) == 0 &&
            pp->pkgname()[n] == '\0') {
          return pp;
        }
      }
      return NULL;
    }

public:
    PackageHashtable(int table_size)
            : BasicHashtable<mtClass>(table_size, sizeof(PackageInfo)) {}

    PackageHashtable(int table_size, HashtableBucket<mtClass>* t, int number_of_entries)
            : BasicHashtable<mtClass>(table_size, sizeof(PackageInfo), t, number_of_entries) {}

    PackageInfo* get_entry(const char* pkgname, int n) {
      unsigned int hash = compute_hash(pkgname, n);
      return get_entry(hash_to_index(hash), hash, pkgname, n);
    }

    PackageInfo* new_entry(char* pkgname, int n) {
      unsigned int hash = compute_hash(pkgname, n);
      PackageInfo* pp;
      pp = (PackageInfo*)BasicHashtable<mtClass>::new_entry(hash);
      pp->set_pkgname(pkgname);
      return pp;
    }

    void add_entry(PackageInfo* pp) {
      int index = hash_to_index(pp->hash());
      BasicHashtable<mtClass>::add_entry(index, pp);
    }

    void copy_pkgnames(const char** packages) {
      int n = 0;
      for (int i = 0; i < table_size(); ++i) {
        for (PackageInfo* pp = bucket(i); pp != NULL; pp = pp->next()) {
          packages[n++] = pp->pkgname();
        }
      }
      assert(n == number_of_entries(), "just checking");
    }

    CDS_ONLY(void copy_table(char** top, char* end, PackageHashtable* table);)
};

ClassPathEntry* ClassLoader::_first_entry         = NULL;
ClassPathEntry* ClassLoader::_last_entry          = NULL;
int             ClassLoader::_num_entries         = 0;
PackageHashtable* ClassLoader::_package_hash_table = NULL;

#if INCLUDE_CDS
SharedPathsMiscInfo* ClassLoader::_shared_paths_misc_info = NULL;
#endif
// helper routines
bool string_starts_with(const char* str, const char* str_to_find) {
  size_t str_len = strlen(str);
  size_t str_to_find_len = strlen(str_to_find);
  if (str_to_find_len > str_len) {
    return false;
  }
  return (strncmp(str, str_to_find, str_to_find_len) == 0);
}

bool string_ends_with(const char* str, const char* str_to_find) {
  size_t str_len = strlen(str);
  size_t str_to_find_len = strlen(str_to_find);
  if (str_to_find_len > str_len) {
    return false;
  }
  return (strncmp(str + (str_len - str_to_find_len), str_to_find, str_to_find_len) == 0);
}


MetaIndex::MetaIndex(char** meta_package_names, int num_meta_package_names) {
  if (num_meta_package_names == 0) {
    _meta_package_names = NULL;
    _num_meta_package_names = 0;
  } else {
    _meta_package_names = NEW_C_HEAP_ARRAY(char*, num_meta_package_names, mtClass);
    _num_meta_package_names = num_meta_package_names;
    memcpy(_meta_package_names, meta_package_names, num_meta_package_names * sizeof(char*));
  }
}


MetaIndex::~MetaIndex() {
  FREE_C_HEAP_ARRAY(char*, _meta_package_names, mtClass);
}


bool MetaIndex::may_contain(const char* class_name) {
  if ( _num_meta_package_names == 0) {
    return false;
  }
  size_t class_name_len = strlen(class_name);
  for (int i = 0; i < _num_meta_package_names; i++) {
    char* pkg = _meta_package_names[i];
    size_t pkg_len = strlen(pkg);
    size_t min_len = MIN2(class_name_len, pkg_len);
    if (!strncmp(class_name, pkg, min_len)) {
      return true;
    }
  }
  return false;
}


ClassPathEntry::ClassPathEntry() {
  set_next(NULL);
}


bool ClassPathEntry::is_lazy() {
  return false;
}

ClassPathDirEntry::ClassPathDirEntry(const char* dir) : ClassPathEntry() {
  char* copy = NEW_C_HEAP_ARRAY(char, strlen(dir)+1, mtClass);
  strcpy(copy, dir);
  _dir = copy;
}


ClassFileStream* ClassPathDirEntry::open_stream(const char* name, TRAPS) {
  D_WARN_Unimplement;
}


ClassPathZipEntry::ClassPathZipEntry(jzfile* zip, const char* zip_name) : ClassPathEntry() {
  _zip = zip;
  char *copy = NEW_C_HEAP_ARRAY(char, strlen(zip_name)+1, mtClass);
  strcpy(copy, zip_name);
  _zip_name = copy;
}

ClassPathZipEntry::~ClassPathZipEntry() {
  if (ZipClose != NULL) {
    (*ZipClose)(_zip);
  }
  FREE_C_HEAP_ARRAY(char, (void*)_zip_name, mtClass);
}

u1* ClassPathZipEntry::open_entry(const char* name, jint* filesize, bool nul_terminate, TRAPS) {
  D_WARN_Unimplement;
}

ClassFileStream* ClassPathZipEntry::open_stream(const char* name, TRAPS) {
  D_WARN_Unimplement;
}

// invoke function for each entry in the zip file
void ClassPathZipEntry::contents_do(void f(const char* name, void* context), void* context) {
  D_WARN_Unimplement;
}

LazyClassPathEntry::LazyClassPathEntry(const char* path, const struct stat* st, bool throw_exception) : ClassPathEntry() {
  D_WARN_Unimplement;
  _st = *st;
  _meta_index = NULL;
  _resolved_entry = NULL;
  _has_error = false;
  _throw_exception = throw_exception;
}

bool LazyClassPathEntry::is_jar_file() {
  return ((_st.st_mode & S_IFREG) == S_IFREG);
}

ClassPathEntry* LazyClassPathEntry::resolve_entry(TRAPS) {
  D_WARN_Unimplement;
}

ClassFileStream* LazyClassPathEntry::open_stream(const char* name, TRAPS) {
  if (_meta_index != NULL &&
      !_meta_index->may_contain(name)) {
    return NULL;
  }
  if (_has_error) {
    return NULL;
  }
  ClassPathEntry* cpe = resolve_entry(THREAD);
  if (cpe == NULL) {
    _has_error = true;
    return NULL;
  } else {
    return cpe->open_stream(name, THREAD);
  }
}

bool LazyClassPathEntry::is_lazy() {
  return true;
}

u1* LazyClassPathEntry::open_entry(const char* name, jint* filesize, bool nul_terminate, TRAPS) {
  if (_has_error) {
    return NULL;
  }
  ClassPathEntry* cpe = resolve_entry(THREAD);
  if (cpe == NULL) {
    _has_error = true;
    return NULL;
  } else if (cpe->is_jar_file()) {
    return ((ClassPathZipEntry*)cpe)->open_entry(name, filesize, nul_terminate,THREAD);
  } else {
    ShouldNotReachHere();
    *filesize = 0;
    return NULL;
  }
}

static void print_meta_index(LazyClassPathEntry* entry,
                             GrowableArray<char*>& meta_packages) {
  tty->print("[Meta index for %s=", entry->name());
  for (int i = 0; i < meta_packages.length(); i++) {
    if (i > 0) tty->print(" ");
    tty->print("%s", meta_packages.at(i));
  }
  tty->print_cr("]");
}

#if INCLUDE_CDS
void ClassLoader::exit_with_path_failure(const char* error, const char* message) {
  D_WARN_Unimplement;
}
#endif

void ClassLoader::trace_class_path(outputStream* out, const char* msg, const char* name) {
  if (!TraceClassPaths) {
    return;
  }

  if (msg) {
    out->print("%s", msg);
  }
  if (name) {
    if (strlen(name) < 256) {
      out->print("%s", name);
    } else {
      // For very long paths, we need to print each character separately,
      // as print_cr() has a length limit
      while (name[0] != '\0') {
        out->print("%c", name[0]);
        name++;
      }
    }
  }
  if (msg && msg[0] == '[') {
    out->print_cr("]");
  } else {
    out->cr();
  }
}

void ClassLoader::setup_bootstrap_meta_index() {
  D_WARN_Unimplement;
}

void ClassLoader::setup_meta_index(const char* meta_index_path, const char* meta_index_dir, int start_index) {
  D_WARN_Unimplement;
}

#if INCLUDE_CDS
void ClassLoader::check_shared_classpath(const char *path) {
  if (strcmp(path, "") == 0) {
    exit_with_path_failure("Cannot have empty path in archived classpaths", NULL);
  }

  struct stat st;
  if (os::stat(path, &st) == 0) {
    if ((st.st_mode & S_IFREG) != S_IFREG) { // is directory
      if (!os::dir_is_empty(path)) {
        tty->print_cr("Error: non-empty directory '%s'", path);
        exit_with_path_failure("CDS allows only empty directories in archived classpaths", NULL);
      }
    }
  }
}
#endif

void ClassLoader::setup_bootstrap_search_path() {
  D_WARN_Unimplement;
}

#if INCLUDE_CDS
int ClassLoader::get_shared_paths_misc_info_size() {
  D_WARN_Unimplement;
}

void* ClassLoader::get_shared_paths_misc_info() {
  D_WARN_Unimplement;
}

bool ClassLoader::check_shared_paths_misc_info(void *buf, int size) {
  D_WARN_Unimplement;
}
#endif

void ClassLoader::setup_search_path(const char *class_path, bool canonicalize) {
  int offset = 0;
  int len = (int)strlen(class_path);
  int end = 0;

  // Iterate over class path entries
  for (int start = 0; start < len; start = end) {
    while (class_path[end] && class_path[end] != os::path_separator()[0]) {
      end++;
    }
    EXCEPTION_MARK;
    ResourceMark rm(THREAD);
    char* path = NEW_RESOURCE_ARRAY(char, end - start + 1);
    strncpy(path, &class_path[start], end - start);
    path[end - start] = '\0';
    if (canonicalize) {
      char* canonical_path = NEW_RESOURCE_ARRAY(char, JVM_MAXPATHLEN + 1);
      if (get_canonical_path(path, canonical_path, JVM_MAXPATHLEN)) {
        path = canonical_path;
      }
    }
    update_class_path_entry_list(path, /*check_for_duplicates=*/canonicalize);
#if INCLUDE_CDS
    if (DumpSharedSpaces) {
      check_shared_classpath(path);
    }
#endif
    while (class_path[end] == os::path_separator()[0]) {
      end++;
    }
  }
}

ClassPathEntry* ClassLoader::create_class_path_entry(const char *path, const struct stat* st,
                                                     bool lazy, bool throw_exception, TRAPS) {
  D_WARN_Unimplement;
}


// Create a class path zip entry for a given path (return NULL if not found
// or zip/JAR file cannot be opened)
ClassPathZipEntry* ClassLoader::create_class_path_zip_entry(const char *path) {
  D_WARN_Unimplement;
}

// returns true if entry already on class path
bool ClassLoader::contains_entry(ClassPathEntry *entry) {
  ClassPathEntry* e = _first_entry;
  while (e != NULL) {
    // assume zip entries have been canonicalized
    if (strcmp(entry->name(), e->name()) == 0) {
      return true;
    }
    e = e->next();
  }
  return false;
}

void ClassLoader::add_to_list(ClassPathEntry *new_entry) {
  if (new_entry != NULL) {
    if (_last_entry == NULL) {
      _first_entry = _last_entry = new_entry;
    } else {
      _last_entry->set_next(new_entry);
      _last_entry = new_entry;
    }
  }
  _num_entries ++;
}

// Returns true IFF the file/dir exists and the entry was successfully created.
bool ClassLoader::update_class_path_entry_list(const char *path,
                                               bool check_for_duplicates,
                                               bool throw_exception) {
  D_WARN_Unimplement;
}

void ClassLoader::print_bootclasspath() {
  ClassPathEntry* e = _first_entry;
  tty->print("[bootclasspath= ");
  while (e != NULL) {
    tty->print("%s ;", e->name());
    e = e->next();
  }
  tty->print_cr("]");
}

void ClassLoader::load_zip_library() {
  D_WARN_Unimplement;
}

int ClassLoader::crc32(int crc, const char* buf, int len) {
  assert(Crc32 != NULL, "ZIP_CRC32 is not found");
  return (*Crc32)(crc, (const jbyte*)buf, len);
}

// PackageInfo data exists in order to support the java.lang.Package
// class.  A Package object provides information about a java package
// (version, vendor, etc.) which originates in the manifest of the jar
// file supplying the package.  For application classes, the ClassLoader
// object takes care of this.

// For system (boot) classes, the Java code in the Package class needs
// to be able to identify which source jar file contained the boot
// class, so that it can extract the manifest from it.  This table
// identifies java packages with jar files in the boot classpath.

// Because the boot classpath cannot change, the classpath index is
// sufficient to identify the source jar file or directory.  (Since
// directories have no manifests, the directory name is not required,
// but is available.)

// When using sharing -- the pathnames of entries in the boot classpath
// may not be the same at runtime as they were when the archive was
// created (NFS, Samba, etc.).  The actual files and directories named
// in the classpath must be the same files, in the same order, even
// though the exact name is not the same.


PackageInfo* ClassLoader::lookup_package(const char *pkgname) {
  D_WARN_Unimplement;
}


bool ClassLoader::add_package(const char *pkgname, int classpath_index, TRAPS) {
  D_WARN_Unimplement;
}


oop ClassLoader::get_system_package(const char* name, TRAPS) {
  D_WARN_Unimplement;
}


objArrayOop ClassLoader::get_system_packages(TRAPS) {
  D_WARN_Unimplement;
}


instanceKlassHandle ClassLoader::load_classfile(Symbol* h_name, TRAPS) {
  D_WARN_Unimplement;
}


void ClassLoader::create_package_info_table(HashtableBucket<mtClass> *t, int length,
                                            int number_of_entries) {
  assert(_package_hash_table == NULL, "One package info table allowed.");
  assert(length == package_hash_table_size * sizeof(HashtableBucket<mtClass>),
         "bad shared package info size.");
  _package_hash_table = new PackageHashtable(package_hash_table_size, t,
                                             number_of_entries);
}


void ClassLoader::create_package_info_table() {
    assert(_package_hash_table == NULL, "shouldn't have one yet");
    _package_hash_table = new PackageHashtable(package_hash_table_size);
}


// Initialize the class loader's access to methods in libzip.  Parse and
// process the boot classpath into a list ClassPathEntry objects.  Once
// this list has been created, it must not change order (see class PackageInfo)
// it can be appended to and is by jvmti and the kernel vm.

void ClassLoader::initialize() {
  D_WARN_Unimplement;
}

#if INCLUDE_CDS
void ClassLoader::initialize_shared_path() {

}
#endif

jlong ClassLoader::classloader_time_ms() {

}

jlong ClassLoader::class_init_count() {

}

jlong ClassLoader::class_init_time_ms() {

}

jlong ClassLoader::class_verify_time_ms() {

}

jlong ClassLoader::class_link_count() {

}

jlong ClassLoader::class_link_time_ms() {

}

int ClassLoader::compute_Object_vtable() {
  D_WARN_Unimplement;
}


void classLoader_init() {
  ClassLoader::initialize();
}


bool ClassLoader::get_canonical_path(const char* orig, char* out, int len) {
  assert(orig != NULL && out != NULL && len > 0, "bad arguments");
  if (CanonicalizeEntry != NULL) {
    JavaThread* THREAD = JavaThread::current();
    JNIEnv* env = THREAD->jni_environment();
    ResourceMark rm(THREAD);

    // os::native_path writes into orig_copy
    char* orig_copy = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, strlen(orig)+1);
    strcpy(orig_copy, orig);
    if ((CanonicalizeEntry)(env, os::native_path(orig_copy), out, len) < 0) {
      return false;
    }
  } else {
    // On JDK 1.2.2 the Canonicalize does not exist, so just do nothing
    strncpy(out, orig, len);
    out[len - 1] = '\0';
  }
  return true;
}

#ifndef PRODUCT

void ClassLoader::verify() {
  _package_hash_table->verify();
}


// CompileTheWorld
//
// Iterates over all class path entries and forces compilation of all methods
// in all classes found. Currently, only zip/jar archives are searched.
//
// The classes are loaded by the Java level bootstrap class loader, and the
// initializer is called. If DelayCompilationDuringStartup is true (default),
// the interpreter will run the initialization code. Note that forcing
// initialization in this way could potentially lead to initialization order
// problems, in which case we could just force the initialization bit to be set.


// We need to iterate over the contents of a zip/jar file, so we replicate the
// jzcell and jzfile definitions from zip_util.h but rename jzfile to real_jzfile,
// since jzfile already has a void* definition.
//
// Note that this is only used in debug mode.
//
// HotSpot integration note:
// Matches zip_util.h 1.14 99/06/01 from jdk1.3 beta H build


// JDK 1.3 version
typedef struct real_jzentry13 {         /* Zip file entry */
    char *name;                 /* entry name */
    jint time;                  /* modification time */
    jint size;                  /* size of uncompressed data */
    jint csize;                 /* size of compressed data (zero if uncompressed) */
    jint crc;                   /* crc of uncompressed data */
    char *comment;              /* optional zip file comment */
    jbyte *extra;               /* optional extra data */
    jint pos;                   /* position of LOC header (if negative) or data */
} real_jzentry13;

typedef struct real_jzfile13 {  /* Zip file */
    char *name;                 /* zip file name */
    jint refs;                  /* number of active references */
    jint fd;                    /* open file descriptor */
    void *lock;                 /* read lock */
    char *comment;              /* zip file comment */
    char *msg;                  /* zip error message */
    void *entries;              /* array of hash cells */
    jint total;                 /* total number of entries */
    unsigned short *table;      /* Hash chain heads: indexes into entries */
    jint tablelen;              /* number of hash eads */
    real_jzfile13 *next;        /* next zip file in search list */
    jzentry *cache;             /* we cache the most recently freed jzentry */
    /* Information on metadata names in META-INF directory */
    char **metanames;           /* array of meta names (may have null names) */
    jint metacount;             /* number of slots in metanames array */
    /* If there are any per-entry comments, they are in the comments array */
    char **comments;
} real_jzfile13;

// JDK 1.2 version
typedef struct real_jzentry12 {  /* Zip file entry */
    char *name;                  /* entry name */
    jint time;                   /* modification time */
    jint size;                   /* size of uncompressed data */
    jint csize;                  /* size of compressed data (zero if uncompressed) */
    jint crc;                    /* crc of uncompressed data */
    char *comment;               /* optional zip file comment */
    jbyte *extra;                /* optional extra data */
    jint pos;                    /* position of LOC header (if negative) or data */
    struct real_jzentry12 *next; /* next entry in hash table */
} real_jzentry12;

typedef struct real_jzfile12 {  /* Zip file */
    char *name;                 /* zip file name */
    jint refs;                  /* number of active references */
    jint fd;                    /* open file descriptor */
    void *lock;                 /* read lock */
    char *comment;              /* zip file comment */
    char *msg;                  /* zip error message */
    real_jzentry12 *entries;    /* array of zip entries */
    jint total;                 /* total number of entries */
    real_jzentry12 **table;     /* hash table of entries */
    jint tablelen;              /* number of buckets */
    jzfile *next;               /* next zip file in search list */
} real_jzfile12;


void ClassPathDirEntry::compile_the_world(Handle loader, TRAPS) {
  // For now we only compile all methods in all classes in zip/jar files
  tty->print_cr("CompileTheWorld : Skipped classes in %s", _dir);
  tty->cr();
}


bool ClassPathDirEntry::is_rt_jar() {
  return false;
}

void ClassPathZipEntry::compile_the_world(Handle loader, TRAPS) {
  if (JDK_Version::is_jdk12x_version()) {
    compile_the_world12(loader, THREAD);
  } else {
    compile_the_world13(loader, THREAD);
  }
  if (HAS_PENDING_EXCEPTION) {
    if (PENDING_EXCEPTION->is_a(SystemDictionary::OutOfMemoryError_klass())) {
      CLEAR_PENDING_EXCEPTION;
      tty->print_cr("\nCompileTheWorld : Ran out of memory\n");
      tty->print_cr("Increase class metadata storage if a limit was set");
    } else {
      tty->print_cr("\nCompileTheWorld : Unexpected exception occurred\n");
    }
  }
}

// Version that works for JDK 1.3.x
void ClassPathZipEntry::compile_the_world13(Handle loader, TRAPS) {
  real_jzfile13* zip = (real_jzfile13*) _zip;
  tty->print_cr("CompileTheWorld : Compiling all classes in %s", zip->name);
  tty->cr();
  // Iterate over all entries in zip file
  for (int n = 0; ; n++) {
    real_jzentry13 * ze = (real_jzentry13 *)((*GetNextEntry)(_zip, n));
    if (ze == NULL) break;
    ClassLoader::compile_the_world_in(ze->name, loader, CHECK);
  }
}


// Version that works for JDK 1.2.x
void ClassPathZipEntry::compile_the_world12(Handle loader, TRAPS) {
  real_jzfile12* zip = (real_jzfile12*) _zip;
  tty->print_cr("CompileTheWorld : Compiling all classes in %s", zip->name);
  tty->cr();
  // Iterate over all entries in zip file
  for (int n = 0; ; n++) {
    real_jzentry12 * ze = (real_jzentry12 *)((*GetNextEntry)(_zip, n));
    if (ze == NULL) break;
    ClassLoader::compile_the_world_in(ze->name, loader, CHECK);
  }
}

bool ClassPathZipEntry::is_rt_jar() {
  if (JDK_Version::is_jdk12x_version()) {
    return is_rt_jar12();
  } else {
    return is_rt_jar13();
  }
}

// JDK 1.3 version
bool ClassPathZipEntry::is_rt_jar13() {
  real_jzfile13* zip = (real_jzfile13*) _zip;
  int len = (int)strlen(zip->name);
  // Check whether zip name ends in "rt.jar"
  // This will match other archives named rt.jar as well, but this is
  // only used for debugging.
  return (len >= 6) && (strcasecmp(zip->name + len - 6, "rt.jar") == 0);
}

// JDK 1.2 version
bool ClassPathZipEntry::is_rt_jar12() {
  real_jzfile12* zip = (real_jzfile12*) _zip;
  int len = (int)strlen(zip->name);
  // Check whether zip name ends in "rt.jar"
  // This will match other archives named rt.jar as well, but this is
  // only used for debugging.
  return (len >= 6) && (strcasecmp(zip->name + len - 6, "rt.jar") == 0);
}

void LazyClassPathEntry::compile_the_world(Handle loader, TRAPS) {
  ClassPathEntry* cpe = resolve_entry(THREAD);
  if (cpe != NULL) {
    cpe->compile_the_world(loader, CHECK);
  }
}

bool LazyClassPathEntry::is_rt_jar() {
  Thread* THREAD = Thread::current();
  ClassPathEntry* cpe = resolve_entry(THREAD);
  return (cpe != NULL) ? cpe->is_jar_file() : false;
}

void ClassLoader::compile_the_world() {
  EXCEPTION_MARK;
  HandleMark hm(THREAD);
  ResourceMark rm(THREAD);
  // Make sure we don't run with background compilation
  BackgroundCompilation = false;
  // Find bootstrap loader
  Handle system_class_loader (THREAD, SystemDictionary::java_system_loader());
  // Iterate over all bootstrap class path entries
  ClassPathEntry* e = _first_entry;
  jlong start = os::javaTimeMillis();
  while (e != NULL) {
    // We stop at rt.jar, unless it is the first bootstrap path entry
    if (e->is_rt_jar() && e != _first_entry) break;
    e->compile_the_world(system_class_loader, CATCH);
    e = e->next();
  }
  jlong end = os::javaTimeMillis();
  tty->print_cr("CompileTheWorld : Done (%d classes, %d methods, " JLONG_FORMAT " ms)",
                _compile_the_world_class_counter, _compile_the_world_method_counter, (end - start));
  {
    // Print statistics as if before normal exit:
    extern void print_statistics();
    print_statistics();
  }
  vm_exit(0);
}

int ClassLoader::_compile_the_world_class_counter = 0;
int ClassLoader::_compile_the_world_method_counter = 0;
static int _codecache_sweep_counter = 0;

// Filter out all exceptions except OOMs
static void clear_pending_exception_if_not_oom(TRAPS) {
  if (HAS_PENDING_EXCEPTION &&
      !PENDING_EXCEPTION->is_a(SystemDictionary::OutOfMemoryError_klass())) {
    CLEAR_PENDING_EXCEPTION;
  }
  // The CHECK at the caller will propagate the exception out
}

/**
 * Returns if the given method should be compiled when doing compile-the-world.
 *
 * TODO:  This should be a private method in a CompileTheWorld class.
 */
static bool can_be_compiled(methodHandle m, int comp_level) {
  assert(CompileTheWorld, "must be");

  // It's not valid to compile a native wrapper for MethodHandle methods
  // that take a MemberName appendix since the bytecode signature is not
  // correct.
  vmIntrinsics::ID iid = m->intrinsic_id();
  if (MethodHandles::is_signature_polymorphic(iid) && MethodHandles::has_member_arg(iid)) {
    return false;
  }

  return CompilationPolicy::can_be_compiled(m, comp_level);
}

void ClassLoader::compile_the_world_in(char* name, Handle loader, TRAPS) {
  int len = (int)strlen(name);
  if (len > 6 && strcmp(".class", name + len - 6) == 0) {
    // We have a .class file
    char buffer[2048];
    strncpy(buffer, name, len - 6);
    buffer[len-6] = 0;
    // If the file has a period after removing .class, it's not really a
    // valid class file.  The class loader will check everything else.
    if (strchr(buffer, '.') == NULL) {
      _compile_the_world_class_counter++;
      if (_compile_the_world_class_counter > CompileTheWorldStopAt) return;

      // Construct name without extension
      TempNewSymbol sym = SymbolTable::new_symbol(buffer, CHECK);
      // Use loader to load and initialize class
      Klass* ik = SystemDictionary::resolve_or_null(sym, loader, Handle(), THREAD);
      instanceKlassHandle k (THREAD, ik);
      if (k.not_null() && !HAS_PENDING_EXCEPTION) {
        k->initialize(THREAD);
      }
      bool exception_occurred = HAS_PENDING_EXCEPTION;
      clear_pending_exception_if_not_oom(CHECK);
      if (CompileTheWorldPreloadClasses && k.not_null()) {
        ConstantPool::preload_and_initialize_all_classes(k->constants(), THREAD);
        if (HAS_PENDING_EXCEPTION) {
          // If something went wrong in preloading we just ignore it
          clear_pending_exception_if_not_oom(CHECK);
          tty->print_cr("Preloading failed for (%d) %s", _compile_the_world_class_counter, buffer);
        }
      }

      if (_compile_the_world_class_counter >= CompileTheWorldStartAt) {
        if (k.is_null() || exception_occurred) {
          // If something went wrong (e.g. ExceptionInInitializerError) we skip this class
          tty->print_cr("CompileTheWorld (%d) : Skipping %s", _compile_the_world_class_counter, buffer);
        } else {
          tty->print_cr("CompileTheWorld (%d) : %s", _compile_the_world_class_counter, buffer);
          // Preload all classes to get around uncommon traps
          // Iterate over all methods in class
          int comp_level = CompilationPolicy::policy()->initial_compile_level();
          for (int n = 0; n < k->methods()->length(); n++) {
            methodHandle m (THREAD, k->methods()->at(n));
            if (can_be_compiled(m, comp_level)) {
              if (++_codecache_sweep_counter == CompileTheWorldSafepointInterval) {
                // Give sweeper a chance to keep up with CTW
                VM_ForceSafepoint op;
                VMThread::execute(&op);
                _codecache_sweep_counter = 0;
              }
              // Force compilation
              CompileBroker::compile_method(m, InvocationEntryBci, comp_level,
                                            methodHandle(), 0, "CTW", THREAD);
              if (HAS_PENDING_EXCEPTION) {
                clear_pending_exception_if_not_oom(CHECK);
                tty->print_cr("CompileTheWorld (%d) : Skipping method: %s", _compile_the_world_class_counter, m->name_and_sig_as_C_string());
              } else {
                _compile_the_world_method_counter++;
              }
              if (TieredCompilation && TieredStopAtLevel >= CompLevel_full_optimization) {
                // Clobber the first compile and force second tier compilation
                nmethod* nm = m->code();
                if (nm != NULL && !m->is_method_handle_intrinsic()) {
                  // Throw out the code so that the code cache doesn't fill up
                  nm->make_not_entrant();
                }
                CompileBroker::compile_method(m, InvocationEntryBci, CompLevel_full_optimization,
                                              methodHandle(), 0, "CTW", THREAD);
                if (HAS_PENDING_EXCEPTION) {
                  clear_pending_exception_if_not_oom(CHECK);
                  tty->print_cr("CompileTheWorld (%d) : Skipping method: %s", _compile_the_world_class_counter, m->name_and_sig_as_C_string());
                } else {
                  _compile_the_world_method_counter++;
                }
              }
            } else {
              tty->print_cr("CompileTheWorld (%d) : Skipping method: %s", _compile_the_world_class_counter, m->name_and_sig_as_C_string());
            }

            nmethod* nm = m->code();
            if (nm != NULL && !m->is_method_handle_intrinsic()) {
              // Throw out the code so that the code cache doesn't fill up
              nm->make_not_entrant();
            }
          }
        }
      }
    }
  }
}

#endif //PRODUCT

// Please keep following two functions at end of this file. With them placed at top or in middle of the file,
// they could get inlined by agressive compiler, an unknown trick, see bug 6966589.
void PerfClassTraceTime::initialize() {
  D_WARN_Unimplement;
}

PerfClassTraceTime::~PerfClassTraceTime() {
  D_WARN_Unimplement;
}
