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
#include "oops/oop.inline.hpp"
#include "utilities/ostream.hpp"
#include "utilities/top.hpp"
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

extern "C" void jio_print(const char* s); // Declarationtion of jvm method

outputStream::outputStream(int width) {
  _width       = width;
  _position    = 0;
  _newlines    = 0;
  _precount    = 0;
  _indentation = 0;
}

outputStream::outputStream(int width, bool has_time_stamps) {
  _width       = width;
  _position    = 0;
  _newlines    = 0;
  _precount    = 0;
  _indentation = 0;
}

void outputStream::update_position(const char* s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char ch = s[i];
    if (ch == '\n') {
      _newlines += 1;
      _precount += _position + 1;
      _position = 0;
    } else if (ch == '\t') {
      int tw = 8 - (_position & 7);
      _position += tw;
      _precount -= tw-1;  // invariant:  _precount + _position == total count
    } else {
      _position += 1;
    }
  }
}

// Execute a vsprintf, using the given buffer if necessary.
// Return a pointer to the formatted string.
const char* outputStream::do_vsnprintf(char* buffer, size_t buflen,
                                       const char* format, va_list ap,
                                       bool add_cr,
                                       size_t& result_len) {
  const char* result;
  if (add_cr)  buflen--;
  if (!strchr(format, '%')) {
    // constant format string
    result = format;
    result_len = strlen(result);
    if (add_cr && result_len >= buflen)  result_len = buflen-1;  // truncate
  } else if (format[0] == '%' && format[1] == 's' && format[2] == '\0') {
    // trivial copy-through format string
    result = va_arg(ap, const char*);
    result_len = strlen(result);
    if (add_cr && result_len >= buflen)  result_len = buflen-1;  // truncate
  } else if (vsnprintf(buffer, buflen, format, ap) >= 0) {
    result = buffer;
    result_len = strlen(result);
  } else {
    DEBUG_ONLY(warning("increase O_BUFLEN in ostream.hpp -- output truncated");)
    result = buffer;
    result_len = buflen - 1;
    buffer[result_len] = 0;
  }
  if (add_cr) {
    if (result != buffer) {
      strncpy(buffer, result, buflen);
      result = buffer;
    }
    buffer[result_len++] = '\n';
    buffer[result_len] = 0;
  }
  return result;
}

void outputStream::print(const char* format, ...) {
  char buffer[O_BUFLEN];
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, ap, false, len);
  write(str, len);
  va_end(ap);
}

void outputStream::print_cr(const char* format, ...) {
  char buffer[O_BUFLEN];
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, ap, true, len);
  write(str, len);
  va_end(ap);
}

void outputStream::vprint(const char *format, va_list argptr) {
  char buffer[O_BUFLEN];
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, argptr, false, len);
  write(str, len);
}

void outputStream::vprint_cr(const char* format, va_list argptr) {
  char buffer[O_BUFLEN];
  size_t len;
  const char* str = do_vsnprintf(buffer, O_BUFLEN, format, argptr, true, len);
  write(str, len);
}

void outputStream::fill_to(int col) {
  int need_fill = col - position();
  sp(need_fill);
}

void outputStream::move_to(int col, int slop, int min_space) {
  if (position() >= col + slop)
    cr();
  int need_fill = col - position();
  if (need_fill < min_space)
    need_fill = min_space;
  sp(need_fill);
}

void outputStream::put(char ch) {
  assert(ch != 0, "please fix call site");
  char buf[] = { ch, '\0' };
  write(buf, 1);
}

#define SP_USE_TABS false

void outputStream::sp(int count) {
  if (count < 0)  return;
  if (SP_USE_TABS && count >= 8) {
    int target = position() + count;
    while (count >= 8) {
      this->write("\t", 1);
      count -= 8;
    }
    count = target - position();
  }
  while (count > 0) {
    int nw = (count > 8) ? 8 : count;
    this->write("        ", nw);
    count -= nw;
  }
}

void outputStream::cr() {
  this->write("\n", 1);
}

void outputStream::stamp() {

}

void outputStream::stamp(bool guard,
                         const char* prefix,
                         const char* suffix) {
  if (!guard) {
    return;
  }
  print_raw(prefix);
  stamp();
  print_raw(suffix);
}

