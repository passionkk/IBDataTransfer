/*
 * RDMAResource.cpp
 *
 *  Created on: 2015-5-6
 *      Author: hek
 */

#include "RDMAResource.h"
#include "RDMAThread.h"

#include "IBDT_ErrorDefine.h"

RDMAResource::RDMAResource() {
	// TODO 自动生成的构造函数存根

}

RDMAResource::~RDMAResource() {
	// TODO 自动生成的析构函数存根
}

bool RDMAResource::InitResource(struct sock_t *m_sock, struct rdma_resource_t *m_res)
{
	if(!initIBDevice(m_res))
	{
		g_lastErrorCode = IBDT_E_InitIBDeviceFailed;
		ERROR("初始化IB设备失败\n");
		return false;
	}

	return true;
}

bool RDMAResource::initIBDevice(struct rdma_resource_t *m_res)
{
	int num_devices;
	struct ibv_device *ibDev = NULL;
	struct user_param_t *user_param = &(m_res->user_param);

	// 获取设备名称
	m_res->dev_list = ibv_get_device_list(&num_devices);
	if (!m_res->dev_list)
	{
		ERROR("获取IB设备列表失败.\n");
		return false;
	}

	if (!num_devices)
	{
		INFO("找到 %d 个设备.\n", num_devices);
		return false;
	}

	for (int i = 0; i < num_devices; i++)
	{
		if (!strcmp(ibv_get_device_name(m_res->dev_list[i]),user_param->hca_id))
		{
			ibDev = m_res->dev_list[i];
			break;
		}
	}

	if (!ibDev)
	{
		ERROR("IB设备 %s 未找到\n", user_param->hca_id);
		return false;
	}

	// 获取设备句柄
	m_res->ib_ctx = ibv_open_device(ibDev);
	if (!m_res->ib_ctx)
	{
		ERROR("打开设备 %s 失败\n", user_param->hca_id);
		return false;
	}

	// 查询端口属性(使用默认端口：1)
	if (ibv_query_port(m_res->ib_ctx, 1, &m_res->port_attr))
	{
		ERROR("获取IB端口 %d 属性失败\n", 1);
		return false;
	}

	if (m_res->port_attr.state != IBV_PORT_ACTIVE)
	{
	    ERROR(" 设备 %s, 端口#%d 处于非活动状态\n", user_param->hca_id, 1);
	    return false;
	}

	// 分配保护域
	m_res->pd = ibv_alloc_pd(m_res->ib_ctx);
	if (!m_res->pd)
	{
		ERROR("分配保护域失败\n");
		return false;
	}

	INFO("IB设备初始化完成\n");

	return true;
}


bool RDMAResource::DestroyResource(struct rdma_resource_t* m_res)
{
	if (m_res->pd)
		ibv_dealloc_pd(m_res->pd);

	if (m_res->ib_ctx)
		ibv_close_device(m_res->ib_ctx);

	if (m_res->dev_list)
		ibv_free_device_list(m_res->dev_list);

	return true;
}


bool RDMAResource::CreateQP(struct thread_context_t *t_ctx)
{
	struct rdma_resource_t* rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct ibv_qp_init_attr qp_init_attr;

	if (!createCQ(t_ctx))
	{
		ERROR("创建CQ失败.\n");
		return false;
	}

	memset(&qp_init_attr, 0, sizeof(qp_init_attr));
	qp_init_attr.cap.max_send_wr = user_param->num_of_oust;
	qp_init_attr.cap.max_recv_wr = user_param->num_of_oust;
	qp_init_attr.cap.max_send_sge = 1;
	qp_init_attr.cap.max_recv_sge = 0;
	qp_init_attr.send_cq = t_ctx->send_cq;
	qp_init_attr.recv_cq = t_ctx->recv_cq;
	qp_init_attr.qp_type = IBV_QPT_RC;
	qp_init_attr.sq_sig_all = 1;
	qp_init_attr.qp_context = 0; /// How to set.

	//dumpQPInitAttr(&qp_init_attr);
	t_ctx->qp = ibv_create_qp(rdma_resource->pd, &qp_init_attr);
	if (t_ctx->qp == NULL)
	{
		ERROR("ibv_create_qp失败.\n");
		return false;
	}
	return true;
}

