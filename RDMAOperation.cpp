/*
 * RDMAOperation.cpp
 *
 *  Created on: 2015-5-6
 *      Author: hek
 */

#include "RDMAOperation.h"

RDMAOperation::RDMAOperation() {
	// TODO 自动生成的构造函数存根

}

RDMAOperation::~RDMAOperation() {
	// TODO 自动生成的析构函数存根
}

bool RDMAOperation::PostReceive(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	uint32_t i;
	struct ibv_recv_wr *head;
	struct ibv_recv_wr *wr;
	struct ibv_recv_wr *headBak;

	memset(rdma_req->rdma_buf, 0, rdma_req->data_size);
	head = createRecvWRList(t_ctx, rdma_req);
	headBak = head;
	if (!head)
		return false;

	if (rdma_req->post_mode == POST_MODE_ONE_BY_ONE)
	{
		for (i = 0; i < rdma_req->num_of_oust; i++)
		{
			wr = head;
			head = head->next;
			wr->next = NULL;

			if (!doPostRecv(t_ctx, wr))
			{
				ERROR("线程 %u: doPostRecv(list) 失败.\n", t_ctx->thread_id);
				break;
			}
		}

		if (i != rdma_req->num_of_oust)
		{
			// Todo: how to cancel the posted WRs?
			;
		}
	}
	else
	{
		if (!doPostRecv(t_ctx, head))
		{
			ERROR("线程 %u: doPostRecv(one) 失败.\n", t_ctx->thread_id);
		}
	}
	if (headBak == NULL)
		ERROR("[RDMAOperation::PostReceive]headBak is null\n");
	destroyRecvWRList(headBak);
	return true;
}

struct ibv_recv_wr *RDMAOperation::createRecvWRList(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	uint32_t i;
	struct ibv_recv_wr *wr = NULL;
	struct ibv_recv_wr *head = NULL;
	struct ibv_recv_wr *tail = NULL;

	for (i = 0; i < rdma_req->num_of_oust; i++)
	{
		wr = createRecvWR(t_ctx, rdma_req);
		if (wr == NULL)
		{
			ERROR("线程 %u: 创建接收WR失败.\n", t_ctx->thread_id);
			break;
		}

		// dump_recv_wr(qp_ctx, wr);

		if (head == NULL)
		{
			head = tail = wr;
		}
		else
		{
			tail->next = wr;
			tail = wr;
		}
	}

	if (i != rdma_req->num_of_oust)
	{
		destroyRecvWRList(head);
		head = NULL;
	}

	return head;
}

struct ibv_recv_wr *RDMAOperation::createRecvWR(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	struct ibv_recv_wr *recv_wr;
	struct ibv_sge *sge_arr;
	struct ibv_sge *sge;
	uint32_t size;
	uint32_t off = 0;
	uint32_t left;
	uint32_t num_sge;
	int i;

	recv_wr = (struct ibv_recv_wr*) malloc(sizeof(struct ibv_recv_wr));
	if (!recv_wr)
	{
		ERROR("线程 %u: 分配接收 WR 失败.\n", t_ctx->thread_id);
		return NULL;
	}
	memset(recv_wr, 0, sizeof(struct ibv_recv_wr));

	num_sge = rdma_req->data_size / DEF_SG_SIZE + 1;
	if (num_sge > 0)
	{
		// Todo: we need at least one for UD and SRQ(GRH for UD or SRQ with message > 0).
		size = sizeof(struct ibv_sge) * num_sge;
		sge_arr = (struct ibv_sge*) malloc(size);
		if (sge_arr == NULL)
		{
			free(recv_wr);
			ERROR("线程 %u: 为s/g entries array分配空间失败.\n", t_ctx->thread_id);
			return NULL;
		}
		memset(sge_arr, 0, size);
	}

	recv_wr->next = NULL;
	recv_wr->wr_id = (uint64_t) rdma_req->rdma_buf;
	recv_wr->num_sge = num_sge;
	recv_wr->sg_list = sge_arr;

	/// off = RDMA_BUF_HDR_SIZE;

	if (recv_wr->num_sge > 0)
	{
		// when there is a SRQ, leave space for the maximum message that can be reached to the SRQ
		left = rdma_req->data_size;

		// fill the sg_list entry
		for (i = 0; i < recv_wr->num_sge; i++)
		{
			sge = &(recv_wr->sg_list[i]);

			sge->lkey = t_ctx->local_mr->lkey;
			sge->addr = (uint64_t) (rdma_req->rdma_buf);
			// in case of SRQ: put all the extra bytes in the last s/g in the list
			if (left > DEF_SG_SIZE)
			{
				sge->length = DEF_SG_SIZE;
			}
			else
			{
				sge->length = left;
			}

			off += sge->length;
			left -= sge->length;
		}
	}

	return recv_wr;
}

