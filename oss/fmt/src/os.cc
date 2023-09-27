// Formatting library for C++ - optional OS-specific functionality
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

// Disable bogus MSVC warnings.
#if !defined(_CRT_SECURE_NO_WARNINGS) && defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#include "fmt/os.h"

#include <climits>

#if FMT_USE_FCNTL
#  include <sys/stat.h>
#  include <sys/types.h>

#  ifdef _WRS_KERNEL   // VxWorks7 kernel
#    include <ioLib.h> // getpagesize
#  endif

#  ifndef _WIN32
#    include <unistd.h>
#  else
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    include <io.h>

#    ifndef S_IRUSR
#      define S_IRUSR _S_IREAD
#    endif
#    ifndef S_IWUSR
#      define S_IWUSR _S_IWRITE
#    endif
#    ifndef S_IRGRP
#      define S_IRGRP 0
#    endif
#    ifndef S_IWGRP
#      define S_IWGRP 0
#    endif
#    ifndef S_IROTH
#      define S_IROTH 0
#    endif
#    ifndef S_IWOTH
#      define S_IWOTH 0
#    endif
#  endif  // _WIN32
#endif    // FMT_USE_FCNTL

#ifdef _WIN32
#  include <windows.h>
#endif

namespace {
#ifdef _WIN32
// Return type of read and write functions.
using rwresult = int;

// On Windows the count argument to read and write is unsigned, so convert
// it from size_t preventing integer overflow.
inline unsigned convert_rwcount(std::size_t count) {
  return count <= UINT_MAX ? static_cast<unsigned>(count) : UINT_MAX;
}
#elif FMT_USE_FCNTL
// Return type of read and write functions.
using rwresult = ssize_t;

inline std::size_t convert_rwcount(std::size_t count) { return count; }
#endif
}  // namespace

FMT_BEGIN_NAMESPACE

#ifdef _WIN32
namespace detail {

class system_message {
  system_message(const system_message&) = delete;
  void operator=(const system_message&) = delete;

  unsigned long result_;
  wchar_t* message_;

  static bool is_whitespace(wchar_t c) noexcept {
    return c == L' ' || c == L'\n' || c == L'\r' || c == L'\t' || c == L'\0';
  }

 public:
  explicit system_message(unsigned long error_code)
      : result_(0), message_(nullptr) {
    result_ = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&message_), 0, nullptr);
    if (result_ != 0) {
      while (result_ != 0 && is_whitespace(message_[result_ - 1])) {
        --result_;
      }
    }
  }
  ~system_message() { LocalFree(message_); }
  explicit operator bool() const noexcept { return result_ != 0; }
  operator basic_string_view<wchar_t>() const noexcept {
    return basic_string_view<wchar_t>(message_, result_);
  }
};

class utf8_system_category final : public std::error_category {
 public:
  const char* name() const noexcept override { return "system"; }
  std::string message(int error_code) const override {
    auto&& msg = system_message(error_code);
    if (msg) {
      auto utf8_message = to_utf8<wchar_t>();
      if (utf8_message.convert(msg)) {
        return utf8_message.str();
      }
    }
    return "unknown error";
  }
};

}  // namespace detail

FMT_API const std::error_category& system_category() noexcept {
  static const detail::utf8_system_category category;
  return category;
}

std::system_error vwindows_error(int err_code, string_view format_str,
                                 format_args args) {
  auto ec = std::error_code(err_code, system_category());
  return std::system_error(ec, vformat(format_str, args));
}

void detail::format_windows_error(detail::buffer<char>& out, int error_code,
                                  const char* message) noexcept {
  FMT_TRY {
    auto&& msg = system_message(error_code);
    if (msg) {
      auto utf8_message = to_utf8<wchar_t>();
      if (utf8_message.convert(msg)) {
        fmt::format_to(appender(out), FMT_STRING("{}: {}"), message,
                       string_view(utf8_message));
        return;
      }
    }
  }
  FMT_CATCH(...) {}
  format_error_code(out, error_code, message);
}

