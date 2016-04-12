/*
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
 *
 */

#ifndef VL_SOCK_H
#define VL_SOCK_H



#include <netinet/in.h>
#include <arpa/inet.h>
#include <byteswap.h>

#ifdef __cplusplus
extern "C" {
#endif


#define IP_STR_LENGTH 15
#define SOCKET_RETRY  10

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ntohll(x) bswap_64(x)
#define htonll(x) bswap_64(x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ntohll(x) x
#define htonll(x) x
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

struct sock_t{
    int            sock_fd;
    int            is_daemon;
    unsigned short port;
    char           ip[IP_STR_LENGTH+1];

};

struct sock_bind_t {
    unsigned int counter;
    int          socket_fd;
    int            is_daemon;
    unsigned short port;
    char           ip[IP_STR_LENGTH+1];
};

#ifdef __cplusplus
}
#endif

#endif
