/* Copyright 2005 Renzo Davoli VDE-2
 * Licensed under the GPL
 * --pidfile/-p and cleanup management by Mattia Belletti.
 * some code remains from uml_switch Copyright 2001, 2002 Jeff Dike and others
 * Modified by Ludovico Gardenghi 2005
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "switch.h"
#include "qtimer.h"
#include "hash.h"
#include "port.h"
#ifdef FSTP
#include "fstp.h"
#endif
#include "consmgmt.h"
#include <sys/time.h>
#include <time.h>

#include <config.h>
#include <vde.h>
#include <vdecommon.h>

#include <poll.h>

static struct swmodule *swmh;

char *prog;
unsigned char switchmac[ETH_ALEN];
unsigned int priority=DEFAULT_PRIORITY;

static int hash_size=INIT_HASH_SIZE;
static int numports=INIT_NUMPORTS;


static void recaddswm(struct swmodule **p,struct swmodule *new)
{
	struct swmodule *this=*p;
	if (this == NULL)
		*p=new;
	else 
		recaddswm(&(this->next),new);
}

void add_swm(struct swmodule *new)
{
	static int lastlwmtag;
	new->swmtag= ++lastlwmtag;
	if (new != NULL && new->swmtag != 0) {
		new->next=NULL;
		recaddswm(&swmh,new);
	}
}

static void recdelswm(struct swmodule **p,struct swmodule *old)
{
	struct swmodule *this=*p;
	if (this != NULL) {
		if(this == old)
			*p=this->next;
		else
			recdelswm(&(this->next),old);
	}
}

void del_swm(struct swmodule *old)
{
	if (old != NULL) {
		recdelswm(&swmh,old);
	}
}

/* FD MGMT */
struct pollplus {
	unsigned char type;
	void *private_data;
	time_t timestamp;
};

#define MAXFDS_INITIAL 8
#define MAXFDS_STEP 16
static int nfds = 0;
static int nprio =0;
static struct pollfd *fds = NULL;
static struct pollplus **fdpp = NULL;

/* permutation array: it maps each fd to its index in fds/fdpp */
/* fdpermsize is a multiple of 16 */
#define FDPERMSIZE_LOGSTEP 4
static short *fdperm;
static int fdpermsize=0;

static int maxfds = 0;

static struct swmodule **fdtypes;
static int ntypes;
static int maxtypes;

#define PRIOFLAG 0x80
#define TYPEMASK 0x7f
#define ISPRIO(X) ((X) & PRIOFLAG)

#define TYPE2MGR(X) (fdtypes[((X) & TYPEMASK)])

unsigned char add_type(struct swmodule *mgr,int prio)
{
	int i;
	if(ntypes==maxtypes) {
		maxtypes = maxtypes ? 2 * maxtypes : 8;
		if (maxtypes > PRIOFLAG) {
			printlog(LOG_ERR,"too many file types");
			exit(1);
		}
		if((fdtypes = realloc(fdtypes, maxtypes * sizeof(struct swmodule *))) == NULL){
			printlog(LOG_ERR,"realloc fdtypes %s",strerror(errno));
			exit(1);
		}
		memset(fdtypes+ntypes,0,sizeof(struct swmodule *) * maxtypes-ntypes);
		i=ntypes;
	} else
		for(i=0; fdtypes[i] != NULL; i++)
			;
	fdtypes[i]=mgr;
	ntypes++;
	return i | ((prio != 0)?PRIOFLAG:0);
}

void del_type(unsigned char type)
{
	type &= TYPEMASK;
	if (type < maxtypes)
		fdtypes[type]=NULL;
	ntypes--;
}