void outputStream::date_stamp(bool guard,
                              const char* prefix,
                              const char* suffix) {
  if (!guard) {
    return;
  }
  print_raw(prefix);
  static const char error_time[] = "yyyy-mm-ddThh:mm:ss.mmm+zzzz";
  static const int buffer_length = 32;
  char buffer[buffer_length];
  const char* iso8601_result = os::iso8601_time(buffer, buffer_length);
  if (iso8601_result != NULL) {
    print_raw(buffer);
  } else {
    print_raw(error_time);
  }
  print_raw(suffix);
  return;
}

void outputStream::gclog_stamp(const GCId& gc_id) {

}

outputStream& outputStream::indent() {
  while (_position < _indentation) sp();
  return *this;
}

void outputStream::print_jlong(jlong value) {
  print(JLONG_FORMAT, value);
}

void outputStream::print_julong(julong value) {
  print(JULONG_FORMAT, value);
}

/**
 * This prints out hex data in a 'windbg' or 'xxd' form, where each line is:
 *   <hex-address>: 8 * <hex-halfword> <ascii translation (optional)>
 * example:
 * 0000000: 7f44 4f46 0102 0102 0000 0000 0000 0000  .DOF............
 * 0000010: 0000 0000 0000 0040 0000 0020 0000 0005  .......@... ....
 * 0000020: 0000 0000 0000 0040 0000 0000 0000 015d  .......@.......]
 * ...
 *
 * indent is applied to each line.  Ends with a CR.
 */
void outputStream::print_data(void* data, size_t len, bool with_ascii) {
  size_t limit = (len + 16) / 16 * 16;
  for (size_t i = 0; i < limit; ++i) {
    if (i % 16 == 0) {
      indent().print(SIZE_FORMAT_HEX_W(07)":", i);
    }
    if (i % 2 == 0) {
      print(" ");
    }
    if (i < len) {
      print("%02x", ((unsigned char*)data)[i]);
    } else {
      print("  ");
    }
    if ((i + 1) % 16 == 0) {
      if (with_ascii) {
        print("  ");
        for (size_t j = 0; j < 16; ++j) {
          size_t idx = i + j - 15;
          if (idx < len) {
            char c = ((char*)data)[idx];
            print("%c", c >= 32 && c <= 126 ? c : '.');
          }
        }
      }
      cr();
    }
  }
}

stringStream::stringStream(size_t initial_size) : outputStream() {
  buffer_length = initial_size;
  buffer        = NEW_RESOURCE_ARRAY(char, buffer_length);
  buffer_pos    = 0;
  buffer_fixed  = false;
  DEBUG_ONLY(rm = Thread::current()->current_resource_mark();)
}

// useful for output to fixed chunks of memory, such as performance counters
stringStream::stringStream(char* fixed_buffer, size_t fixed_buffer_size) : outputStream() {
  buffer_length = fixed_buffer_size;
  buffer        = fixed_buffer;
  buffer_pos    = 0;
  buffer_fixed  = true;
}

void stringStream::write(const char* s, size_t len) {
  size_t write_len = len;               // number of non-null bytes to write
  size_t end = buffer_pos + len + 1;    // position after write and final '\0'
  if (end > buffer_length) {
    if (buffer_fixed) {
      // if buffer cannot resize, silently truncate
      end = buffer_length;
      write_len = end - buffer_pos - 1; // leave room for the final '\0'
    } else {
      // For small overruns, double the buffer.  For larger ones,
      // increase to the requested size.
      if (end < buffer_length * 2) {
        end = buffer_length * 2;
      }
      char* oldbuf = buffer;
      assert(rm == NULL || Thread::current()->current_resource_mark() == rm,
             "stringStream is re-allocated with a different ResourceMark");
      buffer = NEW_RESOURCE_ARRAY(char, end);
      strncpy(buffer, oldbuf, buffer_pos);
      buffer_length = end;
    }
  }
  // invariant: buffer is always null-terminated
  guarantee(buffer_pos + write_len + 1 <= buffer_length, "stringStream oob");
  buffer[buffer_pos + write_len] = 0;
  strncpy(buffer + buffer_pos, s, write_len);
  buffer_pos += write_len;

  // Note that the following does not depend on write_len.
  // This means that position and count get updated
  // even when overflow occurs.
  update_position(s, len);
}

