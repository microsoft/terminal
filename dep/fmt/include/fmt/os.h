// Formatting library for C++ - optional OS-specific functionality
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_OS_H_
#define FMT_OS_H_

#if defined(__MINGW32__) || defined(__CYGWIN__)
// Workaround MinGW bug https://sourceforge.net/p/mingw/bugs/2024/.
#  undef __STRICT_ANSI__
#endif

#include <cerrno>
#include <clocale>  // for locale_t
#include <cstddef>
#include <cstdio>
#include <cstdlib>  // for strtod_l

#if defined __APPLE__ || defined(__FreeBSD__)
#  include <xlocale.h>  // for LC_NUMERIC_MASK on OS X
#endif

#include "format.h"

// UWP doesn't provide _pipe.
#if FMT_HAS_INCLUDE("winapifamily.h")
#  include <winapifamily.h>
#endif
#if FMT_HAS_INCLUDE("fcntl.h") && \
    (!defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP))
#  include <fcntl.h>  // for O_RDONLY
#  define FMT_USE_FCNTL 1
#else
#  define FMT_USE_FCNTL 0
#endif

#ifndef FMT_POSIX
#  if defined(_WIN32) && !defined(__MINGW32__)
// Fix warnings about deprecated symbols.
#    define FMT_POSIX(call) _##call
#  else
#    define FMT_POSIX(call) call
#  endif
#endif

// Calls to system functions are wrapped in FMT_SYSTEM for testability.
#ifdef FMT_SYSTEM
#  define FMT_POSIX_CALL(call) FMT_SYSTEM(call)
#else
#  define FMT_SYSTEM(call) call
#  ifdef _WIN32
// Fix warnings about deprecated symbols.
#    define FMT_POSIX_CALL(call) ::_##call
#  else
#    define FMT_POSIX_CALL(call) ::call
#  endif
#endif

// Retries the expression while it evaluates to error_result and errno
// equals to EINTR.
#ifndef _WIN32
#  define FMT_RETRY_VAL(result, expression, error_result) \
    do {                                                  \
      (result) = (expression);                            \
    } while ((result) == (error_result) && errno == EINTR)
#else
#  define FMT_RETRY_VAL(result, expression, error_result) result = (expression)
#endif

#define FMT_RETRY(result, expression) FMT_RETRY_VAL(result, expression, -1)

FMT_BEGIN_NAMESPACE

/**
  \rst
  A reference to a null-terminated string. It can be constructed from a C
  string or ``std::string``.

  You can use one of the following type aliases for common character types:

  +---------------+-----------------------------+
  | Type          | Definition                  |
  +===============+=============================+
  | cstring_view  | basic_cstring_view<char>    |
  +---------------+-----------------------------+
  | wcstring_view | basic_cstring_view<wchar_t> |
  +---------------+-----------------------------+

  This class is most useful as a parameter type to allow passing
  different types of strings to a function, for example::

    template <typename... Args>
    std::string format(cstring_view format_str, const Args & ... args);

    format("{}", 42);
    format(std::string("{}"), 42);
  \endrst
 */
template <typename Char> class basic_cstring_view {
 private:
  const Char* data_;

 public:
  /** Constructs a string reference object from a C string. */
  basic_cstring_view(const Char* s) : data_(s) {}

  /**
    \rst
    Constructs a string reference from an ``std::string`` object.
    \endrst
   */
  basic_cstring_view(const std::basic_string<Char>& s) : data_(s.c_str()) {}

  /** Returns the pointer to a C string. */
  const Char* c_str() const { return data_; }
};

using cstring_view = basic_cstring_view<char>;
using wcstring_view = basic_cstring_view<wchar_t>;

// An error code.
class error_code {
 private:
  int value_;

 public:
  explicit error_code(int value = 0) FMT_NOEXCEPT : value_(value) {}

  int get() const FMT_NOEXCEPT { return value_; }
};

#ifdef _WIN32
namespace internal {
// A converter from UTF-16 to UTF-8.
// It is only provided for Windows since other systems support UTF-8 natively.
class utf16_to_utf8 {
 private:
  memory_buffer buffer_;

 public:
  utf16_to_utf8() {}
  FMT_API explicit utf16_to_utf8(wstring_view s);
  operator string_view() const { return string_view(&buffer_[0], size()); }
  size_t size() const { return buffer_.size() - 1; }
  const char* c_str() const { return &buffer_[0]; }
  std::string str() const { return std::string(&buffer_[0], size()); }

  // Performs conversion returning a system error code instead of
  // throwing exception on conversion error. This method may still throw
  // in case of memory allocation error.
  FMT_API int convert(wstring_view s);
};

FMT_API void format_windows_error(buffer<char>& out, int error_code,
                                  string_view message) FMT_NOEXCEPT;
}  // namespace internal

/** A Windows error. */
class windows_error : public system_error {
 private:
  FMT_API void init(int error_code, string_view format_str, format_args args);

 public:
  /**
   \rst
   Constructs a :class:`fmt::windows_error` object with the description
   of the form

   .. parsed-literal::
     *<message>*: *<system-message>*

   where *<message>* is the formatted message and *<system-message>* is the
   system message corresponding to the error code.
   *error_code* is a Windows error code as given by ``GetLastError``.
   If *error_code* is not a valid error code such as -1, the system message
   will look like "error -1".

   **Example**::

     // This throws a windows_error with the description
     //   cannot open file 'madeup': The system cannot find the file specified.
     // or similar (system message may vary).
     const char *filename = "madeup";
     LPOFSTRUCT of = LPOFSTRUCT();
     HFILE file = OpenFile(filename, &of, OF_READ);
     if (file == HFILE_ERROR) {
       throw fmt::windows_error(GetLastError(),
                                "cannot open file '{}'", filename);
     }
   \endrst
  */
  template <typename... Args>
  windows_error(int error_code, string_view message, const Args&... args) {
    init(error_code, message, make_format_args(args...));
  }
};