void add_fd(int fd,unsigned char type,void *private_data)
{
	struct pollfd *p;
	int index;
	/* enlarge fds and fdpp array if needed */
	if(nfds == maxfds){
		maxfds = maxfds ? maxfds + MAXFDS_STEP : MAXFDS_INITIAL;
		if((fds = realloc(fds, maxfds * sizeof(struct pollfd))) == NULL){
			printlog(LOG_ERR,"realloc fds %s",strerror(errno));
			exit(1);
		}
		if((fdpp = realloc(fdpp, maxfds * sizeof(struct pollplus *))) == NULL){
			printlog(LOG_ERR,"realloc pollplus %s",strerror(errno));
			exit(1);
		}
	}
	if (fd >= fdpermsize) {
		fdpermsize = ((fd >> FDPERMSIZE_LOGSTEP) + 1) << FDPERMSIZE_LOGSTEP;
		if((fdperm = realloc(fdperm, fdpermsize * sizeof(short))) == NULL){
			printlog(LOG_ERR,"realloc fdperm %s",strerror(errno));
			exit(1);
		}
	}
	if (ISPRIO(type)) {
		fds[nfds]=fds[nprio];
		fdpp[nfds]=fdpp[nprio];
		index=nprio;
		nprio++;
	} else
		index=nfds;
	if((fdpp[index]=malloc(sizeof(struct pollplus))) == NULL) {
		printlog(LOG_ERR,"realloc pollplus elem %s",strerror(errno));
		exit(1);
	}
	fdperm[fd]=index;
	p = &fds[index];
	p->fd = fd;
	p->events = POLLIN | POLLHUP;
	fdpp[index]->type=type;
	fdpp[index]->private_data=private_data;
	fdpp[index]->timestamp=0;
	nfds++;
}

static void file_cleanup(void)
{
	int i;
	for(i = 0; i < nfds; i++)
		TYPE2MGR(fdpp[i]->type)->cleanup(fdpp[i]->type,fds[i].fd,fdpp[i]->private_data);
}

void remove_fd(int fd)
{
	int i;

	for(i = 0; i < nfds; i++){
		if(fds[i].fd == fd) break;
	}
	if(i == nfds){
		printlog(LOG_WARNING,"remove_fd : Couldn't find descriptor %d", fd);
	} else {
		struct pollplus *old=fdpp[i];
		TYPE2MGR(fdpp[i]->type)->cleanup(fdpp[i]->type,fds[i].fd,fdpp[i]->private_data);
		if (ISPRIO(fdpp[i]->type)) nprio--;
		memmove(&fds[i], &fds[i + 1], (nfds - i - 1) * sizeof(struct pollfd));
		memmove(&fdpp[i], &fdpp[i + 1], (nfds - i - 1) * sizeof(struct pollplus *));
		for(;i<nfds;i++)
			fdperm[fds[i].fd]=i;
		free(old);
		nfds--;
	}
}

/* read/update events/private_data */
void *mainloop_get_private_data(int fd)
{
	if (fd >= 0 && fd < fdpermsize)
		return (fdpp[fdperm[fd]]->private_data);
	else
		return NULL;
}

void mainloop_set_private_data(int fd,void *private_data)
{
	if (fd >=0  && fd < fdpermsize)
		fdpp[fdperm[fd]]->private_data = private_data;
}

short mainloop_pollmask_get(int fd)
{
#if DEBUG_MAINLOOP_MASK
	if (fds[fdperm[fd]].fd != fd) printf("PERMUTATION ERROR %d %d\n",fds[fdperm[fd]].fd,fd);
#endif
	return fds[fdperm[fd]].events;
}

void mainloop_pollmask_add(int fd, short events)
{
#if DEBUG_MAINLOOP_MASK
	if (fds[fdperm[fd]].fd != fd) printf("PERMUTATION ERROR %d %d\n",fds[fdperm[fd]].fd,fd);
#endif
	fds[fdperm[fd]].events |= events;
}

void mainloop_pollmask_del(int fd, short events)
{
#if DEBUG_MAINLOOP_MASK
	if (fds[fdperm[fd]].fd != fd) printf("PERMUTATION ERROR %d %d\n",fds[fdperm[fd]].fd,fd);
#endif
	fds[fdperm[fd]].events &= ~events;
}

