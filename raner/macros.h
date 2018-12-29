// Copyright 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_BASE_MACROS_H_
#define RANER_BASE_MACROS_H_

#include <stddef.h>  // For size_t.

// Put this in the declarations for a class to be uncopyable.
#define DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete

// Put this in the declarations for a class to be unassignable.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete

// Put this in the declarations for a class to be uncopyable and unassignable.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#if defined(NDEBUG)

#define HANDLE_EINTR(x)                                     \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#else

#define HANDLE_EINTR(x)                                      \
  ({                                                         \
    int eintr_wrapper_counter = 0;                           \
    decltype(x) eintr_wrapper_result;                        \
    do {                                                     \
      eintr_wrapper_result = (x);                            \
    } while (eintr_wrapper_result == -1 && errno == EINTR && \
             eintr_wrapper_counter++ < 100);                 \
    eintr_wrapper_result;                                    \
  })

#endif  // NDEBUG

// Preserving errno for Close() is important because the function is very often
// used in cleanup code, after an error occurred, and it is very easy to pass an
// invalid file descriptor to close() in this context, or more rarely, a
// spurious signal might make close() return -1 + setting errno to EINTR,
// masking the real reason for the original error. This leads to very unpleasant
// debugging sessions.
#define PRESERVE_ERRNO_HANDLE_EINTR(Func) \
  do {                                    \
    int local_errno = errno;              \
    (void)HANDLE_EINTR(Func);             \
    errno = local_errno;                  \
  } while (false);

#define IGNORE_EINTR(x)                                   \
  ({                                                      \
    decltype(x) eintr_wrapper_result;                     \
    do {                                                  \
      eintr_wrapper_result = (x);                         \
      if (eintr_wrapper_result == -1 && errno == EINTR) { \
        eintr_wrapper_result = 0;                         \
      }                                                   \
    } while (0);                                          \
    eintr_wrapper_result;                                 \
  })

// RANER_IS_LITTLE_ENDIAN
// RANER_IS_BIG_ENDIAN
//
// Checks the endianness of the platform.
//
// Notes: uses the built in endian macros provided by GCC (since 4.6) and
// Clang (since 3.2); see
// https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html.
// Otherwise, if _WIN32, assume little endian. Otherwise, bail with an error.
#if defined(RANER_IS_BIG_ENDIAN)
#error "RANER_IS_BIG_ENDIAN cannot be directly set."
#endif
#if defined(RANER_IS_LITTLE_ENDIAN)
#error "RANER_IS_LITTLE_ENDIAN cannot be directly set."
#endif

#if (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define RANER_IS_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RANER_IS_BIG_ENDIAN 1
#else
#error "raner endian detection needs to be set up for your compiler"
#endif

#if defined(__linux__)
#define OS_LINUX 1
#define OS_POSIX 1
#define USE_TCMALLOC 1
// include a system header to pull in features.h for glibc/uclibc macros.
#include <unistd.h>
#if defined(__GLIBC__) && !defined(__UCLIBC__)
// we really are using glibc, not uClibc pretending to be glibc
#define LIBC_GLIBC 1
#endif
#endif  // defined(__linux__)

// Compiler detection.
#if defined(__GNUC__)
#define COMPILER_GCC 1
#else
#error Please add support for your compiler in macros.h
#endif

#endif  // RANER_BASE_MACROS_H_