bool RDMAOperation::doPostRecv(struct thread_context_t *t_ctx, struct ibv_recv_wr *wr)
{
	int rc = 0;
	struct ibv_recv_wr *bad_wr;

	rc = ibv_post_recv(t_ctx->qp, wr, &bad_wr);
	if (rc)
	{
		//dumpRecvWR(wr);

		ERROR("ibv_post_recv(one) 返回值: %d, 原因: %s.\n", rc, strerror(errno));
		return false;
	}

	return true;
}

void RDMAOperation::destroyRecvWRList(struct ibv_recv_wr *head)
{
	struct ibv_recv_wr *wr;

	while (head)
	{
		wr = head;
		head = head->next;
		destroyRecvWR(wr);
	}
}

void RDMAOperation::destroyRecvWR(struct ibv_recv_wr *recv_wr)
{
	if (recv_wr)
	{
		if (recv_wr->sg_list)
		{
			free(recv_wr->sg_list);
		}
		free(recv_wr);
	}
}



bool RDMAOperation::PostSend(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	uint32_t i;
	int rc = 0;
	struct ibv_send_wr *head;
	struct ibv_send_wr *wr;
	struct ibv_send_wr *bad_wr;
	struct ibv_send_wr* headBak;

	head = createSendWRList(t_ctx, rdma_req);
	headBak = head;
	if (head == NULL)
	{
		ERROR("线程 %u: 创建发送WR失败.\n", t_ctx->thread_id);
		return false;
	}
	TRACE("线程 %u: 创建发送WR成功.\n", t_ctx->thread_id);

	if (rdma_req->post_mode == POST_MODE_ONE_BY_ONE)
	{
		for (i = 0; i < rdma_req->num_of_oust; i++)
		{
			wr = head;
			head = head->next;
			wr->next = NULL;

			rc = ibv_post_send(t_ctx->qp, wr, &bad_wr);
			if (rc)
			{
				ERROR("线程 %u: ibv_post_send(list) 失败.\n", t_ctx->thread_id);
				break;
			}
			TRACE("线程 %u: ibv_post_send(list) 成功.\n", t_ctx->thread_id);
		}

		if (i != rdma_req->num_of_oust)
		{
			// Todo: how to cancel the posted WRs?
			;
		}

	} else
	{
		rc = ibv_post_send(t_ctx->qp, head, &bad_wr);
		if (rc)
		{
			ERROR("线程 %u: ibv_post_send(one) 失败.\n", t_ctx->thread_id);
		}
		TRACE("线程 %u: ibv_post_send(one) 成功.\n", t_ctx->thread_id);
	}

	if (headBak == NULL)
		ERROR("[RDMAOperation:PostSend]headBak is null\n");
	destroySendWRList(headBak);
	return (rc == 0);
}

