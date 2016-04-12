/*
 * RDMASock.cpp
 *
 *  Created on: 2015-5-14
 *      Author: hek
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>


#include "RDMASock.h"


RDMASock::RDMASock() {
	// TODO 自动生成的构造函数存根

}

RDMASock::~RDMASock() {
	// TODO 自动生成的析构函数存根
}

int RDMASock::sock_close(struct sock_t *sock)
{
	return close_sock_fd(sock->sock_fd);
}



void RDMASock::sock_init(struct sock_t *sock)
{
	sock->sock_fd = -1;
}



int RDMASock::sock_c2d(struct sock_t *sock, unsigned int size, void *buf)
{
	return ((!sock->is_daemon) ? sock_send(sock, size, buf) : sock_recv(sock, size, buf));
}



int RDMASock::sock_sync_ready(struct sock_t *sock)
{
	char cm_buf = 'a';
	return sock_sync_data(sock, sizeof(cm_buf), &cm_buf, &cm_buf);
}



int RDMASock::sock_d2c(struct sock_t *sock, unsigned int size, void *buf)
{
	return ((sock->is_daemon) ? sock_send(sock, size, buf) : sock_recv(sock, size, buf));
}


int RDMASock::sock_connect_multi(struct sock_bind_t *sock_bind, struct sock_t *sock)
{
	 int rc;

	if (sock_bind->is_daemon)
	{
		printf("Running sock_daemon_wait.\n");
		rc = sock_daemon_wait(sock_bind->port, sock_bind, sock);
	}
	else
	{
		printf("Running sock_client_connect, Server IP: %s\n", sock_bind->ip);
		rc = sock_client_connect(sock_bind->ip, sock_bind->port, sock);
	}

	return rc;
}



int RDMASock::sock_sync_data(struct sock_t *sock, unsigned int size, void *out_buf, void *in_buf)
{
	int rc;

	if (sock->is_daemon)
	{
		rc = sock_send(sock, size, out_buf);
		if (rc != 0)
			return rc;
		rc = sock_recv(sock, size, in_buf);
		if (rc != 0)
			return rc;
	}
	else
	{
		rc = sock_recv(sock, size, in_buf);
		if (rc != 0)
			return rc;
		rc = sock_send(sock, size, out_buf);
		if (rc != 0)
			return rc;
	}

	return 0;
}



int RDMASock::sock_close_multi(struct sock_t *sock, struct sock_bind_t *sock_bind)
{
	int rc;

	printf("Closing sock->sock_fd.\n");
	rc = close_sock_fd(sock->sock_fd);
	if (sock->is_daemon)
	{
		if (sock_bind->counter == 0)
		{
			printf("counter of the bind contains zero.\n");
			return -1;
		}

		sock_bind->counter--;
		if (sock_bind->counter == 0)
		{
			printf("Closing sock_bind->socket_fd.\n");
			rc |= close_sock_fd(sock_bind->socket_fd);
		}
	}

	return rc;
}



int RDMASock::close_sock_fd(int sock_fd)
{
	int rc;

	if (sock_fd == -1)
	{
		printf("sock_fd is not allocated.\n");
		return -1;
	}

	rc = close(sock_fd);
	if (rc != 0)
	{
		printf("close socket failed: %s.\n", strerror(errno));
		return -1;
	}

	return 0;
}



int RDMASock::sock_connect(struct sock_bind_t *sock_bind, struct sock_t *sock)
{
	int rc;

	if (sock_bind->is_daemon)
	{
		rc = sock_daemon_wait(sock_bind->port, sock_bind, sock);
		rc |= bind_close(sock_bind); /// bug
	}
	else
	{
		rc = sock_client_connect(sock_bind->ip, sock_bind->port, sock);
	}

	return rc;
}



int RDMASock::sock_send(struct sock_t *sock, unsigned int size, void *buf)
{
	int rc;

	if (sock->sock_fd == -1)
	{
		printf("socket_fd is not allocated.\n");
		return EINVAL;
	}

retry_after_signal:
	rc = send(sock->sock_fd, buf, size, 0);
	if (rc != (int)size)
	{
		if ((errno == EINTR) && (rc != 0)) // ???
			goto retry_after_signal;
		if (rc)
			return rc;
		else
			return errno;
	}

	printf("socket_fd = 0x%x, Sent buf=%p size=%d rc=%d.\n", sock->sock_fd,
			buf, size, rc);
	return 0;
}

int RDMASock::sock_recv(struct sock_t *sock, unsigned int size, void *buf)
{
	int rc;

	if (sock->sock_fd == -1)
	{
		printf("socket_fd is not allocated.\n");
		return -1;
	}

retry_after_signal:

	rc = recv(sock->sock_fd, buf, size, MSG_WAITALL);
	if (rc != (int)size)
	{
		printf("recv failed, sock_fd: %d, rc: %d, error: %s.\n", sock->sock_fd,
				rc, strerror(errno));

		if ((errno == EINTR) && (rc != 0)) // ???
			goto retry_after_signal;

		if (rc)
			return rc;
		else
			return errno;
	}

	printf("socket_fd = 0x%x, Received buf=%p size=%d rc=%d.\n", sock->sock_fd,
			buf, size, rc);
	return 0;
}

int RDMASock::sock_daemon_wait(unsigned short  port, struct sock_bind_t *sock_bind, struct sock_t *sock)
{
	struct sockaddr_in remote_addr;
	struct protoent *protoent;
	struct sockaddr_in my_addr;
	socklen_t addr_len;
	int rc;
	int tmp;

	sock->port = port;
	sock->is_daemon = 1;

	if (sock_bind->counter == 0)
	{
		sock_bind->socket_fd = socket(PF_INET, SOCK_STREAM, 0);
		if (sock_bind->socket_fd == -1)
		{
			printf("socket failed, reason: %s.\n", strerror(errno));
			return -1;
		}
		printf(("TCP socket was created.\n"));

		tmp = 1;
		setsockopt(sock_bind->socket_fd, SOL_SOCKET, SO_REUSEADDR,
				(char *) &tmp, sizeof(tmp));

		my_addr.sin_family = PF_INET;
		my_addr.sin_port = htons(port);
		my_addr.sin_addr.s_addr = INADDR_ANY;
		rc = bind(sock_bind->socket_fd, (struct sockaddr *) (&my_addr),
				sizeof(my_addr));
		if (rc == -1)
		{
			printf("bind failed: %s.\n", strerror(errno));
			return -1;
		}
		printf("Start listening on port %u.\n", port);

		rc = listen(sock_bind->socket_fd, 1);
		if (rc == -1) {
			printf("listen failed: %s.\n", strerror(errno));
			return -1;
		}
	}

	addr_len = sizeof(remote_addr);
	sock->sock_fd = accept(sock_bind->socket_fd,
			(struct sockaddr *) (&remote_addr), &addr_len);
	if (sock->sock_fd == -1)
	{
		printf("accept failed: %s.\n", strerror(errno));
		return -1;
	}
	printf("Accepted connection from IP = %s and port = %u.\n",
			inet_ntoa(remote_addr.sin_addr), remote_addr.sin_port);

	protoent = getprotobyname("tcp");
	if (!protoent)
	{
		printf("getprotobyname error.\n");
		return -1;
	}

	tmp = 1;
	setsockopt(sock->sock_fd, protoent->p_proto, TCP_NODELAY, (char *) &tmp,
			sizeof(tmp));
	signal(SIGPIPE, SIG_IGN); /* If we don't do this, then gdbserver simply exits when the remote side dies.  */
	sock_bind->counter++;

	printf("Connection was established on port %u, socket_fd = 0x%x.\n", port,
			sock->sock_fd);
	return 0;
}

