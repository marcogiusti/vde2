#include <errno.h>
#include <unittest.h>
#include <libvdeplug.h>


static void
test_interface_version(TESTARGS, void *usrptr)
{
#ifndef LIBVDEPLUG_INTERFACE_VERSION
	FAIL("LIBVDEPLUG_INTERFACE_VERSION not defined");
#else
	ASSERT_EQUAL(LIBVDEPLUG_INTERFACE_VERSION, 1,
			"Our tests are based to the version 1 of the protocol");
#endif
}

static void
test_vde_open_invalid_version(TESTARGS, void *usrptr)
{
	VDECONN *conn = NULL;
	struct vde_open_args args;

	conn = vde_open_real("", "", 0, &args);
	ASSERT_EQUAL(errno, EINVAL, "EINVAL reported");
	ASSERT_PTR_NULL(conn, "connection is not set");
}

static void
test_vde_open_relative_path(TESTARGS, void *usrptr)
{
	VDECONN *conn = NULL;
	int error;

	conn = vde_open("relative/path/to/a/sock", NULL, NULL);
	error = errno;
	ASSERT_PTR_NULL(conn, "the connection should not be set");
	ASSERT_NOT_EQUAL(error, 0, "errno shoud be set");
}

struct test_suite*
load_test_suite(struct test_loader *loader)
{
	struct test_suite *suite;

	suite = test_suite_new();
	suite->name = "test_asserts";
	suite->doc = "Basic tests about the asserts";
	suite->add_test(suite, test_case_new(test_interface_version));
	suite->add_test(suite, test_case_new(test_vde_open_invalid_version));
	suite->add_test(suite, test_case_new(test_vde_open_relative_path));
	return suite;
}

int
main(int argc, char *argv[])
{
	return test_main3(0, (char **) {NULL});
}