void report_windows_error(int error_code, const char* message) noexcept {
  report_error(detail::format_windows_error, error_code, message);
}
#endif  // _WIN32

buffered_file::~buffered_file() noexcept {
  if (file_ && FMT_SYSTEM(fclose(file_)) != 0)
    report_system_error(errno, "cannot close file");
}

buffered_file::buffered_file(cstring_view filename, cstring_view mode) {
  FMT_RETRY_VAL(file_, FMT_SYSTEM(fopen(filename.c_str(), mode.c_str())),
                nullptr);
  if (!file_)
    FMT_THROW(system_error(errno, FMT_STRING("cannot open file {}"),
                           filename.c_str()));
}

void buffered_file::close() {
  if (!file_) return;
  int result = FMT_SYSTEM(fclose(file_));
  file_ = nullptr;
  if (result != 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot close file")));
}

int buffered_file::descriptor() const {
#ifdef fileno  // fileno is a macro on OpenBSD so we cannot use FMT_POSIX_CALL.
  int fd = fileno(file_);
#else
  int fd = FMT_POSIX_CALL(fileno(file_));
#endif
  if (fd == -1)
    FMT_THROW(system_error(errno, FMT_STRING("cannot get file descriptor")));
  return fd;
}

#if FMT_USE_FCNTL
#  ifdef _WIN32
using mode_t = int;
#  endif
constexpr mode_t default_open_mode =
    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

file::file(cstring_view path, int oflag) {
#  if defined(_WIN32) && !defined(__MINGW32__)
  fd_ = -1;
  auto converted = detail::utf8_to_utf16(string_view(path.c_str()));
  *this = file::open_windows_file(converted.c_str(), oflag);
#  else
  FMT_RETRY(fd_, FMT_POSIX_CALL(open(path.c_str(), oflag, default_open_mode)));
  if (fd_ == -1)
    FMT_THROW(
        system_error(errno, FMT_STRING("cannot open file {}"), path.c_str()));
#  endif
}

file::~file() noexcept {
  // Don't retry close in case of EINTR!
  // See http://linux.derkeiler.com/Mailing-Lists/Kernel/2005-09/3000.html
  if (fd_ != -1 && FMT_POSIX_CALL(close(fd_)) != 0)
    report_system_error(errno, "cannot close file");
}

void file::close() {
  if (fd_ == -1) return;
  // Don't retry close in case of EINTR!
  // See http://linux.derkeiler.com/Mailing-Lists/Kernel/2005-09/3000.html
  int result = FMT_POSIX_CALL(close(fd_));
  fd_ = -1;
  if (result != 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot close file")));
}

long long file::size() const {
#  ifdef _WIN32
  // Use GetFileSize instead of GetFileSizeEx for the case when _WIN32_WINNT
  // is less than 0x0500 as is the case with some default MinGW builds.
  // Both functions support large file sizes.
  DWORD size_upper = 0;
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd_));
  DWORD size_lower = FMT_SYSTEM(GetFileSize(handle, &size_upper));
  if (size_lower == INVALID_FILE_SIZE) {
    DWORD error = GetLastError();
    if (error != NO_ERROR)
      FMT_THROW(windows_error(GetLastError(), "cannot get file size"));
  }
  unsigned long long long_size = size_upper;
  return (long_size << sizeof(DWORD) * CHAR_BIT) | size_lower;
#  else
  using Stat = struct stat;
  Stat file_stat = Stat();
  if (FMT_POSIX_CALL(fstat(fd_, &file_stat)) == -1)
    FMT_THROW(system_error(errno, FMT_STRING("cannot get file attributes")));
  static_assert(sizeof(long long) >= sizeof(file_stat.st_size),
                "return type of file::size is not large enough");
  return file_stat.st_size;
#  endif
}

std::size_t file::read(void* buffer, std::size_t count) {
  rwresult result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(read(fd_, buffer, convert_rwcount(count))));
  if (result < 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot read from file")));
  return detail::to_unsigned(result);
}

