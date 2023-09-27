// Formatting library for C++ - optional OS-specific functionality
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_OS_H_
#define FMT_OS_H_

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <system_error>  // std::system_error

#if defined __APPLE__ || defined(__FreeBSD__)
#  include <xlocale.h>  // for LC_NUMERIC_MASK on OS X
#endif

#include "format.h"

#ifndef FMT_USE_FCNTL
// UWP doesn't provide _pipe.
#  if FMT_HAS_INCLUDE("winapifamily.h")
#    include <winapifamily.h>
#  endif
#  if (FMT_HAS_INCLUDE(<fcntl.h>) || defined(__APPLE__) || \
       defined(__linux__)) &&                              \
      (!defined(WINAPI_FAMILY) ||                          \
       (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP))
#    include <fcntl.h>  // for O_RDONLY
#    define FMT_USE_FCNTL 1
#  else
#    define FMT_USE_FCNTL 0
#  endif
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
#  define FMT_SYSTEM(call) ::call
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
FMT_BEGIN_EXPORT

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

#ifdef _WIN32
FMT_API const std::error_category& system_category() noexcept;

namespace detail {
FMT_API void format_windows_error(buffer<char>& out, int error_code,
                                  const char* message) noexcept;
}

FMT_API std::system_error vwindows_error(int error_code, string_view format_str,
                                         format_args args);

/**
 \rst
 Constructs a :class:`std::system_error` object with the description
 of the form

 .. parsed-literal::
   *<message>*: *<system-message>*

 where *<message>* is the formatted message and *<system-message>* is the
 system message corresponding to the error code.
 *error_code* is a Windows error code as given by ``GetLastError``.
 If *error_code* is not a valid error code such as -1, the system message
 will look like "error -1".

 **Example**::

   // This throws a system_error with the description
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
std::system_error windows_error(int error_code, string_view message,
                                const Args&... args) {
  return vwindows_error(error_code, message, fmt::make_format_args(args...));
}

// Reports a Windows error without throwing an exception.
// Can be used to report errors from destructors.
FMT_API void report_windows_error(int error_code, const char* message) noexcept;
#else
inline const std::error_category& system_category() noexcept {
  return std::system_category();
}
#endif  // _WIN32

// std::system is not available on some platforms such as iOS (#2248).
#ifdef __OSX__
template <typename S, typename... Args, typename Char = char_t<S>>
void say(const S& format_str, Args&&... args) {
  std::system(format("say \"{}\"", format(format_str, args...)).c_str());
}
#endif

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
  buffered_file() noexcept : file_(nullptr) {}

  // Destroys the object closing the file it represents if any.
  FMT_API ~buffered_file() noexcept;

 public:
  buffered_file(buffered_file&& other) noexcept : file_(other.file_) {
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
  FILE* get() const noexcept { return file_; }

  FMT_API int descriptor() const;

  void vprint(string_view format_str, format_args args) {
    fmt::vprint(file_, format_str, args);
  }

  template <typename... Args>
  inline void print(string_view format_str, const Args&... args) {
    vprint(format_str, fmt::make_format_args(args...));
  }
};

#if FMT_USE_FCNTL
// A file. Closed file is represented by a file object with descriptor -1.
// Methods that are not declared with noexcept may throw
// fmt::system_error in case of failure. Note that some errors such as
// closing the file multiple times will cause a crash on Windows rather
// than an exception. You can get standard behavior by overriding the
// invalid parameter handler with _set_invalid_parameter_handler.
class FMT_API file {
 private:
  int fd_;  // File descriptor.

  // Constructs a file object with a given descriptor.
  explicit file(int fd) : fd_(fd) {}

 public:
  // Possible values for the oflag argument to the constructor.
  enum {
    RDONLY = FMT_POSIX(O_RDONLY),  // Open for reading only.
    WRONLY = FMT_POSIX(O_WRONLY),  // Open for writing only.
    RDWR = FMT_POSIX(O_RDWR),      // Open for reading and writing.
    CREATE = FMT_POSIX(O_CREAT),   // Create if the file doesn't exist.
    APPEND = FMT_POSIX(O_APPEND),  // Open in append mode.
    TRUNC = FMT_POSIX(O_TRUNC)     // Truncate the content of the file.
  };

  // Constructs a file object which doesn't represent any file.
  file() noexcept : fd_(-1) {}

  // Opens a file and constructs a file object representing this file.
  file(cstring_view path, int oflag);

 public:
  file(const file&) = delete;
  void operator=(const file&) = delete;

  file(file&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

  // Move assignment is not noexcept because close may throw.
  file& operator=(file&& other) {
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
  }

  // Destroys the object closing the file it represents if any.
  ~file() noexcept;

  // Returns the file descriptor.
  int descriptor() const noexcept { return fd_; }

  // Closes the file.
  void close();

  // Returns the file size. The size has signed type for consistency with
  // stat::st_size.
  long long size() const;

  // Attempts to read count bytes from the file into the specified buffer.
  size_t read(void* buffer, size_t count);

  // Attempts to write count bytes from the specified buffer to the file.
  size_t write(const void* buffer, size_t count);

  // Duplicates a file descriptor with the dup function and returns
  // the duplicate as a file object.
  static file dup(int fd);

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  void dup2(int fd);

  // Makes fd be the copy of this file descriptor, closing fd first if
  // necessary.
  void dup2(int fd, std::error_code& ec) noexcept;

  // Creates a pipe setting up read_end and write_end file objects for reading
  // and writing respectively.
  static void pipe(file& read_end, file& write_end);

  // Creates a buffered_file object associated with this file and detaches
  // this file object from the file.
  buffered_file fdopen(const char* mode);

#  if defined(_WIN32) && !defined(__MINGW32__)
  // Opens a file and constructs a file object representing this file by
  // wcstring_view filename. Windows only.
  static file open_windows_file(wcstring_view path, int oflag);
#  endif
};

// Returns the memory page size.
long getpagesize();

namespace detail {

struct buffer_size {
  buffer_size() = default;
  size_t value = 0;
  buffer_size operator=(size_t val) const {
    auto bs = buffer_size();
    bs.value = val;
    return bs;
  }
};

struct ostream_params {
  int oflag = file::WRONLY | file::CREATE | file::TRUNC;
  size_t buffer_size = BUFSIZ > 32768 ? BUFSIZ : 32768;

  ostream_params() {}

  template <typename... T>
  ostream_params(T... params, int new_oflag) : ostream_params(params...) {
    oflag = new_oflag;
  }

  template <typename... T>
  ostream_params(T... params, detail::buffer_size bs)
      : ostream_params(params...) {
    this->buffer_size = bs.value;
  }

// Intel has a bug that results in failure to deduce a constructor
// for empty parameter packs.
#  if defined(__INTEL_COMPILER) && __INTEL_COMPILER < 2000
  ostream_params(int new_oflag) : oflag(new_oflag) {}
  ostream_params(detail::buffer_size bs) : buffer_size(bs.value) {}
#  endif
};

class file_buffer final : public buffer<char> {
  file file_;

  FMT_API void grow(size_t) override;

 public:
  FMT_API file_buffer(cstring_view path, const ostream_params& params);
  FMT_API file_buffer(file_buffer&& other);
  FMT_API ~file_buffer();

  void flush() {
    if (size() == 0) return;
    file_.write(data(), size() * sizeof(data()[0]));
    clear();
  }

  void close() {
    flush();
    file_.close();
  }
};

}  // namespace detail

// Added {} below to work around default constructor error known to
// occur in Xcode versions 7.2.1 and 8.2.1.
constexpr detail::buffer_size buffer_size{};

/** A fast output stream which is not thread-safe. */
class FMT_API ostream {
 private:
  FMT_MSC_WARNING(suppress : 4251)
  detail::file_buffer buffer_;

