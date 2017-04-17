
SUBDIRS = . src doc examples

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-more-warnings=error             \
    --enable-gtk-doc                         \
	--with-udev-base-dir="$$dc_install_base" \
	CFLAGS="-O2 -D_FORTIFY_SOURCE=2"         \
	$(NULL)

ACLOCAL_AMFLAGS = -I m4