int RDMASock::sock_client_connect(char *ip, unsigned short  port, struct sock_t *sock)
{
	struct in_addr remote_addr;
	struct sockaddr_in server_addr;
	int rc;
	int i;

	sock->sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock->sock_fd == -1)
	{
		printf("socket failed: %s.\n", strerror(errno));
		return -1;
	}

	sock->port = port;
	strncpy(sock->ip, ip, IP_STR_LENGTH);
	if (inet_aton(ip, &remote_addr) == 0)
	{
		printf("inet_aton failed: %s.\n", strerror(errno));
		return -1;
	}

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = remote_addr;
	printf("Connecting to IP %s, port %u.\n", sock->ip, port);

	i = 0;
	do {
		rc = connect(sock->sock_fd, (struct sockaddr *) (&server_addr),
				sizeof(server_addr));
		if (rc == -1)
		{
			sleep(1);
			printf("connect[%d] failed, reason %s.\n", i, strerror(errno));
		}

		i++;

	} while ((i < SOCKET_RETRY) && (rc == -1));

	if (rc == -1)
	{
		printf("Connecting failed: %s.\n", strerror(errno));
		return -1;
	}

	printf("Connection was established to IP %s, port %u, socket_fd = 0x%x.\n",
			sock->ip, port, sock->sock_fd);
	sock->is_daemon = 0;
	return 0;
}

int RDMASock::bind_close(struct sock_bind_t *sock_bind)
{
	return close_sock_fd(sock_bind->socket_fd);
}












