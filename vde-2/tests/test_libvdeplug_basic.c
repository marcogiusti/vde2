#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <unittest.h>
#include <libvdeplug.h>


#define TEMPFILENAME "/tmp/vdetest.XXXXXX"


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

static void
test_vde_open_invalid_sock(TESTARGS, void *usrptr)
{
	VDECONN *conn = NULL;
	int error,
		fp;
	char *filename;

	if ((filename = (char *) malloc(strlen(TEMPFILENAME))) == NULL)
		/* TODO: add a failure. */
		FAIL("malloc failed");
	strcpy(filename, TEMPFILENAME);
	if ((fp = mkstemp(filename)) < 0) {
		/* TODO: add a failure. */
		free(filename);
		FAIL("mkstemp failed");
	}
	close(fp);
	conn = vde_open(filename, NULL, NULL);
	error = errno;
	unlink(filename);
	free(filename);
	ASSERT_PTR_NULL(conn, "the connection should not be set");
	ASSERT_NOT_EQUAL(error, 0, "errno shoud be set");
}

static void
test_2p2_no_socket_name_given(TESTARGS, void *usrptr)
{
	VDECONN *conn = NULL;
	int error;
	struct vde_open_args args = {
		.port = -1, /* VDEFLAG_P2P_SOCKET; */
		.group = NULL,
		.mode = 0
	};

	conn = vde_open(NULL, NULL, &args);
	error = errno;
	ASSERT_PTR_NULL(conn, "the connection should not be set");
	ASSERT_EQUAL(error, EINVAL, "errno shoud be set to EINVAL");
}

struct test_suite*
load_test_suite(struct test_loader *loader)
{
	struct test_suite *suite;

	suite = test_suite_new();
	suite->name = "test_libvdeplug_basic";
	suite->doc = "Basic libvdeplug tests";
	suite->add_test(suite, test_case_new(test_interface_version));
	suite->add_test(suite, test_case_new(test_vde_open_invalid_version));
	suite->add_test(suite, test_case_new(test_vde_open_relative_path));
	suite->add_test(suite, test_case_new(test_vde_open_invalid_sock));
	suite->add_test(suite, test_case_new(test_2p2_no_socket_name_given));
	return suite;
}

int
main(int argc, char *argv[])
{
	return test_main3(0, (char **) {NULL});
}
