dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.68])

dnl The mccr version number
m4_define([mccr_major_version], [1])
m4_define([mccr_minor_version], [3])
m4_define([mccr_micro_version], [0])
m4_define([mccr_version],
          [mccr_major_version.mccr_minor_version.mccr_micro_version])

AC_INIT([mccr], [mccr_version], [aleksander@aleksander.es])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign no-define no-dist-gzip dist-xz tar-ustar -Wno-portability])
AM_MAINTAINER_MODE([enable])

dnl Support silent build rules. Disable
dnl by either passing --disable-silent-rules to configure or passing V=1
dnl to make
AM_SILENT_RULES([yes])

dnl Required programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

dnl Initialize libtool
LT_PREREQ([2.2])
LT_INIT

dnl Specific warnings to always use
MCCR_COMPILER_WARNINGS

dnl Version stuff
MCCR_MAJOR_VERSION=mccr_major_version
MCCR_MINOR_VERSION=mccr_minor_version
MCCR_MICRO_VERSION=mccr_micro_version
MCCR_VERSION=mccr_version
AC_SUBST(MCCR_MAJOR_VERSION)
AC_SUBST(MCCR_MINOR_VERSION)
AC_SUBST(MCCR_MICRO_VERSION)
AC_SUBST(MCCR_VERSION)

dnl Checks for builtin atomic operations
AC_CACHE_CHECK([for lock-free atomic intrinsics], mccr_cv_atomic_lock_free, [
  AC_TRY_LINK([],
                 [volatile int atomic = 2;\
                  __sync_bool_compare_and_swap (&atomic, 2, 3);],
                 [mccr_cv_atomic_lock_free=yes],
                 [mccr_cv_atomic_lock_free=no])
  if test "$mccr_cv_atomic_lock_free" = "no"; then
    SAVE_CFLAGS="${CFLAGS}"
    CFLAGS="-march=i486"
    AC_TRY_LINK([],
                   [volatile int atomic = 2;\
                    __sync_bool_compare_and_swap (&atomic, 2, 3);],
                   [AC_MSG_ERROR([MCCR must be built with -march=i486 or later.])],
                   [])
    CFLAGS="${SAVE_CFLAGS}"
  fi
])

# Some compilers support atomic operations but do not define
# __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4, like clang
if test x"$mccr_cv_atomic_lock_free" = xyes; then
  AC_TRY_LINK([],
                 [__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4;],
                 [],
                 [AC_DEFINE(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4, 1, [ compiler supports atomic operations])])
fi

dnl Require hidapi
PKG_CHECK_MODULES(HIDAPI_USB, [hidapi-libusb], [have_hidapi_usb=yes], [have_hidapi_usb=no])
PKG_CHECK_MODULES(HIDAPI_RAW, [hidapi-hidraw], [have_hidapi_raw=yes], [have_hidapi_raw=no])

dnl Require libusb if usb backend
PKG_CHECK_MODULES(LIBUSB, [libusb-1.0], [have_libusb=yes], [have_libusb=no])

AC_ARG_WITH([hidapi-backend],
            AS_HELP_STRING([--with-hidapi-backend=[usb|raw]],
                           [build with a specific hidapi backend [default: raw preferred]]),
            [with_hidapi_backend=$withval],
            [with_hidapi_backend=auto])

if [ test "$with_hidapi_backend" = "auto" ]; then
    if [ test "$have_hidapi_raw" = "yes" ]; then
        hidapi_backend="raw"
    elif [ test "$have_hidapi_usb" = "yes" ]; then
        hidapi_backend="usb"
    else
        AC_MSG_ERROR([hidapi library not found])
    fi
else
    hidapi_backend="$with_hidapi_backend"
fi

case "$hidapi_backend" in
    "usb")
        if [ test "$have_hidapi_usb" != "yes" ]; then
            AC_MSG_ERROR([hidapi library with USB backend required])
        fi
        if [ test "$have_libusb" != "yes" ]; then
            AC_MSG_ERROR([libusb library required])
        fi
        AC_DEFINE([HIDAPI_BACKEND_USB], 1, [Define if usb backend is used])
        HIDAPI_CFLAGS=$HIDAPI_USB_CFLAGS
        HIDAPI_LIBS=$HIDAPI_USB_LIBS
	HIDAPI_PKGCONFIG="libusb-1.0 hidapi-libusb"
        ;;
    "raw")
        if [ test "$have_hidapi_raw" != "yes" ]; then
            AC_MSG_ERROR([hidapi library with RAW backend required])
        fi
        AC_DEFINE([HIDAPI_BACKEND_RAW], 1, [Define if raw backend is used])
        HIDAPI_CFLAGS=$HIDAPI_RAW_CFLAGS
        HIDAPI_LIBS=$HIDAPI_RAW_LIBS
	HIDAPI_PKGCONFIG="hidapi-hidraw"
        ;;
    *)
        AC_MSG_ERROR([unknown hidapi backend requested: $hidapi_backend])
        ;;
esac

