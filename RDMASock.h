/*
 * RDMASock.h
 *
 *  Created on: 2015-5-14
 *      Author: hek
 */

#ifndef RDMAUTILS_H_
#define RDMAUTILS_H_

#include "rdma_utils.h"


class RDMASock
{
public:
	RDMASock();
	virtual ~RDMASock();

public:
	static void sock_init(struct sock_t *sock);
	static int sock_connect(struct sock_bind_t *sock_bind, struct sock_t *sock);
	static int sock_connect_multi(struct sock_bind_t *sock_bind, struct sock_t *sock);
	static int sock_close(struct sock_t *sock);
	static int sock_close_multi(struct sock_t *sock, struct sock_bind_t *sock_bind);
	static int sock_sync_data(struct sock_t *sock, unsigned int size, void *out_buf, void *in_buf);
	static int sock_sync_ready(struct sock_t *sock);
	static int sock_d2c(struct sock_t *sock, unsigned int size, void *buf);
	static int sock_c2d(struct sock_t *sock, unsigned int size, void *buf);
	static int close_sock_fd(int sock_fd);

private:
	static int sock_send(struct sock_t *sock, unsigned int size, void* buf);
	static int sock_recv(struct sock_t *sock, unsigned int size, void* buf);

	static int sock_daemon_wait(unsigned short port, struct sock_bind_t *sock_bind, struct sock_t *sock);
	static int sock_client_connect(char *ip, unsigned short port, struct sock_t *sock);

	static int bind_close(struct sock_bind_t *sock_bind);
};

#endif /* RDMAUTILS_H_ */
