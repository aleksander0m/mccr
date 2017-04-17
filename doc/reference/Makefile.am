

# The name of the module.
DOC_MODULE = libmccr

# The top-level SGML file.
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS =

# The directory containing the source code.
DOC_SOURCE_DIR = \
	$(top_srcdir)/src/libmccr \
	$(NULL)

# Used for dependencies
HFILE_GLOB = \
	$(top_srcdir)/src/libmccr/*.h \
	$(NULL)
CFILE_GLOB = \
	$(top_srcdir)/src/libmccr/*.c \
	$(NULL)

# Headers to ignore
IGNORE_HFILES = \
	mccr-hid.h \
	mccr-feature-report.h \
	mccr-input-report.h \
	mccr-log.h \
	mccr-raw.h \
	mccr-usb.h \
	$(NULL)

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(NULL)

GTKDOC_LIBS = \
	$(top_builddir)/src/libmccr/libmccr.la \
	$(NULL)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --output-format=xml --sgml-mode --name-space=mccr

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml

expand_content_files =

extra_files =

include $(top_srcdir)/gtk-doc.make

EXTRA_DIST += \
	version.xml.in \
	$(NULL)

CLEANFILES += \
	$(DOC_MODULE)-decl-list.txt \
	$(DOC_MODULE)-decl.txt \
	$(DOC_MODULE)-overrides.txt \
	$(DOC_MODULE)-undeclared.txt \
	$(DOC_MODULE)-undocumented.txt \
	$(DOC_MODULE)-overrides.txt \
	$(DOC_MODULE)-unused.txt \
	$(DOC_MODULE).args \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals \
	$(DOC_MODULE).types \
	*.stamp \
	-rf xml html tmpl \
	$(NULL)
