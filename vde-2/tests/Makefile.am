LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
AM_CFLAGS = -Wall -Werror -ggdb -O0
AM_LDFLAGS = -ldl -rdynamic -lunittest
LDADD = $(top_builddir)/src/lib/libvdeplug.la
check_PROGRAMS = test_libvdeplug_basic
TESTS = $(check_PROGRAMS)
test_libvdeplug_basic_SOURCES = test_libvdeplug_basic.c