AC_SUBST(HIDAPI_CFLAGS)
AC_SUBST(HIDAPI_LIBS)
AC_SUBST(HIDAPI_PKGCONFIG)
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)

AM_CONDITIONAL([HIDAPI_BACKEND_USB], [test "$hidapi_backend" = "usb"])
AM_CONDITIONAL([HIDAPI_BACKEND_RAW], [test "$hidapi_backend" = "raw"])

dnl require libdukpt, GTK+, GLib/GIO, GUdev, libxml and Soup to build the UI

DUKPT_REQUIRED=1.2
GTK_REQUIRED=3.10
GLIB_REQUIRED=2.40
GUDEV_REQUIRED=147
LIBXML_REQUIRED=2
SOUP_REQUIRED=2.42

PKG_CHECK_MODULES(DUKPT, [dukpt >= $DUKPT_REQUIRED], [have_dukpt=yes], [have_dukpt=no])
AC_SUBST(DUKPT_CFLAGS)
AC_SUBST(DUKPT_LIBS)

PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= $GTK_REQUIRED], [have_gtk=yes],[have_gtk=no])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= $GLIB_REQUIRED gobject-2.0 gio-2.0], [have_glib=yes], [have_glib=no])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
if test "x$have_glib" = "xyes"; then
   GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable=glib_compile_resources gio-2.0`
   AC_SUBST(GLIB_COMPILE_RESOURCES)

   GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
   AC_SUBST(GLIB_MKENUMS)
fi

PKG_CHECK_MODULES(GUDEV, [gudev-1.0 >= $GUDEV_REQUIRED], [have_gudev=yes],[have_gudev=no])
AC_SUBST(GUDEV_CFLAGS)
AC_SUBST(GUDEV_LIBS)

PKG_CHECK_MODULES(LIBXML, [libxml-2.0 >= $LIBXML_REQUIRED], [have_libxml=yes],[have_libxml=no])
AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES(SOUP, [libsoup-2.4 >= $SOUP_REQUIRED], [have_soup=yes],[have_soup=no])
AC_SUBST(SOUP_CFLAGS)
AC_SUBST(SOUP_LIBS)

dnl udev base directory
AC_ARG_WITH(udev-base-dir, AS_HELP_STRING([--with-udev-base-dir=DIR], [where udev base directory is]))
if test -n "$with_udev_base_dir" ; then
    UDEV_BASE_DIR="$with_udev_base_dir"
else
    UDEV_BASE_DIR="/lib/udev"
fi
AC_SUBST(UDEV_BASE_DIR)

dnl mccr-gtk is optional
AC_ARG_ENABLE([mccr-gtk],
              AS_HELP_STRING([--enable-mccr-gtk],
                             [build mccr-gtk [default=yes]]),
              [build_mccr_gtk=$enableval],
              [build_mccr_gtk=yes])

if test "x$build_mccr_gtk" = "xyes"; then
   if test "x$have_gtk" = "xno"; then
       AC_MSG_ERROR([Couldn't find GTK+ >= ${GTK_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   elif test "x$have_glib" = "xno"; then
       AC_MSG_ERROR([Couldn't find GLib/GIO >= ${GLIB_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   elif test "x$have_gudev" = "xno"; then
       AC_MSG_ERROR([Couldn't find GUdev >= ${GUDEV_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   elif test "x$have_soup" = "xno"; then
       AC_MSG_ERROR([Couldn't find libsoup >= ${SOUP_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   elif test "x$have_libxml" = "xno"; then
       AC_MSG_ERROR([Couldn't find libxml >= ${LIBXML_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   elif test "x$have_dukpt" = "xno"; then
       AC_MSG_ERROR([Couldn't find libdukpt >= ${DUKPT_REQUIRED}. Install it, or otherwise configure using --disable-mccr-gtk to disable building `mccr-gtk'.])
   fi
fi
AM_CONDITIONAL(BUILD_MCCR_GTK, test "x$build_mccr_gtk" = "xyes")

dnl enable glib test options
GLIB_TESTS

dnl Documentation
GTK_DOC_CHECK(1.0)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/common/Makefile
                 src/libmccr/Makefile
                 src/libmccr/mccr.h
                 src/libmccr/mccr.pc
                 src/mccr-cli/Makefile
                 src/mccr-gtk/Makefile
                 src/mccr-gtk/test/Makefile
                 doc/Makefile
                 doc/reference/Makefile
                 doc/reference/version.xml
		 examples/Makefile])
AC_OUTPUT

echo "
    mccr $VERSION
    ==============================================

    Build:
      compiler:             ${CC}
      cflags:               ${CFLAGS}
      ldflags:              ${LDFLAGS}
      maintainer mode:      ${USE_MAINTAINER_MODE}

    System paths:
      prefix:               ${prefix}
      udev base directory:  ${UDEV_BASE_DIR}

    Features:
      hidapi backend:       ${hidapi_backend}

    Components:
      libmccr:              yes
      mccr-cli:             yes
      mccr-gtk:             ${build_mccr_gtk}
"