// Reports a Windows error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_windows_error(int error_code,
                                  string_view message) FMT_NOEXCEPT;
#endif  // _WIN32

// A buffered file.
class buffered_file {
 private:
  FILE* file_;

  friend class file;

  explicit buffered_file(FILE* f) : file_(f) {}

 public:
  buffered_file(const buffered_file&) = delete;
  void operator=(const buffered_file&) = delete;

  // Constructs a buffered_file object which doesn't represent any file.
  buffered_file() FMT_NOEXCEPT : file_(nullptr) {}

  // Destroys the object closing the file it represents if any.
  FMT_API ~buffered_file() FMT_NOEXCEPT;

 public:
  buffered_file(buffered_file&& other) FMT_NOEXCEPT : file_(other.file_) {
    other.file_ = nullptr;
  }

  buffered_file& operator=(buffered_file&& other) {
    close();
    file_ = other.file_;
    other.file_ = nullptr;
    return *this;
  }

  // Opens a file.
  FMT_API buffered_file(cstring_view filename, cstring_view mode);

  // Closes the file.
  FMT_API void close();

  // Returns the pointer to a FILE object representing this file.
  FILE* get() const FMT_NOEXCEPT { return file_; }

  // We place parentheses around fileno to workaround a bug in some versions
  // of MinGW that define fileno as a macro.
  FMT_API int(fileno)() const;

  void vprint(string_view format_str, format_args args) {
    fmt::vprint(file_, format_str, args);
  }

  template <typename... Args>
  inline void print(string_view format_str, const Args&... args) {
    vprint(format_str, make_format_args(args...));
  }
};

#if FMT_USE_FCNTL
// A file. Closed file is represented by a file object with descriptor -1.
// Methods that are not declared with FMT_NOEXCEPT may throw
// fmt::system_error in case of failure. Note that some errors such as
// closing the file multiple times will cause a crash on Windows rather
// than an exception. You can get standard behavior by overriding the
// invalid parameter handler with _set_invalid_parameter_handler.
class file {
 private:
  int fd_;  // File descriptor.

  // Constructs a file object with a given descriptor.
  explicit file(int fd) : fd_(fd) {}

 public:
  // Possible values for the oflag argument to the constructor.
  enum {
    RDONLY = FMT_POSIX(O_RDONLY),  // Open for reading only.
    WRONLY = FMT_POSIX(O_WRONLY),  // Open for writing only.
    RDWR = FMT_POSIX(O_RDWR)       // Open for reading and writing.
  };

  // Constructs a file object which doesn't represent any file.
  file() FMT_NOEXCEPT : fd_(-1) {}

  // Opens a file and constructs a file object representing this file.
  FMT_API file(cstring_view path, int oflag);

 public:
  file(const file&) = delete;
  void operator=(const file&) = delete;

  file(file&& other) FMT_NOEXCEPT : fd_(other.fd_) { other.fd_ = -1; }

  file& operator=(file&& other) FMT_NOEXCEPT {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
  }

  // Destroys the object closing the file it represents if any.
  FMT_API ~file() FMT_NOEXCEPT;

  // Returns the file descriptor.
  int descriptor() const FMT_NOEXCEPT { return fd_; }

  // Closes the file.
  FMT_API void close();

  // Returns the file size. The size has signed type for consistency with
  // stat::st_size.
  FMT_API long long size() const;

  // Attempts to read count bytes from the file into the specified buffer.
  FMT_API std::size_t read(void* buffer, std::size_t count);

  // Attempts to write count bytes from the specified buffer to the file.
  FMT_API std::size_t write(const void* buffer, std::size_t count);

  // Duplicates a file descriptor with the dup function and returns
  // the duplicate as a file object.
  FMT_API static file dup(int fd);

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  FMT_API void dup2(int fd);

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  FMT_API void dup2(int fd, error_code& ec) FMT_NOEXCEPT;

  // Creates a pipe setting up read_end and write_end file objects for reading
  // and writing respectively.
  FMT_API static void pipe(file& read_end, file& write_end);

  // Creates a buffered_file object associated with this file and detaches
  // this file object from the file.
  FMT_API buffered_file fdopen(const char* mode);
};

// Returns the memory page size.
long getpagesize();
#endif  // FMT_USE_FCNTL

#ifdef FMT_LOCALE
// A "C" numeric locale.
class locale {
 private:
#  ifdef _WIN32
  using locale_t = _locale_t;

  static void freelocale(locale_t loc) { _free_locale(loc); }

  static double strtod_l(const char* nptr, char** endptr, _locale_t loc) {
    return _strtod_l(nptr, endptr, loc);
  }
#  endif

  locale_t locale_;

 public:
  using type = locale_t;
  locale(const locale&) = delete;
  void operator=(const locale&) = delete;

  locale() {
#  ifndef _WIN32
    locale_ = FMT_SYSTEM(newlocale(LC_NUMERIC_MASK, "C", nullptr));
#  else
    locale_ = _create_locale(LC_NUMERIC, "C");
#  endif
    if (!locale_) FMT_THROW(system_error(errno, "cannot create locale"));
  }
  ~locale() { freelocale(locale_); }

  type get() const { return locale_; }

  // Converts string to floating-point number and advances str past the end
  // of the parsed input.
  double strtod(const char*& str) const {
    char* end = nullptr;
    double result = strtod_l(str, &end, locale_);
    str = end;
    return result;
  }
};
using Locale FMT_DEPRECATED_ALIAS = locale;
#endif  // FMT_LOCALE
FMT_END_NAMESPACE

#endif  // FMT_OS_H_
