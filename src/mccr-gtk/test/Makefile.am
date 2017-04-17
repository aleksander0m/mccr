
include $(top_srcdir)/gtester.make

if BUILD_MCCR_GTK

noinst_PROGRAMS += \
	test-remote-service-utils \
	$(NULL)

TEST_PROGS += $(noinst_PROGRAMS)

test_remote_service_utils_SOURCES = test-remote-service-utils.c
test_remote_service_utils_CPPFLAGS =  \
	-I$(top_srcdir)/src/mccr-gtk \
	$(LIBXML_CFLAGS)             \
	$(GLIB_CFLAGS)               \
	$(NULL)
test_remote_service_utils_LDADD = \
	$(top_builddir)/src/mccr-gtk/libmui-remote-service-helpers.la \
	$(LIBXML_LIBS)                                                \
	$(GLIB_LIBS)                                                  \
	$(NULL)

endif # BUILD_MCCR_GTK

EXTRA_DIST += \
	get-command-list-response.xml     \
	get-key-list-response.xml         \
	get-key-load-command-response.xml \
	get-command-by-ksn-response.xml   \
	get-command-by-mut-response.xml   \
	$(NULL)