bool RDMAResource::createCQ(struct thread_context_t *t_ctx)
{
	struct rdma_resource_t* rdma_resource = t_ctx->rdma_resource;

	t_ctx->send_comp_channel = ibv_create_comp_channel(rdma_resource->ib_ctx);
	if (t_ctx->send_comp_channel == NULL)
	{
		ERROR("创建发送完成通道失败.\n");
		return false;
	}
	INFO("创建发送完成通道成功\n");

	t_ctx->send_cq = ibv_create_cq(rdma_resource->ib_ctx, DEFAULT_CQ_SIZE, NULL, t_ctx->send_comp_channel, 0);
	if (t_ctx->send_cq == NULL)
	{
		ibv_destroy_comp_channel(t_ctx->send_comp_channel);
		ERROR("创建包含%u个entries的发送CQ失败.\n", DEFAULT_CQ_SIZE);
		return false;
	}
	INFO("创建包含%u个entries的发送CQ成功\n", DEFAULT_CQ_SIZE);

	ibv_req_notify_cq(t_ctx->send_cq, 0);

	t_ctx->recv_comp_channel = ibv_create_comp_channel(rdma_resource->ib_ctx);
	if (t_ctx->recv_comp_channel == NULL)
	{
		ERROR("创建接收完成通道失败.\n");
		return false;
	}
	INFO("创建接收完成通道成功\n");

	t_ctx->recv_cq = ibv_create_cq(rdma_resource->ib_ctx, DEFAULT_CQ_SIZE,
			NULL, t_ctx->recv_comp_channel, 0);
	if (t_ctx->recv_cq == NULL)
	{
		ibv_destroy_comp_channel(t_ctx->recv_comp_channel);
		ERROR("创建包含%u个entries的接收CQ失败.\n", DEFAULT_CQ_SIZE);
		return false;
	}
	INFO("创建包含%u个entries的接收CQ成功\n", DEFAULT_CQ_SIZE);

	ibv_req_notify_cq(t_ctx->recv_cq, 0);

	return true;
}

bool RDMAResource::ConnectQP(struct thread_context_t *t_ctx)
{
	if (!setQPAttr(t_ctx))
	{
		ERROR("set_qp_attr failed.\n");
		return false;
	}

	if (!modifyQPToInit(t_ctx))
	{
		ERROR("QP状态迁移到 INIT 失败\n");
		return false;
	}

	INFO("QP状态迁移到 INIT 成功\n");

	if (!modifyQPToRTR(t_ctx)) {
		ERROR("QP状态迁移到 RTR 失败\n");
		return false;
	}

	INFO("QP状态迁移到 RTR 成功\n");

	if (!modifyQPToRTS(t_ctx))
	{
		ERROR("QP状态迁移到 RTS 失败\n");
		return false;
	}

	INFO("QP状态迁移到 RTS 成功\n");

	return true;
}

bool RDMAResource::setQPAttr(struct thread_context_t *t_ctx)
{
	struct rdma_resource_t* rdma_resource;
	enum ibv_qp_type qp_type;
	struct ibv_qp_attr *qp_attr;
	struct ibv_ah_attr *ah_attr;

	qp_attr = &(t_ctx->qp_attr);
	ah_attr = &(qp_attr->ah_attr);
	rdma_resource = t_ctx->rdma_resource;
	qp_type = IBV_QPT_RC;

	qp_attr->pkey_index = DEF_PKEY_IX;
	qp_attr->port_num = 1;
	qp_attr->sq_psn = t_ctx->psn;

	qp_attr->qp_access_flags = IBV_ACCESS_LOCAL_WRITE
			| IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ
			| IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_MW_BIND;

	// set ibv_ah_attr
	memset(ah_attr, 0, sizeof(struct ibv_ah_attr));
	ah_attr->port_num = 1;		// 使用默认端口：1
	ah_attr->dlid = t_ctx->remote_lid;
	ah_attr->static_rate = DEF_STATIC_RATE;
	ah_attr->src_path_bits = 0;
	ah_attr->sl = DEF_SL;

	qp_attr->path_mtu = IBV_MTU_1024;
	qp_attr->rq_psn = t_ctx->remote_psn;
	qp_attr->dest_qp_num = t_ctx->remote_qpn;


	qp_attr->timeout = 0x12;
	qp_attr->retry_cnt = 6;
	qp_attr->rnr_retry = 6;//DEF_PKEY_IX;
	qp_attr->min_rnr_timer = DEF_RNR_NAK_TIMER;
	qp_attr->max_rd_atomic = 128;
	qp_attr->max_dest_rd_atomic = 16; // Todo: why it should be <= 16?

	return true;
}

