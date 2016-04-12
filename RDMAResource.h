/*
 * RDMAResource.h
 *
 *  Created on: 2015-5-6
 *      Author: hek
 */

#ifndef RDMARESOURCE_H_
#define RDMARESOURCE_H_

#include "rdma_mt.h"

class RDMAResource
{
public:
	RDMAResource();
	virtual ~RDMAResource();


public:
	static bool InitResource(struct sock_t *m_sock, struct rdma_resource_t *m_res);
	static bool DestroyResource(struct rdma_resource_t *m_res);

	// QP
	static bool CreateQP(struct thread_context_t *t_ctx);
	static bool ConnectQP(struct thread_context_t *t_ctx);
	static bool DisConnectQP(struct thread_context_t *t_ctx);
	static void DestroyQP(struct thread_context_t *t_ctx);

	// rdma buf pool
	static bool CreateRDMABufPool(struct thread_context_t *t_ctx);
	static bool FreeRDMAPool(struct thread_context_t* t_ctx);
	static void GetRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t*& rdma_buf);
	static void GetRDMAPayloadBuf(struct rdma_buf_t *rdma_buf, char*& payload_buf);
	static void PutRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t *rdma_buf);


private:
	static bool initIBDevice(struct rdma_resource_t *m_res);

	static bool createCQ(struct thread_context_t *t_ctx);

	static bool setQPAttr(struct thread_context_t *t_ctx);
	static bool modifyQPToInit(struct thread_context_t *t_ctx);
	static bool modifyQPToRTR(struct thread_context_t *t_ctx);
	static bool modifyQPToRTS(struct thread_context_t *t_ctx);

private:
	// 打印输出
	static void dumpQPInitAttr(struct ibv_qp_init_attr *qp_init_attr);
	static void dumpQPAttr(struct thread_context_t *t_ctx);
};

#endif /* RDMARESOURCE_H_ */