void mainloop_pollmask_set(int fd, short events)
{
#if DEBUG_MAINLOOP_MASK
	if (fds[fdperm[fd]].fd != fd) printf("PERMUTATION ERROR %d %d\n",fds[fdperm[fd]].fd,fd);
#endif
	fds[fdperm[fd]].events = events;
}

static void main_loop()
{
	time_t now;
	int n,i;
	while(1) {
		n=poll(fds,nfds,-1);
		now=qtime();
		if(n < 0){ 
			if(errno != EINTR)
				printlog(LOG_WARNING,"poll %s",strerror(errno));
		} else {
			for(i = 0; /*i < nfds &&*/ n>0; i++){
				if(fds[i].revents != 0) {
					int prenfds=nfds;
					n--;
					fdpp[i]->timestamp=now;
					TYPE2MGR(fdpp[i]->type)->handle_io(fdpp[i]->type,fds[i].fd,fds[i].revents,fdpp[i]->private_data);
					if (nfds!=prenfds) /* the current fd has been deleted */
						break; /* PERFORMANCE it is faster returning to poll */
				}	
/* optimization: most used descriptors migrate to the head of the poll array */
#ifdef OPTPOLL
				else
				{
					if (i < nfds && i > 0 && i != nprio) {
						int i_1=i-1;
						if (fdpp[i]->timestamp > fdpp[i_1]->timestamp) {
							struct pollfd tfds;
							struct pollplus *tfdpp;
							tfds=fds[i];fds[i]=fds[i_1];fds[i_1]=tfds;
							tfdpp=fdpp[i];fdpp[i]=fdpp[i_1];fdpp[i_1]=tfdpp;
							fdperm[fds[i].fd]=i;
							fdperm[fds[i_1].fd]=i_1;
						}
					}
				}
#endif
			}
		}
	}
}

/* starting/ending routines, main_loop, main*/
#define HASH_TABLE_SIZE_ARG 0x100
#define MACADDR_ARG         0x101
#define PRIORITY_ARG        0x102

static void Usage(void) {
	struct swmodule *p;
	printf(
			"Usage: vde_switch [OPTIONS]\n"
			"Runs a VDE switch.\n"
			"(global opts)\n"
			"  -h, --help                 Display this help and exit\n"
			"  -v, --version              Display informations on version and exit\n"
			"  -n  --numports             Number of ports (default %d)\n"
			"  -x, --hub                  Make the switch act as a hub\n"
#ifdef FSTP
			"  -F, --fstp                 Activate the fast spanning tree protocol\n"
#endif
			"      --macaddr MAC          Set the Switch MAC address\n"
#ifdef FSTP
			"      --priority N           Set the priority for FST (MAC extension)\n"
#endif
			"      --hashsize N           Hash table size\n"
			,numports);
	for(p=swmh;p != NULL;p=p->next)
		if (p->usage != NULL)
			p->usage();
	printf(
			"\n"
			"Report bugs to "PACKAGE_BUGREPORT "\n"
			);
	exit(1);
}

static void version(void)
{ 
	printf(
			"VDE " PACKAGE_VERSION "\n"
			"Copyright 2003,...,2011 Renzo Davoli\n"
			"some code from uml_switch Copyright (C) 2001, 2002 Jeff Dike and others\n"
			"VDE comes with NO WARRANTY, to the extent permitted by law.\n"
			"You may redistribute copies of VDE under the terms of the\n"
			"GNU General Public License v2.\n"
			"For more information about these matters, see the files\n"
			"named COPYING.\n");
	exit(0);
} 

static struct option *optcpy(struct option *tgt, struct option *src, int n, int tag)
{
	int i;
	memcpy(tgt,src,sizeof(struct option) * n);
	for (i=0;i<n;i++) {
		tgt[i].val=(tgt[i].val & 0xffff) | tag << 16;
	}
	return tgt+n;
}