bool RDMAResource::modifyQPToInit(struct thread_context_t *t_ctx)
{
	struct ibv_qp_attr attr;
	int attr_mask = 0;

	// QP状态迁移：RESET -> INIT
	memset(&attr, 0, sizeof(struct ibv_qp_attr));

	attr.qp_state = IBV_QPS_INIT;
	attr.pkey_index = t_ctx->qp_attr.pkey_index;
	attr.port_num = t_ctx->qp_attr.port_num;
	attr_mask = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT;

	attr.qp_access_flags = t_ctx->qp_attr.qp_access_flags; /// need to check
	attr_mask |= IBV_QP_ACCESS_FLAGS;

	if (ibv_modify_qp(t_ctx->qp, &attr, attr_mask))
	{
		ERROR("修改QP状态到INIT失败\n");
		return false;
	}

	return true;
}

bool RDMAResource::modifyQPToRTR(struct thread_context_t *t_ctx)
{
	struct ibv_qp_attr attr;
	int attr_mask = 0;

	// QP状态迁移：INIT -> RTR
	memset(&attr, 0, sizeof(struct ibv_qp_attr));

	attr.qp_state = IBV_QPS_RTR;
	attr_mask = IBV_QP_STATE;

	attr.path_mtu = t_ctx->qp_attr.path_mtu;
	attr.dest_qp_num = t_ctx->qp_attr.dest_qp_num;	/// 需要初始化参数
	attr.rq_psn = t_ctx->qp_attr.rq_psn;			/// 需要初始化参数
	attr.ah_attr = t_ctx->qp_attr.ah_attr;			/// 需要初始化参数
	attr_mask |= IBV_QP_RQ_PSN | IBV_QP_DEST_QPN | IBV_QP_AV | IBV_QP_PATH_MTU;


	attr.max_dest_rd_atomic = t_ctx->qp_attr.max_dest_rd_atomic;	/// 需要初始化参数
	attr.min_rnr_timer = t_ctx->qp_attr.min_rnr_timer;				/// 需要初始化参数
	attr_mask |= IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

	if (ibv_modify_qp(t_ctx->qp, &attr, attr_mask))
	{
		ERROR("修改QP状态到RTR失败\n");
		return false;
	}
	return true;
}

bool RDMAResource::modifyQPToRTS(struct thread_context_t *t_ctx)
{
	struct ibv_qp_attr attr;
	int attr_mask = 0;

	memset(&attr, 0, sizeof(struct ibv_qp_attr));

	// QP状态迁移： RTR -> RTS
	attr.qp_state = IBV_QPS_RTS;
	attr.sq_psn = t_ctx->qp_attr.sq_psn;
	attr_mask = IBV_QP_STATE | IBV_QP_SQ_PSN;

	attr.timeout = t_ctx->qp_attr.timeout;
	attr.retry_cnt = t_ctx->qp_attr.retry_cnt;
	attr.rnr_retry = t_ctx->qp_attr.rnr_retry;
	attr.max_rd_atomic = t_ctx->qp_attr.max_rd_atomic;
	attr_mask |= IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;

	//dumpQPAttr(t_ctx);

	if(ibv_modify_qp(t_ctx->qp, &attr, attr_mask))
	{
		ERROR("修改QP状态到RTS失败\n");
		return false;
	}

	return true;
}

// 未使用
bool RDMAResource::DisConnectQP(struct thread_context_t *t_ctx)
{
	if (t_ctx->ud_av_hdl != NULL)
	{
		ibv_destroy_ah(t_ctx->ud_av_hdl);
	}
	return true;
}

void RDMAResource::DestroyQP(struct thread_context_t *t_ctx)
{
	ibv_destroy_qp(t_ctx->qp);

	ibv_destroy_cq(t_ctx->send_cq);
	ibv_destroy_cq(t_ctx->recv_cq);
	ibv_destroy_comp_channel(t_ctx->send_comp_channel);
	ibv_destroy_comp_channel(t_ctx->recv_comp_channel);
}

