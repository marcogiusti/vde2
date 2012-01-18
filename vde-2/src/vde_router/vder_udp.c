#include "vder_udp.h"
#include <stdio.h>
#include <unistd.h>

/* UDP header, rfc 768 */

struct __attribute__((packed)) udphdr {
	uint16_t sport, dport, len, crc;
};

static struct vder_udp_socket *socket_list = NULL;


/* interface toward the router */
int vder_udp_recv(struct vde_buff *buf)
{
	struct vder_udp_socket *cur = socket_list;
	int found = 0;
	struct vde_buff *copy = NULL;
	uint16_t port = transport_dport(buf);

	while(cur) {
		if (cur->port == port) {
			if (!found) {
				enqueue(&cur->inq, buf);
				found = 1;
			} else {
				copy = malloc(sizeof(struct vde_buff) + buf->len);
				if (!copy)
					break;
				memcpy(copy, buf, sizeof(struct vde_buff) + buf->len);
				enqueue(&cur->inq, copy);
			}
		}
		cur = cur->next;
	}
	return found;
}

struct vder_udp_socket *vder_udpsocket_open(uint16_t port)
{

	struct vder_udp_socket *vu;

	if (port == 0) {
		errno = EINVAL;
		return NULL;
	}

	vu = malloc(sizeof(struct vder_udp_socket));
	if (!vu)
		return NULL;

	memset(&vu->inq, 0, sizeof(struct vder_queue));
	pthread_mutex_init(&vu->inq.lock, NULL);
	qfifo_setup(&vu->inq, UDPSOCK_BUFFER_SIZE);
	vu->port = port;
	vu->next = socket_list;
	socket_list = vu;
	return vu;
}

void vder_udp_close(struct vder_udp_socket *sock)
{
	struct vder_udp_socket *prev = NULL, *cur = socket_list;
	while(cur) {
		if (cur == sock) {
			if (!prev) {
				socket_list = cur->next;
			} else {
				prev->next = cur->next;
			}
			free(sock);
			return;
		}
		prev = cur;
		cur = cur->next;
	}
}


int vder_udpsocket_sendto(struct vder_udp_socket *sock, void *data, size_t len, uint32_t dst, uint16_t dstport)
{
	struct vde_buff *b;
	struct udphdr *uh;
	uint8_t *datagram;
	struct vder_route *ro;
	int bufsize;
	if (len <= 0) {
		errno = EINVAL;
		return -1;
	}
	len += sizeof(struct udphdr);

	ro = vder_get_route(dst);
	if (!ro) {
		errno = EHOSTUNREACH;
		return -1;
	}

	bufsize = sizeof(struct vde_buff) + sizeof(struct vde_ethernet_header) + sizeof(struct iphdr) + sizeof(struct udphdr) + len;
	b = malloc(bufsize);
	if (!b)
		return -1;
	b->len = bufsize - sizeof(struct vde_buff);
	b->src = NULL;
	b->priority = PRIO_BESTEFFORT;
	uh = (struct udphdr *) payload(b);
	datagram = (uint8_t *)((payload(b) + sizeof(struct udphdr)));
	memcpy(datagram, data, len);

	uh->sport = sock->port;
	uh->dport = dstport;
	uh->len = htons(len);
	uh->crc = 0;
	vder_packet_send(b, dst, PROTO_UDP);
	return len;
}

int vder_udpsocket_sendto_broadcast(struct vder_udp_socket *sock, void *data, size_t len,
	struct vder_iface *iface, uint32_t dst, uint16_t dstport)
{
	struct vde_buff *b;
	struct udphdr *uh;
	uint8_t *datagram;
	int bufsize;
	if (len <= 0) {
		errno = EINVAL;
		return -1;
	}
	len += sizeof(struct udphdr);

	bufsize = sizeof(struct vde_buff) + sizeof(struct vde_ethernet_header) + sizeof(struct iphdr) + sizeof(struct udphdr) + len;
	b = malloc(bufsize);
	if (!b)
		return -1;
	b->len = bufsize - sizeof(struct vde_buff);
	b->src = NULL;
	b->priority = PRIO_BESTEFFORT;
	uh = (struct udphdr *) payload(b);
	datagram = (uint8_t *)((payload(b) + sizeof(struct udphdr)));
	memcpy(datagram, data, len);

	uh->sport = sock->port;
	uh->dport = dstport;
	uh->len = htons(len);
	uh->crc = 0;
	vder_packet_broadcast(b, iface, dst, PROTO_UDP);
	return len;
}


int vder_udpsocket_recvfrom(struct vder_udp_socket *sock, void *data, size_t len, uint32_t *from, uint16_t *fromport, int timeout)
{
	struct vde_buff *b;
	struct udphdr *uh;
	uint8_t *datagram;

	if (len <= 0) {
		errno = EINVAL;
		return -1;
	}

	while ((timeout > 0) && (sock->inq.n == 0)) {
		usleep(10000);
		timeout -= 10;
		if (timeout < 0)
			timeout = 0;
	}

	if ((timeout == 0) && (sock->inq.n == 0)) {
		return 0;
	}

	do {
		b = dequeue(&sock->inq);
	} while(!b);
	uh = (struct udphdr *) payload(b);
	datagram = (uint8_t *)(payload(b) + sizeof(struct udphdr));
	if (ntohs(uh->len) < len)
		len = ntohs(uh->len) - sizeof (struct udphdr);
	memcpy(data, datagram, len);
	*fromport = uh->sport;
	return len;
}


