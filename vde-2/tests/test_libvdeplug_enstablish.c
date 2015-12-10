#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unittest.h>
#include <libvdeplug.h>

#include "common.h"

#define TEST_SOCKET_FILENAME "/tmp/test_libvdeplug"

static struct sockaddr_un _un;


struct testdata {
	char path[sizeof(_un.sun_path)];
	int fd;
	VDECONN *conn;
};


static void
setup(struct test_suite *suite)
{
}

static void
teardown(struct test_suite *suite)
{
	struct testdata *data = (struct testdata *) suite->usrptr;

	if (*data->path != '\0')
		unlink(data->path);
	data->path[0] = '\0';
	if (data->fd != 0) {
		close(data->fd);
		data->fd = 0;
	}
	if (data->conn != NULL) {
		vde_close(data->conn);
		data->conn = NULL;
	}
}

static void
test_p2p_no_one_listen(TESTARGS, void *usrptr)
{
	struct testdata *data = (struct testdata *) usrptr;
	struct vde_open_args args = {
		.port = -1, /* VDEFLAG_P2P_SOCKET; */
		.group = NULL,
		.mode = 0
	};

	strncpy(data->path, TEST_SOCKET_FILENAME, sizeof(data->path));
	if ((data->fd = s_unix_listen(data->path)) < 0)
		ERROR("cannot create the unix socket");
	close(data->fd);
	data->conn = vde_open(data->path, NULL, &args);
	ASSERT_PTR_NOT_NULL(data->conn, "A new connection is established");
}

static void
test_p2p_no_args(TESTARGS, void *usrptr)
{
	struct testdata *data = (struct testdata *) usrptr;

	strncpy(data->path, TEST_SOCKET_FILENAME "[]", sizeof(_un.sun_path));
	/* A square bracket at end of the path's name indicate a p2p socket. */
	data->conn = vde_open(data->path, NULL, NULL);
	ASSERT_PTR_NOT_NULL(data->conn, "A new connection is established");
}

struct test_suite*
load_test_suite(struct test_loader *loader)
{
	struct test_suite *suite;

	suite = test_suite_new();
	suite->usrptr = alloc0(sizeof(struct testdata));
	suite->name = "test_libvdeplug_enstablish";
	suite->doc = "Test libvdeplug enstablished connections";
	suite->setup = setup;
	suite->teardown = teardown;
	suite->add_test(suite, test_case_new(test_p2p_no_one_listen));
	suite->add_test(suite, test_case_new(test_p2p_no_args));
	return suite;
}

int
main(int argc, char *argv[])
{
	return test_main3(0, (char **) {NULL});
}