bool RDMAResource::CreateRDMABufPool(struct thread_context_t *t_ctx)
{
	struct rdma_buf_t *rdma_buf = NULL;
	struct rdma_resource_t* rdma_resource = t_ctx->rdma_resource;

	// 注册本地访问内存; 分配额外8字节用于原子操作（atomic operations）
	t_ctx->buff = (void*) malloc(DEF_MR_SIZE + 8);
	if (t_ctx->buff == NULL)
	{
		ERROR("分配本地访问内存失败.\n");
		return false;
	}

	// 确保buffer是8字节对齐的（for atomic operation）
	t_ctx->buff_aligned = (void*) ((uint64_t) (t_ctx->buff) & (~0x7LL));
	uint32_t mr_access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ
			| IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

	t_ctx->local_mr = ibv_reg_mr(rdma_resource->pd, t_ctx->buff_aligned,
			DEF_MR_SIZE, mr_access);
	if (t_ctx->local_mr == NULL)
	{
		ERROR("Failed to register memory for local access.\n");
		free(t_ctx->buff);
		return false;
	}

	INFO("注册内存MR，地址：%p，lkey: 0x%x, rkey: 0x%x, flags: 0x%x\n",
			t_ctx->buff_aligned, t_ctx->local_mr->lkey, t_ctx->local_mr->rkey, mr_access);

	t_ctx->head = (struct rdma_buf_t*)t_ctx->buff_aligned;
	pthread_mutex_init(&(t_ctx->mr_mutex), NULL);
	pthread_mutex_init(&(t_ctx->pending_mr_mutex), NULL);

	for (int64_t i = 0; i < (DEF_MR_SIZE / DEF_BUF_SIZE); ++i)
	{
		rdma_buf = (struct rdma_buf_t*)((uint64_t)(t_ctx->buff_aligned) + i * DEF_BUF_SIZE);
		rdma_buf->buf_idx = i;
		rdma_buf->next = (struct rdma_buf_t*)((uint64_t)rdma_buf + DEF_BUF_SIZE);
		rdma_buf->status = 0x1fffffff;
		rdma_buf->cur = rdma_buf;

		//DEBUG("rdma_buf idx: %d (0x%lx), next: (0x%lx)\n", rdma_buf->buf_idx, (uint64_t)rdma_buf, (uint64_t)rdma_buf->next);
	}

	rdma_buf->next = NULL; // the last buffer;
	t_ctx->tail = rdma_buf;
	t_ctx->pending_head = NULL;
	t_ctx->pending_tail = NULL;

	INFO("总共创建 %d 个buffer，用于RDMA操作.\n", (DEF_MR_SIZE / DEF_BUF_SIZE));
	return true;
}

bool RDMAResource::FreeRDMAPool(struct thread_context_t* t_ctx)
{
	if (t_ctx == NULL)
	{
		ERROR("[FreeRDMAPool]t_ctx is null.\n");
		return false;
	}
	if (t_ctx->local_mr == NULL)
	{
		ERROR("[FreeRDMAPool]t_ctx->local_mr is null.\n");
		return false;
	}
	if (ibv_dereg_mr(t_ctx->local_mr) != 0)
	{
		printf("ibv_dereg_mr..false\n");
		ERROR("[FreeRDMAPool]ibv_dereg_mr(t_ctx->local_mr) false.\n");
		return false;
	}
	return true;
}

void RDMAResource::GetRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t*& rdma_buf)
{
	pthread_mutex_lock(&t_ctx->mr_mutex);
	if (t_ctx->head == NULL)
	{
		pthread_mutex_lock(&t_ctx->pending_mr_mutex);
		t_ctx->head = t_ctx->pending_head;
		t_ctx->tail = t_ctx->pending_tail;

		t_ctx->pending_head = NULL;
		t_ctx->pending_tail = NULL;

		DEBUG("取Buf：头为空，头尾指向pending头尾，头idx：%d，尾idx：%d, pending头尾置为空\n", t_ctx->head->buf_idx, t_ctx->tail->buf_idx);
		pthread_mutex_unlock(&t_ctx->pending_mr_mutex);
	}

	if (t_ctx->head != NULL)
	{
		rdma_buf = t_ctx->head;
		rdma_buf->status = 0x2fffffff;
		t_ctx->head = rdma_buf->next;
		DEBUG("取出 idx: %d RDMABuf，置状态已使用, 头指向Buf (idx: %d)\n", rdma_buf->buf_idx, t_ctx->head->buf_idx);
		if (t_ctx->head == NULL)
		{
			t_ctx->tail = NULL;
			DEBUG("取Buf：RDMABuf 取到了内存池尾部，头尾都为空\n");
		}
	}
	pthread_mutex_unlock(&t_ctx->mr_mutex);
}

void RDMAResource::GetRDMAPayloadBuf(struct rdma_buf_t *rdma_buf, char*& payload_buf)
{
	assert(sizeof(struct rdma_buf_t) <= RDMA_BUF_HDR_SIZE);
	payload_buf = (char*)((uint64_t)rdma_buf + RDMA_BUF_HDR_SIZE);
}

