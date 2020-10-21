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
#include "classfile/classFileParser.hpp"
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

#include "enclave/EnclaveOcall.h"

 // Entry points in zip.dll for loading zip/jar file entries

typedef void** (JNICALL* ZipOpen_t)(const char* name, char** pmsg);
typedef void (JNICALL* ZipClose_t)(jzfile* zip);
typedef jzentry* (JNICALL* FindEntry_t)(jzfile* zip, const char* name, jint* sizeP, jint* nameLen);
typedef jboolean(JNICALL* ReadEntry_t)(jzfile* zip, jzentry* entry, unsigned char* buf, char* namebuf);
typedef jboolean(JNICALL* ReadMappedEntry_t)(jzfile* zip, jzentry* entry, unsigned char** buf, char* namebuf);
typedef jzentry* (JNICALL* GetNextEntry_t)(jzfile* zip, jint n);
typedef jint(JNICALL* Crc32_t)(jint crc, const jbyte* buf, jint len);

static ZipOpen_t         ZipOpen = NULL;
static ZipClose_t        ZipClose = NULL;
static FindEntry_t       FindEntry = NULL;
static ReadEntry_t       ReadEntry = NULL;
static ReadMappedEntry_t ReadMappedEntry = NULL;
static GetNextEntry_t    GetNextEntry = NULL;
static canonicalize_fn_t CanonicalizeEntry = NULL;
static Crc32_t           Crc32 = NULL;

// Globals

class PackageInfo : public BasicHashtableEntry<mtClass> {
public:
    const char* _pkgname;       // Package name
    int _classpath_index;       // Index of directory or JAR file loaded from

    PackageInfo* next() {
        return (PackageInfo*)BasicHashtableEntry<mtClass>::next();
    }

    const char* pkgname() { return _pkgname; }
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
    inline unsigned int compute_hash(const char* s, int n) {
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

ClassPathEntry* ClassLoader::_first_entry = NULL;
ClassPathEntry* ClassLoader::_last_entry = NULL;
int             ClassLoader::_num_entries = 0;
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
    }
    else {
        _meta_package_names = NEW_C_HEAP_ARRAY(char*, num_meta_package_names, mtClass);
        _num_meta_package_names = num_meta_package_names;
        memcpy(_meta_package_names, meta_package_names, num_meta_package_names * sizeof(char*));
    }
}


MetaIndex::~MetaIndex() {
    FREE_C_HEAP_ARRAY(char*, _meta_package_names, mtClass);
}


bool MetaIndex::may_contain(const char* class_name) {
    if (_num_meta_package_names == 0) {
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
    char* copy = NEW_C_HEAP_ARRAY(char, strlen(dir) + 1, mtClass);
    strcpy(copy, dir);
    _dir = copy;
}


ClassFileStream* ClassPathDirEntry::open_stream(const char* name, TRAPS) {
    // construct full path name
    char path[JVM_MAXPATHLEN];
    if (jio_snprintf(path, sizeof(path), "%s%s%s", _dir, os::file_separator(), name) == -1) {
        return NULL;
    }
    // check if file exists
    struct stat st;

    return NULL;
}


ClassPathZipEntry::ClassPathZipEntry(jzfile* zip, const char* zip_name) : ClassPathEntry() {
    _zip = zip;
    char* copy = NEW_C_HEAP_ARRAY(char, strlen(zip_name) + 1, mtClass);
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
    }
    else {
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
    }
    else if (cpe->is_jar_file()) {
        return ((ClassPathZipEntry*)cpe)->open_entry(name, filesize, nul_terminate, THREAD);
    }
    else {
        ShouldNotReachHere();
        *filesize = 0;
        return NULL;
    }
}

ClassFileStream* EnclaveClassPathEntry::open_stream(const char *name, Thread *__the_thread__) {

    if (entry_map.find(std::string(name)) == entry_map.end())
      return NULL;

    buffer_cache_t cache_entry = entry_map[std::string(name)];
    return new ClassFileStream((u1*)cache_entry.buffer, cache_entry.size, "enclave");    // Resource allocated
}

void* EnclaveClassPathEntry::class_file_buffer(const char *name, int &size) {
    return JVM_CLASSFILE_BUFFER(name, &size);
}

char* EnclaveClassPathEntry::_enclave_file_cache = NULL;
std::map<std::string, buffer_cache_t> EnclaveClassPathEntry::entry_map;

sgx_sha256_hash_t enclave_jar_hash;

void init_classfile() {
  EnclaveClassPathEntry::init_map();
}

