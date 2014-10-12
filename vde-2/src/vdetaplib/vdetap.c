/* Copyright 2004 Renzo Davoli
 * Reseased under the GPLv2 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pwd.h>

#include <config.h>
#include <vde.h>
#include <vdecommon.h>
#include <libvdeplug.h>

#define SWITCH_MAGIC 0xfeedface
#define BUFSIZE 2048
#define MAXDESCR 128

VDECONN *conn;

static unsigned char bufin[BUFSIZE];

static struct pollfd pollv[]={{0,POLLIN|POLLHUP,0},{0,POLLIN|POLLHUP,0}};

int main(int argc,char *argv[])
{
	int fd,fddata;
	int nx;
	int i;
	struct vde_open_args open_args={.port=0,.group=NULL,.mode=0700};
	char *descr;
	/*printf("argc = %d\n",argc);
	for (i=0;i<argc;i++)
		printf("argv %d -%s-\n",i,argv[i]);*/
	if (argc < 4 && argv[0][0] != '-') {
		fprintf(stderr,"vdetap must be activated by libvdetap e.g.\n"
				"   sh%% export LD_PRELOAD=%s/libvdetap.so\n"
				"   csh%% setenv LD_PRELOAD %s/libvdetap.so\n", LIBEXECDIR, LIBEXECDIR);
		exit (-1);
	}

	fd = atoi(argv[1]);
	for (i=3;i<FD_SETSIZE;i++)
		if (i != fd)
			close(i);
	if((fddata = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
		perror("socket");
		exit(1);
	}
	if (argc == 8) {
		open_args.port=atoi(argv[5]);
		open_args.group=argv[6];
	  sscanf(argv[7],"%i",&open_args.mode);
		//fprintf(stderr,"|%d|%s|%o|\n",open_args.port,open_args.group,open_args.mode);
		asprintf(&descr,"tuntaplib %s/%s",argv[4],argv[3]);
	} else
		descr="tuntaplib";
	conn=vde_open(argv[2],descr,&open_args);
	pollv[0].fd=fd;
	pollv[1].fd=vde_datafd(conn);
	for(;;) {
		poll(pollv,2,-1);
		if (pollv[0].revents & POLLHUP || pollv[1].revents & POLLHUP)
			break;
		if (pollv[0].revents & POLLIN) {
			nx=read(fd,bufin,sizeof(bufin));
			/*fprintf(stderr,"RX from pgm %d\n",nx);*/
			vde_send(conn,bufin,nx,0);
		}
		if (pollv[1].revents & POLLIN) {
			nx=vde_recv(conn,bufin,BUFSIZE,0);
			/*fprintf(stderr,"TX to pgm %d\n",nx);*/
			write(fd,bufin,nx);
		}
	}
	vde_close(conn);
	return 0;
}