struct ibv_send_wr *RDMAOperation::createSendWRList(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	uint32_t i;
	struct ibv_send_wr *wr = NULL;
	struct ibv_send_wr *head = NULL;
	struct ibv_send_wr *tail = NULL;

	for (i = 0; i < rdma_req->num_of_oust; i++)
	{
		wr = createSendWR(t_ctx, rdma_req);
		if (wr == NULL)
		{
			ERROR("线程 %u: 创建发送WR失败.\n", t_ctx->thread_id);
			break;
		}

		// dump_send_wr(qp_ctx, wr);
		if (head == NULL)
		{
			head = tail = wr;
		}
		else
		{
			tail->next = wr;
			tail = wr;
		}
	}

	if (i != rdma_req->num_of_oust)
	{
		ERROR("线程 %u: num_of_oust错误.\n", t_ctx->thread_id);
		destroySendWRList(head);
		head = NULL;
	}

	return head;
}

struct ibv_send_wr *RDMAOperation::createSendWR(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	struct ibv_send_wr *send_wr;
	struct ibv_sge *sge_arr = NULL;
	struct ibv_sge *sge;
	uint32_t size;
	uint32_t off = 0;
	uint32_t left = 0;
	uint32_t num_sge;
	enum ibv_wr_opcode opcode = rdma_req->opcode;
	int send_flags;
	int i;


	send_wr = (struct ibv_send_wr*) malloc(sizeof(struct ibv_send_wr));
	if (send_wr == NULL)
	{
		ERROR("线程 %u: 分配发送WR失败.\n", t_ctx->thread_id);
		return NULL;
	}
	memset(send_wr, 0, sizeof(struct ibv_send_wr));

	num_sge = rdma_req->data_size / DEF_SG_SIZE + 1;
	if (num_sge > 0)
	{
		size = sizeof(struct ibv_sge) * num_sge;
		sge_arr = (struct ibv_sge*) malloc(size);
		if (!sge_arr)
		{
			free(send_wr);
			ERROR("线程 %u: 分配s/g entries array失败.\n", t_ctx->thread_id);
			return NULL;
		}
		memset(sge_arr, 0, size);
	}

	/*
	 * IBV_SEND_FENCE - Set the fence indicator for this WR. This means that the processing of this WR
	 *     will be blocked until all prior posted RDMA Read and Atomic WRs will be completed. Valid only
	 *     for QPs with Transport Service Type IBV_QPT_RC
	 *
	 * IBV_SEND_SIGNALED - Set the completion notification indicator for this WR. This means that if the
	 *     QP was created with sq_sig_all=0, a Work Completion will be generated when the processing of
	 *     this WR will be ended. If the QP was created with sq_sig_all=1, there won't be any effect to
	 *     this flag
	 *
	 * IBV_SEND_SOLICITED - Set the solicited event indicator for this WR. This means that when the message
	 *     in this WR will be ended in the remote QP, a solicited event will be created to it and if in the
	 *     remote side the user is waiting for a solicited event, it will be waken up. Relevant only for the
	 *     Send and RDMA Write with immediate opcodes
	 *
	 * IBV_SEND_INLINE - The memory buffers specified in sg_list will be placed inline in the Send Request.
	 *     This mean that the low-level driver (i.e. CPU) will read the data and not the RDMA device. This
	 *     means that the L_Key won't be checked, actually those memory buffers don't even have to be
	 *     registered and they can be reused immediately after ibv_post_send() will be ended. Valid only
	 *     for the Send and RDMA Write opcodes
	 *
	 */
	send_flags = IBV_SEND_SIGNALED;
	if (0)
	{
		// Todo:
		send_flags |= IBV_SEND_INLINE; // Todo: what is inline data.
		send_flags |= IBV_SEND_SOLICITED;
		send_flags |= IBV_SEND_INLINE;
	}

	send_wr->next = NULL;
	send_wr->wr_id = (uint64_t) rdma_req->rdma_buf;
	send_wr->opcode = opcode;
	send_wr->send_flags = send_flags;
	send_wr->num_sge = num_sge;
	send_wr->sg_list = sge_arr;
	if ((opcode == IBV_WR_SEND_WITH_IMM) || (opcode
			== IBV_WR_RDMA_WRITE_WITH_IMM))
	{
		// Todo: how to handle imm data for send_imm and rdma_write_imm?
		t_ctx->imm_qp_ctr_snd++;
		send_wr->imm_data = htonl(t_ctx->imm_qp_ctr_snd);
	}

	switch (opcode)
	{
	case IBV_WR_ATOMIC_CMP_AND_SWP:
		send_wr->wr.atomic.swap = 1;
	case IBV_WR_ATOMIC_FETCH_AND_ADD:
		send_wr->wr.atomic.compare_add = 1;
		send_wr->wr.atomic.remote_addr = (uint64_t) (rdma_req->peer_buf);
		send_wr->wr.atomic.rkey = rdma_req->peer_mr.rkey;
		break;

	case IBV_WR_RDMA_WRITE:
	case IBV_WR_RDMA_WRITE_WITH_IMM:
	case IBV_WR_RDMA_READ:
		send_wr->wr.rdma.remote_addr = (uint64_t) (rdma_req->peer_buf);
		send_wr->wr.rdma.rkey = rdma_req->peer_mr.rkey;
		break;

	default:
		; // send and send_imm do not directly access peer buffer.
	}

	if (send_wr->num_sge > 0)
	{
		off = 0;
		left = rdma_req->data_size;

		for (i = 0; i < send_wr->num_sge; i++)
		{
			sge = &(send_wr->sg_list[i]);

			sge->lkey = t_ctx->local_mr->lkey;
			sge->addr = (uint64_t) (rdma_req->rdma_buf + off); /// The algorithm needs to be checked.
			if (left > DEF_SG_SIZE)
			{
				sge->length = DEF_SG_SIZE;
			}
			else
			{
				sge->length = left;
			}

			off += sge->length;
			left -= sge->length;
		}
	}

	return send_wr;
}

