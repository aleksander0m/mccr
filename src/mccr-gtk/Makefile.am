
EXTRA_DIST =

SUBDIRS = . test

if BUILD_MCCR_GTK

AM_CFLAGS = \
	-I$(top_srcdir)             \
	-I$(top_srcdir)/src/common  \
	-I$(top_srcdir)/src/libmccr \
	$(DUKPT_CFLAGS)             \
	$(GLIB_CFLAGS)              \
	$(GUDEV_CFLAGS)             \
	$(GTK_CFLAGS)               \
	$(SOUP_CFLAGS)              \
	$(LIBXML_CFLAGS)            \
	$(NULL)

AM_LDFLAGS = \
	$(DUKPT_LIBS)                            \
	$(GLIB_LIBS)                             \
	$(GUDEV_LIBS)                            \
	$(GTK_LIBS)                              \
	$(LIBXML_LIBS)                           \
	$(SOUP_LIBS)                             \
	$(top_builddir)/src/common/libcommon.la  \
	$(top_builddir)/src/libmccr/libmccr.la   \
	$(NULL)

# udev rules
udevrulesdir = $(UDEV_BASE_DIR)/rules.d
udevrules_DATA = \
	99-mccr-hid.rules \
	$(NULL)

EXTRA_DIST += $(udevrules_DATA)

# Resources
resource_files =                      \
	$(shell                           \
		$(GLIB_COMPILE_RESOURCES)     \
		--sourcedir=$(srcdir)         \
		--generate-dependencies       \
		$(srcdir)/mui-gresource.xml)  \
		$(NULL)
mui-resources.c: mui-gresource.xml $(resource_files)
	$(AM_V_GEN)                       \
		$(GLIB_COMPILE_RESOURCES)     \
		--target=$@                   \
		--sourcedir=$(srcdir)         \
		--generate-source             \
		--c-name mui                  \
		$(srcdir)/mui-gresource.xml   \
		$(NULL)
mui-resources.h: mui-gresource.xml $(resource_files)
	$(AM_V_GEN)                       \
		$(GLIB_COMPILE_RESOURCES)     \
		--target=$@                   \
		--sourcedir=$(srcdir)         \
		--generate-header             \
		--c-name mui                  \
		$(srcdir)/mui-gresource.xml   \
		$(NULL)

BUILT_SOURCES = \
	mui-resources.h mui-resources.c \
	$(NULL)

EXTRA_DIST += \
	$(BUILT_SOURCES)  \
	mui-gresource.xml \
	$(resource_files) \
	mccr-gtk.desktop  \
	mccr-gtk.png      \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

# Helpers library

noinst_LTLIBRARIES = libmui-remote-service-helpers.la

libmui_remote_service_helpers_la_SOURCES = \
	mui-remote-service-utils.h mui-remote-service-utils.c \
	$(NULL)

# The main program
bin_PROGRAMS = mccr-gtk

mccr_gtk_SOURCES = \
	mccr-gtk.c                                            \
	mui-resources.h mui-resources.c                       \
	mui-app.h mui-app.c                                   \
	mui-page.h mui-page.c                                 \
	mui-page-basic.h mui-page-basic.c                     \
	mui-page-advanced.h mui-page-advanced.c               \
	mui-page-remote-services.h mui-page-remote-services.c \
	mui-window.h mui-window.c                             \
	mui-processor.h mui-processor.c                       \
	mui-remote-service.h mui-remote-service.c             \
	$(NULL)

mccr_gtk_LDADD = \
	libmui-remote-service-helpers.la \
	$(NULL)

# Icon
# http://www.softicons.com/system-icons/web0.2ama-icons-by-chrfb/payment-credit-card-icon
# license: https://creativecommons.org/licenses/by-nc-sa/3.0/
icon256dir = $(datadir)/icons/hicolor/256x256/apps
icon256_DATA = mccr-gtk.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then                                         \
		echo "Updating Gtk icon cache.";                                    \
		$(gtk_update_icon_cache);                                           \
	else                                                                    \
		echo "*** Icon cache not updated.  After (un)install, run this:";   \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

# Desktop file
desktopdir = $(datadir)/applications
desktop_DATA = mccr-gtk.desktop

endif # BUILD_MCCR_GTK