void RDMAResource::PutRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t *rdma_buf)
{
	pthread_mutex_lock(&t_ctx->pending_mr_mutex);
	if (t_ctx->pending_head == NULL)
	{
		t_ctx->pending_head = rdma_buf;
		t_ctx->pending_tail = rdma_buf;
		DEBUG("pending头为空，pending头尾指向该Buf (idx: %d)\n", rdma_buf->buf_idx);
	}
	else
	{
		rdma_buf->next = NULL;
		t_ctx->pending_tail->next = rdma_buf;
		t_ctx->pending_tail = rdma_buf;
		DEBUG("pending尾移动指向该Buf (idx: %d)\n", rdma_buf->buf_idx);
	}
	rdma_buf->cur = rdma_buf;
	rdma_buf->buf_idx = ((uint64_t)rdma_buf - (uint64_t)(t_ctx->buff_aligned)) / DEF_BUF_SIZE;
	rdma_buf->status = 0x1fffffff;
	DEBUG("放回 idx: %d RDMABuf，置状态未使用\n", rdma_buf->buf_idx);
	pthread_mutex_unlock(&t_ctx->pending_mr_mutex);
}

void RDMAResource::dumpQPInitAttr(struct ibv_qp_init_attr *qp_init_attr)
{
	DEBUG("\nDumping ibv_qp_init_attr:\n");
	DEBUG("\t    qp_init_attr.cap.max_send_wr  :%x\n", qp_init_attr->cap.max_send_wr);
	DEBUG("\t    qp_init_attr.cap.max_send_wr  :%x\n", qp_init_attr->cap.max_send_wr);
	DEBUG("\t    qp_init_attr.cap.max_send_sge :%x\n", qp_init_attr->cap.max_send_sge);
	DEBUG("\t    qp_init_attr.cap.max_recv_sge :%x\n", qp_init_attr->cap.max_recv_sge);
	DEBUG("\t    qp_init_attr.send_cq          :%p\n", qp_init_attr->send_cq);
	DEBUG("\t    qp_init_attr.recv_cq          :%p\n", qp_init_attr->recv_cq);
	DEBUG("\t    qp_init_attr.srq              :%p\n", qp_init_attr->srq);
}

void RDMAResource::dumpQPAttr(struct thread_context_t *t_ctx)
{
	struct ibv_qp_attr *qp_attr;

	qp_attr = &t_ctx->qp_attr;

	DEBUG("\nibv_qp_attr:\n");
	DEBUG("\tqp_attr.pkey_index          :0x%x\n", qp_attr->pkey_index);
	DEBUG("\tqp_attr.port_num            :0x%x\n", qp_attr->port_num);
	DEBUG("\tqp_attr.sq_psn              :0x%x\n", qp_attr->sq_psn);

	DEBUG("\tqp_attr.qp_access_flags     :0x%x\n", qp_attr->qp_access_flags);
	DEBUG("\tqp_attr.qkey                :0x%x\n", qp_attr->qkey);
	DEBUG("\tqp_attr.path_mtu            :0x%x\n", qp_attr->path_mtu);
	DEBUG("\tqp_attr.rq_psn              :0x%x\n", qp_attr->rq_psn);
	DEBUG("\tqp_attr.dest_qp_num         :0x%x\n", qp_attr->dest_qp_num);

	DEBUG("\t\tqp_attr.ah_attr.is_global      :0x%x\n", qp_attr->ah_attr.is_global);
	DEBUG("\t\tqp_attr.ah_attr.dlid           :0x%x\n", qp_attr->ah_attr.dlid);
	DEBUG("\t\tqp_attr.ah_attr.src_path_bits  :0x%x\n", qp_attr->ah_attr.src_path_bits);
	DEBUG("\t\tqp_attr.ah_attr.port_num       :0x%x\n", qp_attr->ah_attr.port_num);

	DEBUG("\tqp_attr.timeout             :0x%x\n", qp_attr->timeout);
	DEBUG("\tqp_attr.retry_cnt           :0x%x\n", qp_attr->retry_cnt);
	DEBUG("\tqp_attr.rnr_retry           :0x%x\n", qp_attr->rnr_retry);
	DEBUG("\tqp_attr.min_rnr_timer       :0x%x\n", qp_attr->min_rnr_timer);
	DEBUG("\tqp_attr.max_rd_atomic       :0x%x\n", qp_attr->max_rd_atomic);
	DEBUG("\tqp_attr.max_dest_rd_atomic  :0x%x\n", qp_attr->max_dest_rd_atomic);
}