void RDMAOperation::destroySendWRList(struct ibv_send_wr *head)
{
	struct ibv_send_wr *wr;

	while (head)
	{
		wr = head;
		head = head->next;
		destroySendWR(wr);
	}
}

void RDMAOperation::destroySendWR(struct ibv_send_wr *send_wr)
{
	if (send_wr)
	{
		if (send_wr->sg_list)
		{
			free(send_wr->sg_list);
		}
		free(send_wr);
	}
}


void RDMAOperation::PrintCompletion(int thread_id, int ts_type, const struct ibv_wc *compl_data)
{
	printf("thread %d: -------------------------------------------", thread_id);
	printf("thread %d: wr_id: %p", thread_id, (void*)compl_data->wr_id);
	printf("thread %d: status: %d", thread_id, compl_data->status);
	printf("thread %d: qp_num: 0x%x", thread_id, compl_data->qp_num);

	/* print various props if the completion is good */
	if (compl_data->status == IBV_WC_SUCCESS)
	{
		printf("thread %d: opcode: %d", thread_id, compl_data->opcode);
		/* print the byte_len only when it is valid */
		if ((compl_data->opcode & IBV_WC_RECV) || (compl_data->opcode
				== IBV_WC_RDMA_READ)
				|| (compl_data->opcode == IBV_WC_FETCH_ADD)
				|| (compl_data->opcode == IBV_WC_COMP_SWAP))
			printf("thread %d: byte_len: 0x%x", thread_id, compl_data->byte_len);

		printf("thread %d: wc_flags: 0x%x", thread_id, compl_data->wc_flags);
		if (compl_data->wc_flags & IBV_WC_WITH_IMM)
			printf("thread %d: immediate_data: 0x%x", thread_id, ntohl(compl_data->imm_data));
		if (compl_data->wc_flags & IBV_WC_GRH)
			printf("thread %d: GRH is present", thread_id);
	} else
		/* otherwise, print the vendor syndrome */printf("thread %d: vendor_err: 0x%x", thread_id, compl_data->vendor_err);

	printf("thread %d: -------------------------------------------", thread_id);
}


