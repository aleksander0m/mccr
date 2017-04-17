
lib_LTLIBRARIES = libmccr.la

libmccr_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/src/common \
	$(LIBUSB_CFLAGS) \
	$(HIDAPI_CFLAGS) \
	$(NULL)

libmccr_la_SOURCES = \
	mccr.h mccr.c \
	mccr-log.h mccr-log.c \
	mccr-hid.h mccr-hid.c \
	mccr-feature-report.h mccr-feature-report.c \
	mccr-input-report.h mccr-input-report.c \
	$(NULL)

libmccr_la_LIBADD = \
	$(top_builddir)/src/common/libcommon.la \
	$(NULL)

libmccr_la_LDFLAGS = \
	$(HIDAPI_LIBS) \
	$(LIBUSB_LIBS) \
	$(NULL)

if HIDAPI_BACKEND_USB
libmccr_la_SOURCES += mccr-usb.h mccr-usb.c
libmccr_la_LIBADD += $(LIBUSB_LIBS)
endif

if HIDAPI_BACKEND_RAW
libmccr_la_SOURCES += mccr-raw.h mccr-raw.c
endif

include_HEADERS = \
	mccr.h \
	$(NULL)

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = mccr.pc
