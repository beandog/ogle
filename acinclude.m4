dnl AC_C_ALWAYS_INLINE
dnl Define inline to something appropriate, including the new always_inline
dnl attribute from gcc 3.1
dnl Written by: Michel Lespinasse <walken@zoy.org>
AC_DEFUN([AC_C_ALWAYS_INLINE],
    [AC_C_INLINE
    if test x"$GCC" = x"yes" -a x"$ac_cv_c_inline" = x"inline"; then
        AC_MSG_CHECKING([for always_inline])
        SAVE_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS -Wall -Werror"
        AC_TRY_COMPILE([],[__attribute__ ((__always_inline__)) void f (void);],
            [ac_cv_always_inline=yes],[ac_cv_always_inline=no])
        CFLAGS="$SAVE_CFLAGS"
        AC_MSG_RESULT([$ac_cv_always_inline])
        if test x"$ac_cv_always_inline" = x"yes"; then
            AC_DEFINE_UNQUOTED([inline],[__attribute__ ((__always_inline__))])
        fi
    fi])

dnl AC_CHECK_GENERATE_INTTYPES_H (INCLUDE-DIRECTORY)
dnl generate a default inttypes.h if the header file does not exist already
dnl Written by: Michel Lespinasse <walken@zoy.org>
AC_DEFUN([AC_CHECK_GENERATE_INTTYPES],
    [rm -f $1/inttypes.h
    AC_CHECK_HEADER([inttypes.h],[],
        [AC_CHECK_SIZEOF([char])
        AC_CHECK_SIZEOF([short])
        AC_CHECK_SIZEOF([int])
        AC_CHECK_SIZEOF([long long])
        if test x"$ac_cv_sizeof_char" != x"1" -o \
            x"$ac_cv_sizeof_short" != x"2" -o \
            x"$ac_cv_sizeof_int" != x"4" -o \
            x"$ac_cv_sizeof_long_long" != x"8"; then
            AC_MSG_ERROR([can not build a default inttypes.h])
        fi
        cat >$1/inttypes.h << EOF
/* default inttypes.h for people who do not have it on their system */

#ifndef _INTTYPES_H
#define _INTTYPES_H
#if (!defined __int8_t_defined) && (!defined __BIT_TYPES_DEFINED__)
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
#endif
#if (!defined _LINUX_TYPES_H)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif
#endif
EOF
        ])])
