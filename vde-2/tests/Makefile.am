LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
AM_CPPFLAGS = -I.
AM_CFLAGS = -Wall -Werror -ggdb -O0
AM_LDFLAGS = -ldl -rdynamic -lunittest
LDADD = $(top_builddir)/src/lib/libvdeplug.la

check_PROGRAMS = test_libvdeplug_basic \
				 test_libvdeplug_p2p \
				 test_libvdeplug_enstablish
TESTS = $(check_PROGRAMS)
test_libvdeplug_basic_SOURCES = test_libvdeplug_basic.c
test_libvdeplug_p2p_SOURCES = test_libvdeplug_p2p.c common.c
test_libvdeplug_enstablish_SOURCES = test_libvdeplug_enstablish.c common.c