static int parse_globopt(int c, char *optarg)
{
	int outc=0;
	switch (c) {
		case 'x':
			/* if it is a HUB FST is disabled */
#ifdef FSTP
			fstflag(P_CLRFLAG,FSTP_TAG);
#endif
			portflag(P_SETFLAG,HUB_TAG);
			break;
		case HASH_TABLE_SIZE_ARG:
			sscanf(optarg,"%i",&hash_size);
			break;
#ifdef FSTP
		case 'F':
			if (!portflag(P_GETFLAG,HUB_TAG))
				fstflag(P_SETFLAG,FSTP_TAG);
			break;
		case PRIORITY_ARG:
			sscanf(optarg,"%i",&priority);
			priority &= 0xffff;
			break;
#endif
		case MACADDR_ARG:
			{int maci[ETH_ALEN],rv;
				if (index(optarg,':') != NULL)
					rv=sscanf(optarg,"%x:%x:%x:%x:%x:%x", maci+0, maci+1, maci+2, maci+3, maci+4, maci+5);
				else
					rv=sscanf(optarg,"%x.%x.%x.%x.%x.%x", maci+0, maci+1, maci+2, maci+3, maci+4, maci+5);
				if (rv != 6) {
					printlog(LOG_ERR,"Invalid MAC Addr %s",optarg);
					Usage();
				}
				else  {
					int i;
					for (i=0;i<ETH_ALEN;i++)
						switchmac[i]=maci[i];
				}
			}
			break;
		case 'n':
			sscanf(optarg,"%i",&numports);
			break;
		case 'v':
			version();
			break;
		case 'h':
			Usage();
			break;
		default:
			outc=c;
	}
	return outc;
}

static void parse_args(int argc, char **argv)
{
	struct swmodule *swmp;
	struct option *long_options;
	char *optstring;
	static struct option global_options[] = {
		{"help",0 , 0, 'h'},
		{"hub", 0, 0, 'x'},
#ifdef FSTP
		{"fstp",0 , 0, 'F'},
#endif
		{"version", 0, 0, 'v'},
		{"numports", 1, 0, 'n'},
		{"hashsize", 1, 0, HASH_TABLE_SIZE_ARG},
		{"macaddr", 1, 0, MACADDR_ARG},
#ifdef FSTP
		{"priority", 1, 0, PRIORITY_ARG}
#endif
	};
	static struct option optail = {0,0,0,0};
#define N_global_options (sizeof(global_options)/sizeof(struct option))
	prog = argv[0];
	int totopts=N_global_options+1;

	for(swmp=swmh;swmp != NULL;swmp=swmp->next)
		totopts += swmp->swmnopts;
	long_options=malloc(totopts * sizeof(struct option));
	optstring=malloc(2 * totopts * sizeof(char));
	if (long_options == NULL || optstring==NULL)
		exit(2);
	{ /* fill-in the long_options fields */
		int i;
		char *os=optstring;
		char last=0;
		struct option *opp=long_options;
		opp=optcpy(opp,global_options,N_global_options,0);
		for(swmp=swmh;swmp != NULL;swmp=swmp->next)
			opp=optcpy(opp,swmp->swmopts,swmp->swmnopts,swmp->swmtag);
		optcpy(opp,&optail,1,0);
		for (i=0;i<totopts-1;i++)
		{
			int val=long_options[i].val & 0xffff;
			if(val > ' ' && val <= '~' && val != last)
			{
				*os++=val;
				if(long_options[i].has_arg) *os++=':';
			}
		}
		*os=0;
	}
	{
		/* Parse args */
		int option_index = 0;
		int c;
		while (1) {
			c = GETOPT_LONG (argc, argv, optstring,
					long_options, &option_index);
			if (c == -1)
				break;
			c=parse_globopt(c,optarg);
			for(swmp=swmh;swmp != NULL && c!=0;swmp=swmp->next) {
				if (swmp->parseopt != NULL) {
					if((c >> 7) == 0)
						c=swmp->parseopt(c,optarg);
					else if ((c >> 16) == swmp->swmtag)
						swmp->parseopt(c & 0xffff,optarg),c=0;
				}
			}
		}
	}
	if(optind < argc)
		Usage();
	free(long_options);
	free(optstring);
}

