#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include <vde.h>
#include <vdecommon.h>

#include <vdeplugin.h>


static int testevent(struct dbgcl *tag,void *arg,va_list v);
static int dump(char *arg);

struct plugin vde_plugin_data={
	.name="dump",
	.help="dump packets",
};

static struct comlist cl[]={
	{"dump","============","DUMP Packets",NULL,NOARG},
	{"dump/active","0/1","start dumping data",dump,STRARG},
};

#define D_DUMP 0100 
static struct dbgcl dl[]= {
	 {"dump/packetin","dump incoming packet",D_DUMP|D_IN},
	 {"dump/packetout","dump outgoing packet",D_DUMP|D_OUT},
};

static int dump(char *arg)
{
	int active=atoi(arg);
	if (active)
		eventadd(testevent,"packet",dl);
	else
		eventdel(testevent,"packet",dl);
	return 0;
}

static int testevent(struct dbgcl *event,void *arg,va_list v)
{
	struct dbgcl *this=arg;
	switch (event->tag) {
		case D_PACKET|D_OUT: 
			this++;
		case D_PACKET|D_IN: 
			{
				int port=va_arg(v,int);
				unsigned char *buf=va_arg(v,unsigned char *);
				int len=va_arg(v,int);
				char *pktdump;
				size_t dumplen;
				FILE *out=open_memstream(&pktdump,&dumplen);
				if (out) {
					int i;
					fprintf(out,"Pkt: Port %04d len=%04d ",
							port,
							len);
					for (i=0;i<len;i++)
						fprintf(out,"%02x ",buf[i]);
					fclose(out);
					DBGOUT(this, "%s",pktdump);
					free(pktdump);
				}
			}
	}
	return 0;
}

	static void
	__attribute__ ((constructor))
init (void)
{
	ADDCL(cl);
	ADDDBGCL(dl);
}

	static void
	__attribute__ ((destructor))
fini (void)
{
	DELCL(cl);
	DELDBGCL(dl);
}
