/*
 * Copyright (c) 2005 Mellanox Technologies. All rights reserved.
 */

#ifndef SHRSRC_TEST_H
#define SHRSRC_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <arpa/inet.h>
#include <poll.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h> 
#include <unistd.h> 
#include <assert.h>
#include <infiniband/verbs.h>
#include "rdma_utils.h"

#define RDMA_DEBUG
#define UNUSED __attribute__ ((unused))
#define ATTR_PACKED __attribute__ ((packed))

#define MAX_QP_NUM				512
#define DEFAULT_CQ_SIZE			1024			// 完成队列entry个数

#define DEF_MR_SIZE				(1 << 24)		// 内存注册大小：512M --- 目前是16M
#define DEF_SG_SIZE				(1 << 14)		// SG大小 16K
#define DEF_BUF_SIZE			(1 << 13)		// RDMA buf大小 8K

// RDMA buf头，占用了128字节信息，128字节之后为payload数据
#define RDMA_BUF_UD_ADDITION    40
#define RDMA_BUF_LOCAL_INFO_SZ  24
#define RDMA_BUF_HDR_SIZE		128

#define DEF_PKEY_IX				0
#define DEF_RNR_NAK_TIMER		0x12
#define DEF_SL					0
#define DEF_STATIC_RATE			0


enum post_enum_t
{
	POST_MODE_ONE_BY_ONE,
	POST_MODE_LIST
};

struct thread_context_t
{
	struct rdma_resource_t    *rdma_resource;
	pthread_t          thread;
	struct sock_t      sock;
	uint32_t           thread_id;
	uint64_t           imm_qp_ctr_snd;
	uint32_t           is_requestor;
	uint32_t           num_of_iter;

	struct ibv_cq           *send_cq;
	struct ibv_cq           *recv_cq;
	struct ibv_comp_channel *send_comp_channel;
	struct ibv_comp_channel *recv_comp_channel;
	struct ibv_qp		    *qp;

	struct ibv_qp_attr  qp_attr;
	struct ibv_ah       *ud_av_hdl;

	uint32_t qkey;
	uint32_t psn;
	uint32_t remote_qpn;
	uint32_t remote_qkey;
	uint32_t remote_psn;
	uint16_t remote_lid;

	// 内存池
	void* buff;
	void* buff_aligned;
	struct ibv_mr *local_mr;
	pthread_mutex_t mr_mutex;
	pthread_mutex_t pending_mr_mutex;
	struct rdma_buf_t* head;
	struct rdma_buf_t* tail;
	struct rdma_buf_t* pending_head;
	struct rdma_buf_t* pending_tail;
};


// rdma请求结构
struct rdma_req_t
{
	struct rdma_buf_t *rdma_buf;
	struct thread_context_t *t_ctx;
	uint32_t num_of_oust;
	uint32_t post_mode;

	uint32_t thread_id;
	uint32_t is_requestor;
	uint32_t data_size;
	enum ibv_wr_opcode	opcode;

	union {
		struct ibv_mr	   *local_mr;
		struct ibv_mr	   *remote_mr;
		struct ibv_mr      peer_mr;
	};

	/// Reserved for RDMA RD/WR
	struct ibv_mr	   *remote_mr_rw;
	void			   *peer_buf;
};


// 用户参数数据结构
struct user_param_t
{
	char*	hca_id;
	char*	server_ip;
	char*	src_file_path;
	char*	target_file_path;

	const char         *ip;
	unsigned short int tcp_port;
	enum ibv_wr_opcode opcode;

	uint32_t num_of_thread;
	uint32_t num_of_cq;
	uint32_t num_of_srq;
	uint32_t num_of_qp;
	uint32_t num_of_qp_min;
	uint32_t num_of_oust;
	uint32_t buffer_size;

	uint32_t max_send_wr;
	uint32_t max_recv_wr;
	uint32_t sq_max_inline;

	uint32_t max_recv_size;
	uint32_t max_send_size;

	uint8_t direction; 
	uint32_t use_event;
	uint32_t debug_level;
	uint32_t is_resume_trans;
	uint32_t is_trans_msg_file;
};


// rdma buffer数据结构
struct rdma_buf_t
{
	int32_t buf_idx;
	uint32_t thread_idx; ///
	struct rdma_buf_t* cur;
	struct rdma_buf_t* next;
	char ud_addition[RDMA_BUF_UD_ADDITION];
	uint32_t status;
	uint32_t slid;
	uint32_t dlid;
	uint32_t sqpn;
	uint32_t dqpn;
	uint64_t sqn;
	char pad[32];
};


// rdma 资源数据结构
struct rdma_resource_t
{
	struct user_param_t user_param;

	uint16_t             local_lid;
	struct ibv_device    **dev_list;
	struct ibv_context   *ib_ctx;
	struct ibv_port_attr port_attr;
	struct ibv_pd        *pd;
	struct qp_context_t  *qp_ctx_arr;
	struct cq_context_t  *cq_ctx_arr;
	struct ibv_srq       **srq_hdl_arr;

	struct thread_context_t *client_ctx;
	struct thread_context_t *daemon_ctx;
};

// 文件信息
struct file_info_t
{
    char* src_file_path;         //文件源路径
    char* target_file_path;      //文件目标路径
    long file_size;                 //文件大小
    long file_resume_size;          //文件续传大小
    int file_type;                  //文件类型 0 普通文件 1 消息文件
};

extern uint32_t volatile stop_all;
extern uint32_t DebugLevel;
extern int g_lastErrorCode;

#define DEBUG(...) do { if (DebugLevel > 3) { printf(__VA_ARGS__); }} while (0)
#define TRACE(...) do { if (DebugLevel > 2) { printf(__VA_ARGS__); }} while (0)
#define INFO(...)  do { if (DebugLevel > 1) { printf(__VA_ARGS__); }} while (0)
#define ERROR(...) do { { printf(__VA_ARGS__); }} while (0)


#endif /* SHRSRC_TEST_H */



