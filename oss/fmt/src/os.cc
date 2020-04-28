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

#  ifndef _WIN32
#    include <unistd.h>
#  else
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    include <io.h>
#    include <windows.h>

#    define O_CREAT _O_CREAT
#    define O_TRUNC _O_TRUNC

#    ifndef S_IRUSR
#      define S_IRUSR _S_IREAD
#    endif

#    ifndef S_IWUSR
#      define S_IWUSR _S_IWRITE
#    endif

#    ifdef __MINGW32__
#      define _SH_DENYNO 0x40
#    endif
#  endif  // _WIN32
#endif    // FMT_USE_FCNTL

#ifdef _WIN32
#  include <windows.h>
#endif

#ifdef fileno
#  undef fileno
#endif

namespace {
#ifdef _WIN32
// Return type of read and write functions.
using RWResult = int;

// On Windows the count argument to read and write is unsigned, so convert
// it from size_t preventing integer overflow.
inline unsigned convert_rwcount(std::size_t count) {
  return count <= UINT_MAX ? static_cast<unsigned>(count) : UINT_MAX;
}
#else
// Return type of read and write functions.
using RWResult = ssize_t;

inline std::size_t convert_rwcount(std::size_t count) { return count; }
#endif
}  // namespace

FMT_BEGIN_NAMESPACE

#ifdef _WIN32
internal::utf16_to_utf8::utf16_to_utf8(wstring_view s) {
  if (int error_code = convert(s)) {
    FMT_THROW(windows_error(error_code,
                            "cannot convert string from UTF-16 to UTF-8"));
  }
}

int internal::utf16_to_utf8::convert(wstring_view s) {
  if (s.size() > INT_MAX) return ERROR_INVALID_PARAMETER;
  int s_size = static_cast<int>(s.size());
  if (s_size == 0) {
    // WideCharToMultiByte does not support zero length, handle separately.
    buffer_.resize(1);
    buffer_[0] = 0;
    return 0;
  }

  int length = WideCharToMultiByte(CP_UTF8, 0, s.data(), s_size, nullptr, 0,
                                   nullptr, nullptr);
  if (length == 0) return GetLastError();
  buffer_.resize(length + 1);
  length = WideCharToMultiByte(CP_UTF8, 0, s.data(), s_size, &buffer_[0],
                               length, nullptr, nullptr);
  if (length == 0) return GetLastError();
  buffer_[length] = 0;
  return 0;
}

void windows_error::init(int err_code, string_view format_str,
                         format_args args) {
  error_code_ = err_code;
  memory_buffer buffer;
  internal::format_windows_error(buffer, err_code, vformat(format_str, args));
  std::runtime_error& base = *this;
  base = std::runtime_error(to_string(buffer));
}

void internal::format_windows_error(internal::buffer<char>& out, int error_code,
                                    string_view message) FMT_NOEXCEPT {
  FMT_TRY {
    wmemory_buffer buf;
    buf.resize(inline_buffer_size);
    for (;;) {
      wchar_t* system_message = &buf[0];
      int result = FormatMessageW(
          FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
          error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), system_message,
          static_cast<uint32_t>(buf.size()), nullptr);
      if (result != 0) {
        utf16_to_utf8 utf8_message;
        if (utf8_message.convert(system_message) == ERROR_SUCCESS) {
          internal::writer w(out);
          w.write(message);
          w.write(": ");
          w.write(utf8_message);
          return;
        }
        break;
      }
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        break;  // Can't get error message, report error code instead.
      buf.resize(buf.size() * 2);
    }
  }
  FMT_CATCH(...) {}
  format_error_code(out, error_code, message);
}

void report_windows_error(int error_code,
                          fmt::string_view message) FMT_NOEXCEPT {
  report_error(internal::format_windows_error, error_code, message);
}
#endif  // _WIN32

buffered_file::~buffered_file() FMT_NOEXCEPT {
  if (file_ && FMT_SYSTEM(fclose(file_)) != 0)
    report_system_error(errno, "cannot close file");
}