static void init_mods(void)
{
	struct swmodule *swmp;

	/* Keep track of the initial cwd */
	int cwfd = open(".", O_RDONLY);

	for(swmp=swmh;swmp != NULL;swmp=swmp->next)
		if (swmp->init != NULL)
		{
			swmp->init();
			if (cwfd >= 0)
				/* Restore cwd so each module will be initialized with the
				 * original cwd also if the previous one changed it. */
				fchdir(cwfd);
		}

	close(cwfd);
}

static void cleanup(void)
{
	struct swmodule *swmp;
	file_cleanup();
	for(swmp=swmh;swmp != NULL;swmp=swmp->next)
		if (swmp->cleanup != NULL)
			swmp->cleanup(0,-1,NULL);
}

static void sig_handler(int sig)
{
	printlog(LOG_ERR,"Caught signal %d, cleaning up and exiting", sig);
	cleanup();
	signal(sig, SIG_DFL);
	if (sig == SIGTERM)
		_exit(0);
	else
		kill(getpid(), sig);
}

static void setsighandlers()
{
	/* setting signal handlers.
	 * sets clean termination for SIGHUP, SIGINT and SIGTERM, and simply
	 * ignores all the others signals which could cause termination. */
	struct { int sig; const char *name; int ignore; } signals[] = {
		{ SIGHUP, "SIGHUP", 0 },
		{ SIGINT, "SIGINT", 0 },
		{ SIGPIPE, "SIGPIPE", 1 },
		{ SIGALRM, "SIGALRM", 1 },
		{ SIGTERM, "SIGTERM", 0 },
		{ SIGUSR1, "SIGUSR1", 1 },
		{ SIGUSR2, "SIGUSR2", 1 },
		{ SIGPROF, "SIGPROF", 1 },
		{ SIGVTALRM, "SIGVTALRM", 1 },
#ifdef VDE_LINUX
		{ SIGPOLL, "SIGPOLL", 1 },
#ifdef SIGSTKFLT
		{ SIGSTKFLT, "SIGSTKFLT", 1 },
#endif
		{ SIGIO, "SIGIO", 1 },
		{ SIGPWR, "SIGPWR", 1 },
#ifdef SIGUNUSED
		{ SIGUNUSED, "SIGUNUSED", 1 },
#endif
#endif
#ifdef VDE_DARWIN
		{ SIGXCPU, "SIGXCPU", 1 },
		{ SIGXFSZ, "SIGXFSZ", 1 },
#endif
		{ 0, NULL, 0 }
	};

	int i;
	for(i = 0; signals[i].sig != 0; i++)
		if(signal(signals[i].sig,
					signals[i].ignore ? SIG_IGN : sig_handler) < 0)
			printlog(LOG_ERR,"Setting handler for %s: %s", signals[i].name,
					strerror(errno));
}

void set_switchmac()
{
	struct timeval v;
	long val;
	int i;
	gettimeofday(&v,NULL);
	srand48(v.tv_sec ^ v.tv_usec ^ getpid());
	for(i=0,val=lrand48();i<4; i++,val>>=8)
		switchmac[i+2]=val;
	switchmac[0]=0;
	switchmac[1]=0xff;
}

static void start_modules(void);

int main(int argc, char **argv)
{
	set_switchmac();
	setsighandlers();
	start_modules();
	parse_args(argc,argv);
	atexit(cleanup);
	hash_init(hash_size);
#ifdef FSTP
	fst_init(numports);
#endif
	port_init(numports);
	init_mods();
	loadrcfile();
	qtimer_init();
	main_loop();
	return 0;
}

/* modules: module references are only here! */
static void start_modules(void)
{
	void start_consmgmt(void);
	void start_datasock(void);
	void start_tuntap(void);
	start_datasock();
	start_consmgmt();
#ifdef HAVE_TUNTAP
	start_tuntap();
#endif
}