void EnclaveClassPathEntry::init_map() {
  int buffer_size = 0;
  void* obuffer = class_file_buffer(enclave_name(), buffer_size);
  _enclave_file_cache = new char[buffer_size];
  memcpy(_enclave_file_cache, obuffer, buffer_size);

  sgx_sha_state_handle_t p;
  if (sgx_sha256_init(&p) != SGX_SUCCESS) {
    printf("error in sha256 init\n");
    return;
  }

  char* sha2_buf = _enclave_file_cache;
  if (sgx_sha256_update(sha2_buf, buffer_size, p) != SGX_SUCCESS) {
    printf("error in sha256 update\n");
    return;
  }

  if (sgx_sha256_get_hash(p, &enclave_jar_hash) != SGX_SUCCESS) {
    printf("error in sha256 hash\n");
    return;
  }

//  for (int i = 0;i < SGX_SHA256_HASH_SIZE;i++) {
//    printf("%x", enclave_jar_hash[i]);
//  }
//  printf("\n");


  char *cur = _enclave_file_cache;
  char *name;

  entry_map = std::map<std::string, buffer_cache_t>();

  while (cur < _enclave_file_cache + buffer_size) {
    int *fname_len = (int*)cur;
    int *buffer_len = (int*)(cur + *fname_len + 4);
    name = new char[*fname_len + 1];
    memcpy(name, cur + 4, *fname_len);
    name[*fname_len] = 0;
//    printf("class %d %d %s %lx\n", *fname_len, *buffer_len, name, cur + 4 + *fname_len + 4);
    buffer_cache_t cache;
    cache.buffer = cur + 4 + *fname_len + 4, *buffer_len;
    cache.size = *buffer_len;
    entry_map[std::string(name)] = cache;
    cur = cur + 4 + *fname_len + 4 + *buffer_len;
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
        }
        else {
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
    }
    else {
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
void ClassLoader::check_shared_classpath(const char* path) {
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

bool ClassLoader::check_shared_paths_misc_info(void* buf, int size) {
    D_WARN_Unimplement;
}
#endif

void ClassLoader::setup_search_path(const char* class_path, bool canonicalize) {
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

ClassPathEntry* ClassLoader::create_class_path_entry(const char* path, const struct stat* st,
    bool lazy, bool throw_exception, TRAPS) {
    D_WARN_Unimplement;
}


// Create a class path zip entry for a given path (return NULL if not found
// or zip/JAR file cannot be opened)
ClassPathZipEntry* ClassLoader::create_class_path_zip_entry(const char* path) {
    D_WARN_Unimplement;
}

// returns true if entry already on class path
bool ClassLoader::contains_entry(ClassPathEntry* entry) {
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

void ClassLoader::add_to_list(ClassPathEntry* new_entry) {
    if (new_entry != NULL) {
        if (_last_entry == NULL) {
            _first_entry = _last_entry = new_entry;
        }
        else {
            _last_entry->set_next(new_entry);
            _last_entry = new_entry;
        }
    }
    _num_entries++;
}

// Returns true IFF the file/dir exists and the entry was successfully created.
bool ClassLoader::update_class_path_entry_list(const char* path,
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


PackageInfo* ClassLoader::lookup_package(const char* pkgname) {
    D_WARN_Unimplement;
}


bool ClassLoader::add_package(const char* pkgname, int classpath_index, TRAPS) {
    D_WARN_Unimplement;
}


oop ClassLoader::get_system_package(const char* name, TRAPS) {
    D_WARN_Unimplement;
}


objArrayOop ClassLoader::get_system_packages(TRAPS) {
    D_WARN_Unimplement;
}


instanceKlassHandle ClassLoader::load_classfile(Symbol* h_name, TRAPS) {
    ResourceMark rm(THREAD);
    const char* class_name = h_name->as_C_string();

    stringStream st;
    // st.print() uses too much stack space while handling a StackOverflowError
    // st.print("%s.class", h_name->as_utf8());
    st.print_raw(h_name->as_utf8());
    st.print_raw(".class");
    const char* file_name = st.as_string();

    // Lookup stream for parsing .class file
    ClassFileStream* stream = NULL;
    int classpath_index = 0;
    ClassPathEntry* e = new EnclaveClassPathEntry();
    instanceKlassHandle h;
    {
        while (e != NULL) {
            stream = e->open_stream(file_name, CHECK_NULL);
            if (stream != NULL) {
                break;
            }
            e = e->next();
            ++classpath_index;
        }
    }

    if (stream != NULL) {
        // class file found, parse it
        ClassFileParser parser(stream);
        ClassLoaderData* loader_data = ClassLoaderData::the_null_class_loader_data();
        Handle protection_domain;
        TempNewSymbol parsed_name = NULL;
        instanceKlassHandle result = parser.parseClassFile(h_name,
            loader_data,
            protection_domain,
            parsed_name,
            false,
            THREAD);
        if (HAS_PENDING_EXCEPTION) {
            ResourceMark rm;
            if (DumpSharedSpaces) {
                tty->print_cr("Preload Error: Failed to load %s", class_name);
            }
            return h;
        }
        return result;
    }
    else {
        if (DumpSharedSpaces) {
            tty->print_cr("Preload Warning: Cannot find %s", class_name);
        }
    }

    return h;
}


void ClassLoader::create_package_info_table(HashtableBucket<mtClass>* t, int length,
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
  // hardwired for JDK1.2 -- would need to duplicate class file parsing
  // code to determine actual value from file
  // Would be value '11' if finals were in vtable
  int JDK_1_2_Object_vtable_size = 5;
  return JDK_1_2_Object_vtable_size * vtableEntry::size();
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
        char* orig_copy = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, strlen(orig) + 1);
        strcpy(orig_copy, orig);
        if ((CanonicalizeEntry)(env, os::native_path(orig_copy), out, len) < 0) {
            return false;
        }
    }
    else {
        // On JDK 1.2.2 the Canonicalize does not exist, so just do nothing
        strncpy(out, orig, len);
        out[len - 1] = '\0';
    }
    return true;
}

// Please keep following two functions at end of this file. With them placed at top or in middle of the file,
// they could get inlined by agressive compiler, an unknown trick, see bug 6966589.
void PerfClassTraceTime::initialize() {
    D_WARN_Unimplement;
}

PerfClassTraceTime::~PerfClassTraceTime() {
    D_WARN_Unimplement;
}