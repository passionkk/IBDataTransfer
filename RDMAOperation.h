/*
 * RDMAOperation.h
 *
 *  Created on: 2015-5-6
 *      Author: hek
 */

#ifndef RDMAOPERATION_H_
#define RDMAOPERATION_H_

#include "rdma_mt.h"

class RDMAOperation
{
public:
	RDMAOperation();
	virtual ~RDMAOperation();

public:
	static bool PostReceive(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);
	static bool PostSend(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);
	static void PrintCompletion(int thread_id, int ts_type, const struct ibv_wc *compl_data);

private:
	static struct ibv_recv_wr *createRecvWRList(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);
	static struct ibv_recv_wr *createRecvWR(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);

	static bool doPostRecv(struct thread_context_t *t_ctx, struct ibv_recv_wr *wr);

	static void destroyRecvWRList(struct ibv_recv_wr *head);
	static void destroyRecvWR(struct ibv_recv_wr *recv_wr);

private:
	static struct ibv_send_wr *createSendWRList(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);
	static struct ibv_send_wr *createSendWR(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);

	static void destroySendWRList(struct ibv_send_wr *head);
	static void destroySendWR(struct ibv_send_wr *send_wr);

private:
	static void dumpRecvWR(struct ibv_recv_wr *recv_wr);
	static void dumpSendWR(struct thread_context_t *t_ctx, struct ibv_send_wr *send_wr);

};

#endif /* RDMAOPERATION_H_ */
