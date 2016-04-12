
//#include <QtCore/QCoreApplication>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IBDT_ErrorDefine.h"

#include "rdma_mt.h"

#include "RDMAParser.h"
#include "RDMAResource.h"
#include "RDMAThread.h"
#include "RDMASock.h"

static struct rdma_resource_t rdma_resource;
uint32_t volatile stop_all = 0;
int g_lastErrorCode = IBDT_OK;


static void do_signal(int dunno)
{
    switch (dunno) {
    case 1:
        printf("Get a signal -- SIGHUP ");
        stop_all = 1;
        break;
    case 2:
    case 3:

    default:
        break;
    }

    return;
}

static void signal_init()
{
    signal(SIGHUP, do_signal);
    return;
}

int main(int argc, char *argv[])
{
    struct sock_t sock;
    struct sock_bind_t sock_bind;
    struct user_param_t *user_param = &(rdma_resource.user_param);

    RDMAParser parser;
    if(!parser.Parse(&(rdma_resource.user_param), argv, argc))
    {
        g_lastErrorCode = IBDT_E_InvalidParam;
        return g_lastErrorCode;
    }

    sock_bind.socket_fd = -1;
    sock_bind.counter = 0;
    sock_bind.is_daemon = (user_param->server_ip == NULL) ? 1 : 0;
    sock_bind.port = user_param->tcp_port;
    if (user_param->server_ip)
    {
        strcpy(sock_bind.ip, user_param->server_ip);
    }

    signal_init();
    RDMASock::sock_init(&sock);

    if(!RDMAResource::InitResource(&sock, &rdma_resource))
    {
        goto cleanup;
    }

    if(!RDMAThread::StartRDMAThreads(&rdma_resource, &sock_bind))
    {
        printf("Testing failed\n");
    }

    while(!stop_all)
    {
        if(!sock_bind.is_daemon)
            break;
    }

cleanup:
    RDMAResource::DestroyResource(&rdma_resource);

    return g_lastErrorCode;
}
