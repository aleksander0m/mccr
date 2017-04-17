
bin_PROGRAMS = mccr-cli

mccr_cli_SOURCES = \
	mccr-cli.c \
	$(NULL)

mccr_cli_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/libmccr \
	$(NULL)

mccr_cli_LDADD = \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/libmccr/libmccr.la \
	$(NULL)