char* stringStream::as_string() {
  char* copy = NEW_RESOURCE_ARRAY(char, buffer_pos + 1);
  strncpy(copy, buffer, buffer_pos);
  copy[buffer_pos] = 0;  // terminating null
  return copy;
}

stringStream::~stringStream() {}

outputStream* tty;
outputStream* gclog_or_tty;
CDS_ONLY(fileStream* classlist_file;) // Only dump the classes that can be stored into the CDS archive
extern Mutex* tty_lock;

#define EXTRACHARLEN   32
#define CURRENTAPPX    ".current"
// convert YYYY-MM-DD HH:MM:SS to YYYY-MM-DD_HH-MM-SS
char* get_datetime_string(char *buf, size_t len) {
  os::local_time_string(buf, len);
  int i = (int)strlen(buf);
  while (i-- >= 0) {
    if (buf[i] == ' ') buf[i] = '_';
    else if (buf[i] == ':') buf[i] = '-';
  }
  return buf;
}

static const char* make_log_name_internal(const char* log_name, const char* force_directory,
                                                int pid, const char* tms) {
  const char* basename = log_name;
  char file_sep = os::file_separator()[0];
  const char* cp;
  char  pid_text[32];

  for (cp = log_name; *cp != '\0'; cp++) {
    if (*cp == '/' || *cp == file_sep) {
      basename = cp + 1;
    }
  }
  const char* nametail = log_name;
  // Compute buffer length
  size_t buffer_length;
  if (force_directory != NULL) {
    buffer_length = strlen(force_directory) + strlen(os::file_separator()) +
                    strlen(basename) + 1;
  } else {
    buffer_length = strlen(log_name) + 1;
  }

  const char* pts = strstr(basename, "%p");
  int pid_pos = (pts == NULL) ? -1 : (pts - nametail);

  if (pid_pos >= 0) {
    jio_snprintf(pid_text, sizeof(pid_text), "pid%u", pid);
    buffer_length += strlen(pid_text);
  }

  pts = strstr(basename, "%t");
  int tms_pos = (pts == NULL) ? -1 : (pts - nametail);
  if (tms_pos >= 0) {
    buffer_length += strlen(tms);
  }

  // File name is too long.
  if (buffer_length > JVM_MAXPATHLEN) {
    return NULL;
  }

  // Create big enough buffer.
  char *buf = NEW_C_HEAP_ARRAY(char, buffer_length, mtInternal);

  strcpy(buf, "");
  if (force_directory != NULL) {
    strcat(buf, force_directory);
    strcat(buf, os::file_separator());
    nametail = basename;       // completely skip directory prefix
  }

  // who is first, %p or %t?
  int first = -1, second = -1;
  const char *p1st = NULL;
  const char *p2nd = NULL;

  if (pid_pos >= 0 && tms_pos >= 0) {
    // contains both %p and %t
    if (pid_pos < tms_pos) {
      // case foo%pbar%tmonkey.log
      first  = pid_pos;
      p1st   = pid_text;
      second = tms_pos;
      p2nd   = tms;
    } else {
      // case foo%tbar%pmonkey.log
      first  = tms_pos;
      p1st   = tms;
      second = pid_pos;
      p2nd   = pid_text;
    }
  } else if (pid_pos >= 0) {
    // contains %p only
    first  = pid_pos;
    p1st   = pid_text;
  } else if (tms_pos >= 0) {
    // contains %t only
    first  = tms_pos;
    p1st   = tms;
  }

  int buf_pos = (int)strlen(buf);
  const char* tail = nametail;

  if (first >= 0) {
    tail = nametail + first + 2;
    strncpy(&buf[buf_pos], nametail, first);
    strcpy(&buf[buf_pos + first], p1st);
    buf_pos = (int)strlen(buf);
    if (second >= 0) {
      strncpy(&buf[buf_pos], tail, second - first - 2);
      strcpy(&buf[buf_pos + second - first - 2], p2nd);
      tail = nametail + second + 2;
    }
  }
  strcat(buf, tail);      // append rest of name, or all of name
  return buf;
}