void RDMAOperation::dumpRecvWR(struct ibv_recv_wr *recv_wr)
{
	int i;
	struct ibv_sge *sge;

	DEBUG("\nibv_recv_wr:\n");
	DEBUG("\trecv_wr->next               :%p\n", recv_wr->next);
	DEBUG("\trecv_wr->wr_id              :%lx\n", recv_wr->wr_id);
	DEBUG("\trecv_wr->num_sge            :%x\n", recv_wr->num_sge);
	DEBUG("\trecv_wr->sg_list            :%p\n", recv_wr->sg_list);

	if (recv_wr->num_sge > 0) {
		DEBUG("\n\trecv_wr->sge_list:\n");

		for (i = 0; i < recv_wr->num_sge; i++) {
			sge = &(recv_wr->sg_list[i]);
			DEBUG("\trecv_wr->sg_list[%d].lkey    :%x\n", i, sge->lkey);
			DEBUG("\trecv_wr->sg_list[%d].addr    :%lx\n", i, sge->addr);
			DEBUG("\trecv_wr->sg_list[%d].length  :%x\n", i, sge->length);
		}
	}
}


void RDMAOperation::dumpSendWR(struct thread_context_t *t_ctx, struct ibv_send_wr *send_wr)
{
	int i;
	struct ibv_sge *sge;

	DEBUG("\nibv_send_wr:\n");
	DEBUG("\tsend_wr->next               :%p\n", send_wr->next);
	DEBUG("\tsend_wr->wr_id              :%lx\n", send_wr->wr_id);
	DEBUG("\tsend_wr->opcode             :%x\n", send_wr->opcode);
	DEBUG("\tsend_wr->send_flags         :%x\n", send_wr->send_flags);
	DEBUG("\tsend_wr->num_sge            :%x\n", send_wr->num_sge);
	DEBUG("\tsend_wr->sg_list            :%p\n", send_wr->sg_list);

	if ((send_wr->opcode == IBV_WR_SEND_WITH_IMM) || (send_wr->opcode
			== IBV_WR_RDMA_WRITE_WITH_IMM))
	{
		DEBUG("\n\tsend_wr->imm_data          :%x\n", send_wr->imm_data);
	}

	switch (send_wr->opcode)
	{
	case IBV_WR_ATOMIC_CMP_AND_SWP:
		DEBUG("\t\tsend_wr->wr.atomic.swap                :%lx\n", send_wr->wr.atomic.swap);
	case IBV_WR_ATOMIC_FETCH_AND_ADD:
		DEBUG("\t\tsend_wr->wr.atomic.compare_add         :%lx\n", send_wr->wr.atomic.compare_add);
		DEBUG("\t\tsend_wr->wr.atomic.remote_addr         :%lx\n", send_wr->wr.atomic.remote_addr);
		DEBUG("\t\tsend_wr->wr.atomic.rkey                :%x\n", send_wr->wr.atomic.rkey);
		break;

	case IBV_WR_RDMA_WRITE:
	case IBV_WR_RDMA_WRITE_WITH_IMM:
	case IBV_WR_RDMA_READ:

		DEBUG("\t\tsend_wr->wr.rdma.remote_addr           :%lx\n", send_wr->wr.rdma.remote_addr);
		DEBUG("\t\tsend_wr->wr.rdma.rkey                  :%x\n", send_wr->wr.rdma.rkey);
		break;

	default:
		; // send and send_imm does not directly access peer buffer.
	}

	if (send_wr->num_sge > 0)
	{
		DEBUG("\n\tsend_wr->sg_list:\n");

		for (i = 0; i < send_wr->num_sge; i++)
		{
			sge = &(send_wr->sg_list[i]);
			DEBUG("\t\tsend_wr->sg_list[%d].lkey    :0x%x\n", i, sge->lkey);
			DEBUG("\t\tsend_wr->sg_list[%d].addr    :0x%lx\n", i, sge->addr);
			DEBUG("\t\tsend_wr->sg_list[%d].length  :0x%x\n", i, sge->length);
		}
	}
}














