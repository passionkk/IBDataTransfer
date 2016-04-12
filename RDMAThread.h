/*
 * RDMAThread.h
 *
 *  Created on: 2015-5-13
 *      Author: hek
 */

#ifndef RDMATHREAD_H_
#define RDMATHREAD_H_

#include "rdma_mt.h"

class RDMAThread
{
public:
	RDMAThread();
	virtual ~RDMAThread();

public:
	// RDMA线程管理，创建不同的线程上下文，处理不同的RDMA
	static bool StartRDMAThreads( struct rdma_resource_t* rdma_resource, struct sock_bind_t* sock_bind);

private:
	static void* rdma_thread(void *ptr);

	// 传输文件信息，包含整个交互过程
	static bool TransFileInfo(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req, file_info_t& file_info);
	/*
	** 交互过程：客户端发送要读取的文件名、文件大小，然后等待远端已存在文件大小，守护端收到后解析出文件名、文件大小，读取远端已存在文件大小发送给客户端
	**
	** 客户端								守护端
	** 发送数据 "文件名 文件大小"	----->	接收数据解析出文件名、文件大小
	** 接收数据解析出远端已存在文件大小  <----- 发送数据 "远端已存在文件大小"
	*/
	static bool doRDMATransFileName(struct thread_context_t *t_ctx, file_info_t& file_info);

	// 发送远端文件已存在大小，仅接收端执行
	static bool doSendFileResumeSize(struct thread_context_t *t_ctx, long file_resume_size);
	// 接收远端文件已存在大小，仅请求端执行
	static bool doRecvFileResumeSize(struct thread_context_t *t_ctx, long& file_resume_size);

	// 传输文件，包含整个交互过程
	static bool TransFile(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req, file_info_t& file_info);
	// 发送文件数据，仅请求端执行
	static bool doSendFileData(struct thread_context_t *t_ctx, char* send_buf, int send_size);
	// 接收文件数据，仅接收端执行
	static bool doRecvFileData(struct thread_context_t *t_ctx, char* recv_buf, int recv_size);
	
	// 传输流控：接收端每完成128个读取，通知发送端增加128次发送，发送端发送完就等待
	static bool doRecvContTransFileData(struct thread_context_t *t_ctx, int& totalCount);
	static bool doSendContTransFileData(struct thread_context_t *t_ctx, int totalCount);

	static bool doRDMASend(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req);
	static bool doRDMAReceive(struct thread_context_t *t_ctx, struct rdma_buf_t **rdma_buf);

	static bool getThreadWC(struct thread_context_t *t_ctx, struct ibv_wc *wc, int is_send);
	static bool verifyRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t *rdma_buf);
};

#endif /* RDMATHREAD_H_ */