buffered_file::buffered_file(cstring_view filename, cstring_view mode) {
  FMT_RETRY_VAL(file_, FMT_SYSTEM(fopen(filename.c_str(), mode.c_str())),
                nullptr);
  if (!file_)
    FMT_THROW(system_error(errno, "cannot open file {}", filename.c_str()));
}

void buffered_file::close() {
  if (!file_) return;
  int result = FMT_SYSTEM(fclose(file_));
  file_ = nullptr;
  if (result != 0) FMT_THROW(system_error(errno, "cannot close file"));
}

// A macro used to prevent expansion of fileno on broken versions of MinGW.
#define FMT_ARGS

int buffered_file::fileno() const {
  int fd = FMT_POSIX_CALL(fileno FMT_ARGS(file_));
  if (fd == -1) FMT_THROW(system_error(errno, "cannot get file descriptor"));
  return fd;
}

#if FMT_USE_FCNTL
file::file(cstring_view path, int oflag) {
  int mode = S_IRUSR | S_IWUSR;
#  if defined(_WIN32) && !defined(__MINGW32__)
  fd_ = -1;
  FMT_POSIX_CALL(sopen_s(&fd_, path.c_str(), oflag, _SH_DENYNO, mode));
#  else
  FMT_RETRY(fd_, FMT_POSIX_CALL(open(path.c_str(), oflag, mode)));
#  endif
  if (fd_ == -1)
    FMT_THROW(system_error(errno, "cannot open file {}", path.c_str()));
}

file::~file() FMT_NOEXCEPT {
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
  if (result != 0) FMT_THROW(system_error(errno, "cannot close file"));
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
    FMT_THROW(system_error(errno, "cannot get file attributes"));
  static_assert(sizeof(long long) >= sizeof(file_stat.st_size),
                "return type of file::size is not large enough");
  return file_stat.st_size;
#  endif
}

std::size_t file::read(void* buffer, std::size_t count) {
  RWResult result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(read(fd_, buffer, convert_rwcount(count))));
  if (result < 0) FMT_THROW(system_error(errno, "cannot read from file"));
  return internal::to_unsigned(result);
}

std::size_t file::write(const void* buffer, std::size_t count) {
  RWResult result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(write(fd_, buffer, convert_rwcount(count))));
  if (result < 0) FMT_THROW(system_error(errno, "cannot write to file"));
  return internal::to_unsigned(result);
}

file file::dup(int fd) {
  // Don't retry as dup doesn't return EINTR.
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/dup.html
  int new_fd = FMT_POSIX_CALL(dup(fd));
  if (new_fd == -1)
    FMT_THROW(system_error(errno, "cannot duplicate file descriptor {}", fd));
  return file(new_fd);
}

void file::dup2(int fd) {
  int result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(dup2(fd_, fd)));
  if (result == -1) {
    FMT_THROW(system_error(errno, "cannot duplicate file descriptor {} to {}",
                           fd_, fd));
  }
}

void file::dup2(int fd, error_code& ec) FMT_NOEXCEPT {
  int result = 0;
  FMT_RETRY(result, FMT_POSIX_CALL(dup2(fd_, fd)));
  if (result == -1) ec = error_code(errno);
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
  if (result != 0) FMT_THROW(system_error(errno, "cannot create pipe"));
  // The following assignments don't throw because read_fd and write_fd
  // are closed.
  read_end = file(fds[0]);
  write_end = file(fds[1]);
}

buffered_file file::fdopen(const char* mode) {
  // Don't retry as fdopen doesn't return EINTR.
  FILE* f = FMT_POSIX_CALL(fdopen(fd_, mode));
  if (!f)
    FMT_THROW(
        system_error(errno, "cannot associate stream with file descriptor"));
  buffered_file bf(f);
  fd_ = -1;
  return bf;
}

long getpagesize() {
#  ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#  else
  long size = FMT_POSIX_CALL(sysconf(_SC_PAGESIZE));
  if (size < 0) FMT_THROW(system_error(errno, "cannot get memory page size"));
  return size;
#  endif
}
#endif  // FMT_USE_FCNTL
FMT_END_NAMESPACE