  ostream(cstring_view path, const detail::ostream_params& params)
      : buffer_(path, params) {}

 public:
  ostream(ostream&& other) : buffer_(std::move(other.buffer_)) {}

  ~ostream();

  void flush() { buffer_.flush(); }

  template <typename... T>
  friend ostream output_file(cstring_view path, T... params);

  void close() { buffer_.close(); }

  /**
    Formats ``args`` according to specifications in ``fmt`` and writes the
    output to the file.
   */
  template <typename... T> void print(format_string<T...> fmt, T&&... args) {
    vformat_to(detail::buffer_appender<char>(buffer_), fmt,
               fmt::make_format_args(args...));
  }
};

/**
  \rst
  Opens a file for writing. Supported parameters passed in *params*:

  * ``<integer>``: Flags passed to `open
    <https://pubs.opengroup.org/onlinepubs/007904875/functions/open.html>`_
    (``file::WRONLY | file::CREATE | file::TRUNC`` by default)
  * ``buffer_size=<integer>``: Output buffer size

  **Example**::

    auto out = fmt::output_file("guide.txt");
    out.print("Don't {}", "Panic");
  \endrst
 */
template <typename... T>
inline ostream output_file(cstring_view path, T... params) {
  return {path, detail::ostream_params(params...)};
}
#endif  // FMT_USE_FCNTL

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_OS_H_