std::size_t file::write(const void* buffer, std::size_t count) {
  rwresult result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(write(fd_, buffer, convert_rwcount(count))));
  if (result < 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot write to file")));
  return detail::to_unsigned(result);
}

file file::dup(int fd) {
  // Don't retry as dup doesn't return EINTR.
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/dup.html
  int new_fd = FMT_POSIX_CALL(dup(fd));
  if (new_fd == -1)
    FMT_THROW(system_error(
        errno, FMT_STRING("cannot duplicate file descriptor {}"), fd));
  return file(new_fd);
}

void file::dup2(int fd) {
  int result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(dup2(fd_, fd)));
  if (result == -1) {
    FMT_THROW(system_error(
        errno, FMT_STRING("cannot duplicate file descriptor {} to {}"), fd_,
        fd));
  }
}

void file::dup2(int fd, std::error_code& ec) noexcept {
  int result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(dup2(fd_, fd)));
  if (result == -1) ec = std::error_code(errno, std::generic_category());
}

void file::pipe(file& read_end, file& write_end) {
  // Close the descriptors first to make sure that assignments don't throw
  // and there are no leaks.
  read_end.close();
  write_end.close();
  int fds[2] = {};
#  ifdef _WIN32
  // Make the default pipe capacity same as on Linux 2.6.11+.
  enum { DEFAULT_CAPACITY = 65536 };
  int result = FMT_POSIX_CALL(pipe(fds, DEFAULT_CAPACITY, _O_BINARY));
#  else
  // Don't retry as the pipe function doesn't return EINTR.
  // http://pubs.opengroup.org/onlinepubs/009696799/functions/pipe.html
  int result = FMT_POSIX_CALL(pipe(fds));
#  endif
  if (result != 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot create pipe")));
  // The following assignments don't throw because read_fd and write_fd
  // are closed.
  read_end = file(fds[0]);
  write_end = file(fds[1]);
}

buffered_file file::fdopen(const char* mode) {
// Don't retry as fdopen doesn't return EINTR.
#  if defined(__MINGW32__) && defined(_POSIX_)
  FILE* f = ::fdopen(fd_, mode);
#  else
  FILE* f = FMT_POSIX_CALL(fdopen(fd_, mode));
#  endif
  if (!f) {
    FMT_THROW(system_error(
        errno, FMT_STRING("cannot associate stream with file descriptor")));
  }
  buffered_file bf(f);
  fd_ = -1;
  return bf;
}

#  if defined(_WIN32) && !defined(__MINGW32__)
file file::open_windows_file(wcstring_view path, int oflag) {
  int fd = -1;
  auto err = _wsopen_s(&fd, path.c_str(), oflag, _SH_DENYNO, default_open_mode);
  if (fd == -1) {
    FMT_THROW(system_error(err, FMT_STRING("cannot open file {}"),
                           detail::to_utf8<wchar_t>(path.c_str()).c_str()));
  }
  return file(fd);
}
#  endif

#  if !defined(__MSDOS__)
long getpagesize() {
#    ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#    else
#      ifdef _WRS_KERNEL
  long size = FMT_POSIX_CALL(getpagesize());
#      else
  long size = FMT_POSIX_CALL(sysconf(_SC_PAGESIZE));
#      endif

  if (size < 0)
    FMT_THROW(system_error(errno, FMT_STRING("cannot get memory page size")));
  return size;
#    endif
}
#  endif

namespace detail {

void file_buffer::grow(size_t) {
  if (this->size() == this->capacity()) flush();
}

file_buffer::file_buffer(cstring_view path,
                         const detail::ostream_params& params)
    : file_(path, params.oflag) {
  set(new char[params.buffer_size], params.buffer_size);
}

file_buffer::file_buffer(file_buffer&& other)
    : detail::buffer<char>(other.data(), other.size(), other.capacity()),
      file_(std::move(other.file_)) {
  other.clear();
  other.set(nullptr, 0);
}

file_buffer::~file_buffer() {
  flush();
  delete[] data();
}
}  // namespace detail

ostream::~ostream() = default;
#endif  // FMT_USE_FCNTL
FMT_END_NAMESPACE
