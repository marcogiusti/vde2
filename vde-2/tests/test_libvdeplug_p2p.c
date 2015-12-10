#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
/* #include <fcntl.h> */
#include <unittest.h>
#include <libvdeplug.h>
#include "common.h"


#define TEST_SOCK_PATH "/tmp/test_libvdeplug"


struct testdata {
	pid_t child;
	char *path;
	const char *err;
};

static void
sig_term(int signo)
{
	_exit(0);
}

static void
child_task(int fd, struct testdata *data)
{
	int sd, ret;

	signal(SIGTERM, sig_term);

	if ((sd = s_unix_listen(data->path)) < 0)
		exit(1);
	if ((ret = write(fd, "\0", 1)) == -1)
		exit(1);
	if (s_unix_accept(sd, NULL) < 0)
		exit(1);
	while (1)
		pause();
}

static int
parent_task(int fd)
{
	char buf;

	if (read(fd, &buf, 1) < -1)
		return -1;
	return 0;
}

static void
setup(struct test_suite *suite)
{
	int fd[2] = {-1, -1};
	int status;
	struct testdata *data = (struct testdata *) suite->usrptr;

	data->path = TEST_SOCK_PATH;
	unlink(data->path);
	if (pipe(fd) < 0)
		goto err;
	if ((data->child = fork()) < 0)
		goto err;
	else if (data->child == 0) {
		close(fd[0]);
		/* sleep(5); */
		child_task(fd[1], data);
		abort();  /* child_task does not return. */
	} else {
		/* fprintf(stderr, "%d\n", data->child); */
		/* raise(SIGTRAP); */
		close(fd[1]);
		if (parent_task(fd[0]) == -1)
			goto err;
	}
	return;
err:
	data->err = strerror(errno);
	if (data->child != 0) {
		kill(data->child, SIGKILL);
		waitpid(data->child, &status, 0);
		data->child = 0;
	}
	if (fd[0] != -1) {
		close(fd[0]);
		close(fd[1]);
	}
}

static void
teardown(struct test_suite *suite)
{
	struct testdata *data = (struct testdata *) suite->usrptr;
	int status;

	if (data->child != 0) {
		if (kill(data->child, SIGTERM) == 0)
			waitpid(data->child, &status, 0);
		data->child = 0;
	}
	if (data->path != NULL) {
		unlink(data->path);
		data->path = NULL;
	}
	data->err = NULL;
}

static void
test_socket_already_exists(TESTARGS, void *usrptr)
{
	VDECONN *conn = NULL;
	int error;
	struct testdata *data = (struct testdata *) usrptr;
	struct vde_open_args args = {
		.port = -1, /* VDEFLAG_P2P_SOCKET; */
		.group = NULL,
		.mode = 0
	};

	if (data->err != NULL)
		ERROR(data->err);

	conn = vde_open(data->path, NULL, &args);
	error = errno;
	ASSERT_PTR_NULL(conn, "the connection should not be set");
	ASSERT_EQUAL(error, EADDRINUSE, "socket already in use");
}

struct test_suite*
load_test_suite(struct test_loader *loader)
{
	struct test_suite *suite;

	suite = test_suite_new();
	if ((suite->usrptr = malloc(sizeof(struct testdata))) == NULL) {
		fputs("malloc failed\n", stderr);
		fflush(stderr);
		_exit(1);
	}
	suite->name = "test_libvdeplug_p2p";
	suite->doc = "Test p2p connections";
	suite->setup = setup;
	suite->teardown = teardown;
	suite->add_test(suite, test_case_new(test_socket_already_exists));
	/* suite->add_test(suite, test_case_new(test_no_one_listen)); */
	return suite;
}

int
main(int argc, char *argv[])
{
	return test_main3(0, (char **) {NULL});
}