// log_name comes from -XX:LogFile=log_name, -Xloggc:log_name or
// -XX:DumpLoadedClassList=<file_name>
// in log_name, %p => pid1234 and
//              %t => YYYY-MM-DD_HH-MM-SS
static const char* make_log_name(const char* log_name, const char* force_directory) {
  char timestr[32];
  get_datetime_string(timestr, sizeof(timestr));
  return make_log_name_internal(log_name, force_directory, os::current_process_id(),
                                timestr);
}

#ifndef PRODUCT
void test_loggc_filename() {
  int pid;
  char  tms[32];
  char  i_result[JVM_MAXPATHLEN];
  const char* o_result;
  get_datetime_string(tms, sizeof(tms));
  pid = os::current_process_id();

  // test.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "test.log", tms);
  o_result = make_log_name_internal("test.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"test.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  // test-%t-%p.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "test-%s-pid%u.log", tms, pid);
  o_result = make_log_name_internal("test-%t-%p.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"test-%%t-%%p.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  // test-%t%p.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "test-%spid%u.log", tms, pid);
  o_result = make_log_name_internal("test-%t%p.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"test-%%t%%p.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  // %p%t.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "pid%u%s.log", pid, tms);
  o_result = make_log_name_internal("%p%t.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"%%p%%t.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  // %p-test.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "pid%u-test.log", pid);
  o_result = make_log_name_internal("%p-test.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"%%p-test.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  // %t.log
  jio_snprintf(i_result, JVM_MAXPATHLEN, "%s.log", tms);
  o_result = make_log_name_internal("%t.log", NULL, pid, tms);
  assert(strcmp(i_result, o_result) == 0, "failed on testing make_log_name(\"%%t.log\", NULL)");
  FREE_C_HEAP_ARRAY(char, o_result, mtInternal);

  {
    // longest filename
    char longest_name[JVM_MAXPATHLEN];
    memset(longest_name, 'a', sizeof(longest_name));
    longest_name[JVM_MAXPATHLEN - 1] = '\0';
    o_result = make_log_name_internal((const char*)&longest_name, NULL, pid, tms);
    assert(strcmp(longest_name, o_result) == 0, err_msg("longest name does not match. expected '%s' but got '%s'", longest_name, o_result));
    FREE_C_HEAP_ARRAY(char, o_result, mtInternal);
  }

  {
    // too long file name
    char too_long_name[JVM_MAXPATHLEN + 100];
    int too_long_length = sizeof(too_long_name);
    memset(too_long_name, 'a', too_long_length);
    too_long_name[too_long_length - 1] = '\0';
    o_result = make_log_name_internal((const char*)&too_long_name, NULL, pid, tms);
    assert(o_result == NULL, err_msg("Too long file name should return NULL, but got '%s'", o_result));
  }

  {
    // too long with timestamp
    char longest_name[JVM_MAXPATHLEN];
    memset(longest_name, 'a', JVM_MAXPATHLEN);
    longest_name[JVM_MAXPATHLEN - 3] = '%';
    longest_name[JVM_MAXPATHLEN - 2] = 't';
    longest_name[JVM_MAXPATHLEN - 1] = '\0';
    o_result = make_log_name_internal((const char*)&longest_name, NULL, pid, tms);
    assert(o_result == NULL, err_msg("Too long file name after timestamp expansion should return NULL, but got '%s'", o_result));
  }

  {
    // too long with pid
    char longest_name[JVM_MAXPATHLEN];
    memset(longest_name, 'a', JVM_MAXPATHLEN);
    longest_name[JVM_MAXPATHLEN - 3] = '%';
    longest_name[JVM_MAXPATHLEN - 2] = 'p';
    longest_name[JVM_MAXPATHLEN - 1] = '\0';
    o_result = make_log_name_internal((const char*)&longest_name, NULL, pid, tms);
    assert(o_result == NULL, err_msg("Too long file name after pid expansion should return NULL, but got '%s'", o_result));
  }
}
#endif // PRODUCT

fileStream::fileStream(const char* file_name) {

}

fileStream::fileStream(const char* file_name, const char* opentype) {

}

void fileStream::write(const char* s, size_t len) {
  if (_file != NULL)  {
    // Make an unused local variable to avoid warning from gcc 4.x compiler.
    size_t count = fwrite(s, 1, len, _file);
  }
  update_position(s, len);
}

long fileStream::fileSize() {
  long size = -1;
  if (_file != NULL) {
    long pos  = ::ftell(_file);
    if (::fseek(_file, 0, SEEK_END) == 0) {
      size = ::ftell(_file);
    }
    ::fseek(_file, pos, SEEK_SET);
  }
  return size;
}

char* fileStream::readln(char *data, int count ) {
  char * ret = ::fgets(data, count, _file);
  //Get rid of annoying \n char
  data[::strlen(data)-1] = '\0';
  return ret;
}

fileStream::~fileStream() {
  if (_file != NULL) {
    if (_need_close) fclose(_file);
    _file      = NULL;
  }
}

void fileStream::flush() {
  fflush(_file);
}

fdStream::fdStream(const char* file_name) {
  _fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  _need_close = true;
}

fdStream::~fdStream() {
  if (_fd != -1) {
    if (_need_close) close(_fd);
    _fd = -1;
  }
}

void fdStream::write(const char* s, size_t len) {
  if (_fd != -1) {
    // Make an unused local variable to avoid warning from gcc 4.x compiler.
    size_t count = ::write(_fd, s, (int)len);
  }
  update_position(s, len);
}

// dump vm version, os version, platform info, build id,
// memory usage and command line flags into header
void gcLogFileStream::dump_loggc_header() {
  if (is_open()) {
    print_cr("%s", Abstract_VM_Version::internal_vm_info_string());
    os::print_memory_info(this);
    print("CommandLine flags: ");
    CommandLineFlags::printSetFlags(this);
  }
}

gcLogFileStream::~gcLogFileStream() {
  if (_file != NULL) {
    if (_need_close) fclose(_file);
    _file = NULL;
  }
  if (_file_name != NULL) {
    FREE_C_HEAP_ARRAY(char, _file_name, mtInternal);
    _file_name = NULL;
  }
}

gcLogFileStream::gcLogFileStream(const char* file_name) {

}

void gcLogFileStream::write(const char* s, size_t len) {
  if (_file != NULL) {
    size_t count = fwrite(s, 1, len, _file);
    _bytes_written += count;
  }
  update_position(s, len);
}

// rotate_log must be called from VMThread at safepoint. In case need change parameters
// for gc log rotation from thread other than VMThread, a sub type of VM_Operation
// should be created and be submitted to VMThread's operation queue. DO NOT call this
// function directly. Currently, it is safe to rotate log at safepoint through VMThread.
// That is, no mutator threads and concurrent GC threads run parallel with VMThread to
// write to gc log file at safepoint. If in future, changes made for mutator threads or
// concurrent GC threads to run parallel with VMThread at safepoint, write and rotate_log
// must be synchronized.
void gcLogFileStream::rotate_log(bool force, outputStream* out) {
  //
}

#define LOG_MAJOR_VERSION 160
#define LOG_MINOR_VERSION 1

// Yuck:  jio_print does not accept char*/len.
static void call_jio_print(const char* s, size_t len) {
  char buffer[O_BUFLEN+100];
  if (len > sizeof(buffer)-1) {
    warning("increase O_BUFLEN in ostream.cpp -- output truncated");
    len = sizeof(buffer)-1;
  }
  strncpy(buffer, s, len);
  buffer[len] = '\0';
  jio_print(buffer);
}

intx ttyLocker::hold_tty() {
  D_WARN_Unimplement;
}

void ttyLocker::release_tty(intx holder) {
  D_WARN_Unimplement;
}

bool ttyLocker::release_tty_if_locked() {
  D_WARN_Unimplement;
}

void ttyLocker::break_tty_lock_for_safepoint(intx holder) {
  D_WARN_Unimplement;
}

void ostream_init() {
  D_WARN_Unimplement;
}

void ostream_init_log() {
  D_WARN_Unimplement;
}

// ostream_exit() is called during normal VM exit to finish log files, flush
// output and free resource.
void ostream_exit() {
  D_WARN_Unimplement;
}

// ostream_abort() is called by os::abort() when VM is about to die.
void ostream_abort() {
  D_WARN_Unimplement;
}

staticBufferStream::staticBufferStream(char* buffer, size_t buflen,
                                       outputStream *outer_stream) {
  D_WARN_Unimplement;
}

void staticBufferStream::write(const char* c, size_t len) {
  _outer_stream->print_raw(c, (int)len);
}

void staticBufferStream::flush() {
  _outer_stream->flush();
}

void staticBufferStream::print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, ap, false, len);
  write(str, len);
  va_end(ap);
}

void staticBufferStream::print_cr(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, ap, true, len);
  write(str, len);
  va_end(ap);
}

void staticBufferStream::vprint(const char *format, va_list argptr) {
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, argptr, false, len);
  write(str, len);
}

void staticBufferStream::vprint_cr(const char* format, va_list argptr) {
  size_t len;
  const char* str = do_vsnprintf(_buffer, _buflen, format, argptr, true, len);
  write(str, len);
}

bufferedStream::bufferedStream(size_t initial_size, size_t bufmax) : outputStream() {
  buffer_length = initial_size;
  buffer        = NEW_C_HEAP_ARRAY(char, buffer_length, mtInternal);
  buffer_pos    = 0;
  buffer_fixed  = false;
  buffer_max    = bufmax;
}

bufferedStream::bufferedStream(char* fixed_buffer, size_t fixed_buffer_size, size_t bufmax) : outputStream() {
  buffer_length = fixed_buffer_size;
  buffer        = fixed_buffer;
  buffer_pos    = 0;
  buffer_fixed  = true;
  buffer_max    = bufmax;
}

void bufferedStream::write(const char* s, size_t len) {

  if(buffer_pos + len > buffer_max) {
    flush();
  }

  size_t end = buffer_pos + len;
  if (end >= buffer_length) {
    if (buffer_fixed) {
      // if buffer cannot resize, silently truncate
      len = buffer_length - buffer_pos - 1;
    } else {
      // For small overruns, double the buffer.  For larger ones,
      // increase to the requested size.
      if (end < buffer_length * 2) {
        end = buffer_length * 2;
      }
      buffer = REALLOC_C_HEAP_ARRAY(char, buffer, end, mtInternal);
      buffer_length = end;
    }
  }
  memcpy(buffer + buffer_pos, s, len);
  buffer_pos += len;
  update_position(s, len);
}

char* bufferedStream::as_string() {
  char* copy = NEW_RESOURCE_ARRAY(char, buffer_pos+1);
  strncpy(copy, buffer, buffer_pos);
  copy[buffer_pos] = 0;  // terminating null
  return copy;
}

bufferedStream::~bufferedStream() {
  if (!buffer_fixed) {
    FREE_C_HEAP_ARRAY(char, buffer, mtInternal);
  }
}

#ifndef PRODUCT

#if defined(SOLARIS) || defined(LINUX) || defined(AIX) || defined(_ALLBSD_SOURCE)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Network access
networkStream::networkStream() : bufferedStream(1024*10, 1024*10) {

  _socket = -1;

  int result = os::socket(AF_INET, SOCK_STREAM, 0);
  if (result <= 0) {
    assert(false, "Socket could not be created!");
  } else {
    _socket = result;
  }
}

int networkStream::read(char *buf, size_t len) {
  return os::recv(_socket, buf, (int)len, 0);
}

void networkStream::flush() {
  if (size() != 0) {
    int result = os::raw_send(_socket, (char *)base(), size(), 0);
    assert(result != -1, "connection error");
    assert(result == (int)size(), "didn't send enough data");
  }
  reset();
}

networkStream::~networkStream() {
  close();
}

void networkStream::close() {
  if (_socket != -1) {
    flush();
    os::socket_close(_socket);
    _socket = -1;
  }
}

bool networkStream::connect(const char *ip, short port) {

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  server.sin_addr.s_addr = inet_addr(ip);
  if (server.sin_addr.s_addr == (uint32_t)-1) {
    struct hostent* host = os::get_host_by_name((char*)ip);
    if (host != NULL) {
      memcpy(&server.sin_addr, host->h_addr_list[0], host->h_length);
    } else {
      return false;
    }
  }


  int result = os::connect(_socket, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
  return (result >= 0);
}

#endif
