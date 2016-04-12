/*
* RDMAThread.cpp
*
*  Created on: 2015-5-13
*      Author: hek
*/

#include "RDMAThread.h"
#include "RDMAResource.h"
#include "RDMAOperation.h"
#include "RDMASock.h"
#include "TimeCount.h"
#include "MsgQueue.h"
#include "DirectoryUtil.h"
#include "IBDT_ErrorDefine.h"
#include <QDebug>
#include <QFileInfo>
#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <sys/statfs.h>
using namespace std;

const float fFillPercent = 0.8; // warns if the current disk is over 80% fullfilled.

sem_t g_sem_connect;//信号量
pthread_mutex_t g_lockSock; //对sock_t参数加锁
struct sock_t g_sock;

RDMAThread::RDMAThread()
{
	// TODO 自动生成的构造函数存根
}

RDMAThread::~RDMAThread()
{
	// TODO 自动生成的析构函数存根
}

bool RDMAThread::StartRDMAThreads( struct rdma_resource_t *rdma_resource, struct sock_bind_t *sock_bind)
{
	INFO("RDMAThread::StartRDMAThreads()...\n");
	//初始化信号量
	int nRes = sem_init(&g_sem_connect, 0, 0);
	if(nRes == -1)
	{
		INFO("semaphore intitialization failed\n");
	}
	struct thread_context_t *t_ctx;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	if (user_param->server_ip)
	{
		rdma_resource->client_ctx = (struct thread_context_t*) malloc(sizeof(struct thread_context_t) * user_param->num_of_thread);
	}
	else
	{
		rdma_resource->daemon_ctx = (struct thread_context_t*) malloc(sizeof(struct thread_context_t) * user_param->num_of_thread);
	}

	int rc = 0;
	for (uint32_t i = 0; i < user_param->num_of_thread; i++)
	{
		if (user_param->server_ip)
		{
			t_ctx = &rdma_resource->client_ctx[i];
		}
		else
		{
			t_ctx = &rdma_resource->daemon_ctx[i];
		}

		t_ctx->rdma_resource = rdma_resource;

		INFO("创建RDMA线程....\n");
		rc = pthread_create(&(t_ctx->thread), &attr, RDMAThread::rdma_thread, t_ctx);
		if (rc != 0)
		{
			INFO("创建RDMA线程失败.\n");
			break;
		}
		INFO("创建RDMA线程成功....\n");
	}

	int nCurSemNum = 0;
	struct thread_context_t *t_ctxDaemon = (struct thread_context_t*) malloc(sizeof(struct thread_context_t) * 1);
	while (true)
	{
		do
		{
			sem_getvalue(&g_sem_connect, &nCurSemNum);
			sleep(1);
		}while((uint)nCurSemNum >= user_param->num_of_thread);
		DEBUG("当前连接数:%d\n", nCurSemNum);
		rc = RDMASock::sock_connect_multi(sock_bind, &(t_ctxDaemon->sock));
		if (rc)
		{
			g_lastErrorCode = IBDT_E_ConnectServerFailed;
			ERROR("两端打开连接失败.\n");
			return false;
		}
		pthread_mutex_lock(&g_lockSock);
		g_sock = t_ctxDaemon->sock;
		pthread_mutex_unlock(&g_lockSock);
		//信号量加1
		sem_post(&g_sem_connect);
		if (user_param->server_ip)
		{
			break;
		}
	}

	// 等待线程完成
	uint32_t i = 0;
	int ret = 0;
	unsigned long exit_code;
	if (user_param->server_ip) // client side
	{
		if (i != user_param->num_of_thread)
		{
			// 终止执行
			for (uint32_t j = 0; j < i; j++)
			{
				t_ctx = &rdma_resource->client_ctx[i];
				pthread_cancel(t_ctx->thread);
				pthread_join(t_ctx->thread, (void**)&exit_code);
			}

			ret = 1;
		}

		for (i = 0; i < user_param->num_of_thread; i++)
		{
			t_ctx = &rdma_resource->client_ctx[i];

			rc = pthread_join(t_ctx->thread, (void **) &exit_code);
			if ((rc != 0) || (exit_code != 0))
			{
				ERROR("等待线程[%d]结束失败.\n", i);
				ret = 1;
			}
			else
			{
				INFO("线程[%d]执行完成，返回值：%lu\n", i, exit_code);
			}

			RDMASock::sock_close_multi(&(t_ctx->sock), sock_bind);
		}
		if (rdma_resource->client_ctx != NULL)
			free(rdma_resource->client_ctx);
	}
	else
	{
		if (i != user_param->num_of_thread) {
			// 终止执行
			for (uint32_t j = 0; j < i; j++) {
				t_ctx = &rdma_resource->daemon_ctx[i];
				pthread_cancel(t_ctx->thread);
				pthread_join(t_ctx->thread, (void**) &exit_code);
			}

			ret = 1;
		}

		for (i = 0; i < user_param->num_of_thread; i++) {
			t_ctx = &rdma_resource->daemon_ctx[i];
			INFO("pthread_join:%d.\n", i);
			rc = pthread_join(t_ctx->thread, (void **) &exit_code);
			if ((rc != 0) || (exit_code != 0)) {
				ERROR("等待线程[%d]结束失败.\n", i);
				ret = 1;
			} else {
				INFO("线程[%d]执行完成，返回值：%lu\n", i, exit_code);
			}

			RDMASock::sock_close_multi(&(t_ctx->sock), sock_bind);
		}

		sem_destroy(&g_sem_connect);//清理信号量
		pthread_attr_destroy(&attr);//清理线程属性
		if (rdma_resource->daemon_ctx != NULL)
			free(rdma_resource->daemon_ctx);
	}
	if (t_ctxDaemon != NULL)
	{
		free(t_ctxDaemon);
	}
	return true;
}

void *RDMAThread::rdma_thread(void *ptr)
{
	struct thread_context_t *t_ctx = (struct thread_context_t*) ptr;
	struct rdma_resource_t  *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct rdma_req_t rdma_req;

	t_ctx->thread_id = pthread_self();

	do
	{
		if (!RDMAResource::CreateRDMABufPool(t_ctx))
		{
			g_lastErrorCode = IBDT_E_RegMRFailed;
			ERROR("创建注册内存池失败.\n");
			INFO("RDMASock::close_sock_fd(t_ctx->sock)...\n");
			RDMASock::close_sock_fd(t_ctx->sock.sock_fd);
			return NULL;
		}

		INFO("thread id : %u\n", t_ctx->thread_id);

		if (!RDMAResource::CreateQP(t_ctx))
		{
			g_lastErrorCode = IBDT_E_CreateQPFailed;
			ERROR("创建QP失败.\n");
			return NULL;
		}

		sem_wait(&g_sem_connect);
		int nCurConnectNum = 0;
		sem_getvalue(&g_sem_connect, &nCurConnectNum);
		DEBUG("Current connect num : %d\n", nCurConnectNum);
		pthread_mutex_lock(&g_lockSock);
		t_ctx->sock = g_sock;
		pthread_mutex_unlock(&g_lockSock);
		// 交换数据
		{
			struct thread_sync_info_t
			{
				uint32_t qp_num;
				uint32_t direction;
				uint32_t opcode;
				uint32_t qkey;
				uint32_t psn;
				uint16_t lid;
			}ATTR_PACKED;
			struct thread_sync_info_t local_info;
			struct thread_sync_info_t remote_info;

			local_info.lid = htons(rdma_resource->port_attr.lid);
			local_info.qp_num = htonl(t_ctx->qp->qp_num);
			local_info.direction = htonl(user_param->direction);
			local_info.opcode = htonl(user_param->opcode); /// enum ibv_wr_opcode
			local_info.qkey = htonl(0);
			local_info.psn = htonl(0);

			int rc = RDMASock::sock_sync_data(&(t_ctx->sock), sizeof(local_info), &local_info,
				&remote_info);
			if (rc)
			{
				g_lastErrorCode = IBDT_E_SyncDataFailed;
				ERROR("同步数据失败.\n");
				return NULL;
			}

			t_ctx->remote_lid = ntohs(remote_info.lid);
			t_ctx->remote_qpn = ntohl(remote_info.qp_num);
			t_ctx->remote_qkey = ntohl(remote_info.qkey);
			t_ctx->remote_psn = ntohl(remote_info.psn);

			INFO("远端QP number: \t0x%x\n", t_ctx->remote_qpn);
			INFO("远端LID:\t0x%x\n", t_ctx->remote_lid);

			if (user_param->server_ip == NULL)
			{
				user_param->direction = ntohl(remote_info.direction);
				user_param->opcode = (ibv_wr_opcode)ntohl(remote_info.opcode);

				if (user_param->direction == 0 || user_param->direction == 1) {
					t_ctx->is_requestor = 0;
				} else if (user_param->direction == 2) {
					t_ctx->is_requestor = 1;
				}
			} else {
				if (user_param->direction == 0 || user_param->direction == 1) {
					t_ctx->is_requestor = 1;
				} else if (user_param->direction == 2) {
					t_ctx->is_requestor = 0;
				}
			}

		}
		if (!RDMAResource::ConnectQP(t_ctx))
		{
			g_lastErrorCode = IBDT_E_ConnectQPFailed;
			ERROR("连接QP失败.\n");
			return NULL;
		}
		uint32_t i;
		for (i = 0; i < user_param->num_of_oust; i++)
		{
			RDMAResource::GetRDMABuf(t_ctx, rdma_req.rdma_buf);

			rdma_req.num_of_oust = 1;
			rdma_req.data_size = DEF_BUF_SIZE;

			if(!RDMAOperation::PostReceive(t_ctx, &rdma_req))
			{
				g_lastErrorCode = IBDT_E_PostReceviceFailed;
				ERROR("post_receive失败, i: %d.\n", i);
				return NULL;
			}
		}
		INFO("一次性发送接收请求 %d 个成功完成\n", user_param->num_of_oust);
		if(RDMASock::sock_sync_ready(&t_ctx->sock))
		{
			g_lastErrorCode = IBDT_E_SyncReadyFailed;
			ERROR("同步就绪失败\n");
			return NULL;
		}
		INFO("同步就绪成功\n");


		// 一端传送文件名称，另一端收到后，创建文件
		file_info_t file_info;
		file_info.file_size = 0;
		file_info.file_resume_size = 0;
		file_info.file_type = user_param->is_trans_msg_file;
		file_info.src_file_path = (char*)malloc(BUFSIZ);
		file_info.target_file_path = (char*)malloc(BUFSIZ);
		if(!TransFileInfo(t_ctx, &rdma_req, file_info))
		{
			g_lastErrorCode = IBDT_E_SendFileInfoFailed;
			ERROR("传输文件信息交互失败\n");
			return NULL;
		}
		INFO("传输文件信息交互成功， 文件名： %s, 大小： %ld, 远端文件已存在大小:	%ld\n", file_info.src_file_path, file_info.file_size, file_info.file_resume_size);

		CTimeCount::SetStartTime();
		// 一端读取数据，另一端接收数据写文件
		bool bTransRet = true;
		if(!TransFile(t_ctx, &rdma_req, file_info))
		{
			g_lastErrorCode = IBDT_E_TransferFileFailed;
			if(t_ctx->is_requestor)
				ERROR("传输文件 %s 失败\n", file_info.src_file_path);
			else
				ERROR("传输文件 %s 失败\n", file_info.target_file_path);
			bTransRet = false;//需要修改，直接返回 线程就退出了
		}
		if (bTransRet)
		{
			CTimeCount::SetEndTime();
			if(t_ctx->is_requestor)
				INFO("传输文件 %s 成功 文件类型:%d，耗时: %.1f ms\n", file_info.src_file_path, file_info.file_type, CTimeCount::GetUseTime(false));
			else
				INFO("传输文件 %s 成功 文件类型:%d，耗时: %.1f ms\n", file_info.target_file_path, file_info.file_type, CTimeCount::GetUseTime(false));
			//如果是消息类型的文件并且是接收端，通知消息服务器
			if (file_info.file_type == 1 && t_ctx->is_requestor == 0)
			{
				INFO("write msg queue\n");
				key_t key = CMsgQueue::FTok();
				int nMsqid = -1;
				CMsgQueue::OpenMsgQue(nMsqid, key);
				msg_form msg;
				msg.mtype = MSG_CLIENT_TYPE;
				strcpy(msg.mtext, file_info.target_file_path);
				INFO("send msg %s", msg.mtext);
				CMsgQueue::SendMsg(nMsqid, msg);
			}
		}
		if (file_info.src_file_path != NULL)
			free(file_info.src_file_path);
		if (file_info.target_file_path != NULL)
			free(file_info.target_file_path);
		if(t_ctx->is_requestor == 0)
		{
			INFO("RDMASock::close_sock_fd(t_ctx->sock)...\n");
			RDMASock::close_sock_fd(t_ctx->sock.sock_fd);
		}

		INFO("RDMAResource::DestroyQP(t_ctx)........\n");
		RDMAResource::DestroyQP(t_ctx);

		RDMAResource::FreeRDMAPool(t_ctx);
		if (t_ctx->buff != NULL)
			free(t_ctx->buff);
		else
			printf("t_ctx->buff is null\n");

	}while (user_param->server_ip == NULL);
	INFO("RDMA测试线程成功退出.\n");
	return NULL;
}

bool RDMAThread::TransFileInfo(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req, file_info_t& file_info)
{
	if(file_info.src_file_path == NULL)
	{
		return false;
	}

	struct rdma_resource_t  *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);

	// 一端传送文件名称，另一端收到后，创建文件
	if (t_ctx->is_requestor) {
		strcpy(file_info.src_file_path, user_param->src_file_path);
		if (strlen(user_param->target_file_path) > 0)
			strcpy(file_info.target_file_path, user_param->target_file_path);
		else
			strcpy(file_info.target_file_path, user_param->src_file_path);

                // File Size (2015.10.10 Xun)
                QFileInfo cInfo(file_info.src_file_path);
                file_info.file_size = cInfo.size(); // bytes
                // End of 2015.10.10

	} else {
		memset(file_info.src_file_path, 0, BUFSIZ);
		memset(file_info.target_file_path, 0, BUFSIZ);
	}

	if (!doRDMATransFileName(t_ctx, file_info)) {
		ERROR("doRDMATransFileName失败\n");
		return false;
	}

	if(t_ctx->is_requestor)
	{
		if(doRecvFileResumeSize(t_ctx, file_info.file_resume_size))
		{
			INFO("远端文件已存在大小: %ld\n", file_info.file_resume_size);
		}
		// 交互完后，接收方补一个接收请求
		RDMAResource::GetRDMABuf(t_ctx, rdma_req->rdma_buf);

		if (rdma_req->rdma_buf == NULL) 
		{
			ERROR("获取RDMA buffer失败.\n");
			return false;
		}
		rdma_req->num_of_oust = 1;
		rdma_req->data_size = DEF_BUF_SIZE;
		if (!RDMAOperation::PostReceive(t_ctx, rdma_req))
		{
			ERROR("doRecvFileResumeSize Post Receive Faile\n");
			return false;
		}
	}
	else
	{
/*                // Check Disk Space (2015.10.10 Xun)

                qDebug() << "about to get free size.\n" ;
                struct statfs cDiskInfo;
                statfs(file_info.target_file_path, &cDiskInfo);
                unsigned long long lTotalSize = cDiskInfo.f_blocks * cDiskInfo.f_bsize; // bytes
                unsigned long long lTotalFree = cDiskInfo.f_bfree * cDiskInfo.f_bsize; // bytes
                // qDebug() << "totalSize: " << (long)lTotalSize << "\n";
                printf("totalSize: %ld\n", lTotalSize);
                //qDebug() << "totalFree: " << (long)lTotalFree << "\n";
                printf("totalFree: %ld\n", lTotalFree);

                if(file_info.file_size > (long)lTotalFree) // OverSized
                {
                    qDebug() << "No Space Left for this File.\n";
                }

                float fPercent = (float)lTotalFree/(float)lTotalSize;
                if(fPercent < (1 - fFillPercent))
                {
                    ERROR("No Enough Free Space on this Disk.\n");
                    qDebug() << "No Enough Free Space on this Disk.\n" ;
                    return false;
                }
                else
                {
                    qDebug() << "Space Percent : " << fPercent << "\n";
                }

                // End of 2015.10.10
*/
		// 交互完后，接收方补一个接收请求
		RDMAResource::GetRDMABuf(t_ctx, rdma_req->rdma_buf);

		if (rdma_req->rdma_buf == NULL) {
			ERROR("获取RDMA buffer失败.\n");
			return false;
		}
		rdma_req->num_of_oust = 1;
		rdma_req->data_size = DEF_BUF_SIZE;
		if (!RDMAOperation::PostReceive(t_ctx, rdma_req))
		{
			ERROR("Receive FileName TransFileInfo Post Receive Faile\n");
			return false;
		}
		if(doSendFileResumeSize(t_ctx, file_info.file_resume_size))
		{
			INFO("通知远端文件已存在大小: %ld\n", file_info.file_resume_size);
		}
	}

	return true;
}

bool RDMAThread::doRecvFileResumeSize(struct thread_context_t *t_ctx, long& file_resume_size)
{
	struct rdma_buf_t *rdma_recv_buf;

	TRACE("\n=============== doRecvFileResumeSize ======begin=================\n");
	char* rdma_recv_payload;
	if (!doRDMAReceive(t_ctx, &rdma_recv_buf)) {
		ERROR("doRDMAReceive失败.\n");
		return false;
	}

	RDMAResource::GetRDMAPayloadBuf(rdma_recv_buf, rdma_recv_payload);

	TRACE("doRDMAReceive成功，Data: %s (长度: %d)\n", rdma_recv_payload, (int) strlen(rdma_recv_payload));

	sscanf(rdma_recv_payload, "resume_size %ld", &file_resume_size);

	RDMAResource::PutRDMABuf(t_ctx, rdma_recv_buf);

	TRACE("=============== doRecvFileResumeSize ======end=================\n");
	return true;
}

bool RDMAThread::doSendFileResumeSize(struct thread_context_t *t_ctx, long file_resume_size)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct rdma_req_t rdma_req;

	TRACE("\n=============== doSendFileResumeSize ======begin=================\n");
	char* rdma_send_payload;
	RDMAResource::GetRDMABuf(t_ctx, rdma_req.rdma_buf);
	if (rdma_req.rdma_buf == NULL) {
		ERROR("获取RDMA buffer失败.\n");
		return false;
	}
	TRACE("获取RDMA buffer成功.\n");

	rdma_req.num_of_oust = 1;
	RDMAResource::GetRDMAPayloadBuf(rdma_req.rdma_buf, rdma_send_payload);

	sprintf(rdma_send_payload, "resume_size %ld", file_resume_size);
	rdma_req.data_size = RDMA_BUF_HDR_SIZE + strlen(rdma_send_payload);
	rdma_req.opcode = user_param->opcode;

	if (!doRDMASend(t_ctx, &rdma_req)) {
		ERROR("doRDMASend失败.\n");
		return false;
	}

	TRACE("doRDMASend成功, 数据长度: %d\n", (int)strlen(rdma_send_payload));
	TRACE("=============== doSendFileResumeSize ======end=================\n");

	RDMAResource::PutRDMABuf(t_ctx, rdma_req.rdma_buf);
	return true;
}

bool RDMAThread::doRDMATransFileName(struct thread_context_t *t_ctx, file_info_t& file_info)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct rdma_req_t rdma_req;
	struct rdma_buf_t *rdma_recv_buf;

	TRACE("\n=============== RDMATransFileName ======begin=================\n");
	char* rdma_send_payload;	// 发送文件全路径
	char* rdma_recv_payload;	// 收到文件全路径后，解析出文件名，创建文件


	if (t_ctx->is_requestor) {
		RDMAResource::GetRDMABuf(t_ctx, rdma_req.rdma_buf);
		if (rdma_req.rdma_buf == NULL) {
			ERROR("获取RDMA buffer失败.\n");
			return false;
		}
		TRACE("获取RDMA buffer成功.\n");
		rdma_req.num_of_oust = 1;

		RDMAResource::GetRDMAPayloadBuf(rdma_req.rdma_buf, rdma_send_payload);

		ifstream in(file_info.src_file_path, ios::in|ios::binary|ios::ate);
		file_info.file_size = in.tellg();
		in.close();
		sprintf(rdma_send_payload, "%s %ld %d", file_info.target_file_path, file_info.file_size, file_info.file_type);//发送目标文件路径
		rdma_req.data_size = RDMA_BUF_HDR_SIZE + strlen(rdma_send_payload);
		rdma_req.opcode = user_param->opcode;

		if (!doRDMASend(t_ctx, &rdma_req)) {
			ERROR("doRDMASend失败.\n");
			return false;
		}
		TRACE("doRDMASend成功, Data: %s (长度: %d)\n", rdma_send_payload, (int)strlen(rdma_send_payload));

		RDMAResource::PutRDMABuf(t_ctx, rdma_req.rdma_buf);

	} else {
		if (!doRDMAReceive(t_ctx, &rdma_recv_buf)) {
			ERROR("doRDMAReceive失败.\n");
			return false;
		}

		RDMAResource::GetRDMAPayloadBuf(rdma_recv_buf, rdma_recv_payload);
		TRACE("doRDMAReceive成功，Data: %s (长度: %d)\n", rdma_recv_payload, (int) strlen(rdma_recv_payload));

		sscanf(rdma_recv_payload, "%s %ld %d", file_info.target_file_path, &file_info.file_size, &file_info.file_type);

		//确保文件夹存在
		std::string strParDir = DirectoryUtil::GetParentDir(file_info.target_file_path);
		if (!DirectoryUtil::IsDirExist(strParDir.c_str()))
			DirectoryUtil::CreateDir(strParDir.c_str());
		// 获取文件大小
		ifstream in;
		in.open(file_info.target_file_path, ios::in | ios::binary | ios::ate);
		file_info.file_resume_size = in.tellg();

		RDMAResource::PutRDMABuf(t_ctx, rdma_recv_buf);
	}

	TRACE("=============== RDMATransFileName ======end=================\n");
	return true;
}

bool RDMAThread::TransFile(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req, file_info_t& file_info)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);

	// 请求端打开读取文件数据，接收端收到文件数据写文件
	if (t_ctx->is_requestor) {

		long readSize = 0;

		// 打开文件读数据
		ifstream in;
		in.open(file_info.src_file_path, ios::in /*| ios::binary*/ | ios::ate);
		long size = in.tellg();

		if(user_param->is_resume_trans)
		{
			if(file_info.file_resume_size == 0 || file_info.file_resume_size > size)
				in.seekg(0, ios::beg);
			else if(file_info.file_resume_size > 0 && file_info.file_resume_size <= size)
			{
				in.seekg(file_info.file_resume_size, ios::beg);
				readSize = file_info.file_resume_size;
			}
		}
		else
		{
			in.seekg(0, ios::beg);
		}

		TRACE("文件 %s 大小 %ld\n", file_info.src_file_path, size);

		int sendSize = DEF_BUF_SIZE - RDMA_BUF_HDR_SIZE;
		char sendBuf[sendSize];
		int nSendTotalCount = 1023;
		int nSendCount = 0;
		long lSendTestTime = 0;
		while (!in.eof() && readSize < size) {
			memset(sendBuf, 0, sendSize);
			in.read(sendBuf, sendSize);
			if(!doSendFileData(t_ctx, sendBuf, sendSize))
			{
				ERROR("发送文件 %s 数据失败\n", file_info.src_file_path);
				return false;
			}
			
			readSize += in.gcount();
			
			DEBUG("完成读文件大小： %ld, 进度: %.1f%%\n", readSize, float(readSize*100.0/size));

			nSendCount++;

			// 文件已经读取发送完成，直接退出循环
			if(readSize >= size)
			{
				INFO("文件读取发送完成，发送次数： %d, 累计读取大小： %ld, 进度： %.1f%%\n",
					nSendCount, readSize, float(readSize*100.0/size));
				break;
			}

			if(nSendCount > nSendTotalCount)
			{
				INFO("发送端发完 %d 次数据，累计读取文件大小：%ld, 进度: %.3f%%，等待接收端继续指令\n",
					nSendTotalCount, readSize, float(readSize*100.0/size));
				INFO("[doRecvContTransFileData]lSendTestTime:%ld, nSendCount=%d\n", ++lSendTestTime, nSendCount);
				int nCount = 0;
				if(doRecvContTransFileData(t_ctx, nCount))
				{
					INFO("接收到继续传输文件数据请求，发送次数：%d，再次开始发送\n", nCount);
					nSendTotalCount = nCount;
					nSendCount = 0;
				}
				// 交互完后，接收方补一个接收请求
				RDMAResource::GetRDMABuf(t_ctx, rdma_req->rdma_buf);

				if (rdma_req->rdma_buf == NULL) {
					ERROR("获取RDMA buffer失败.\n");
					//return NULL;
				}
				rdma_req->num_of_oust = 1;
				rdma_req->data_size = DEF_BUF_SIZE;
				if (!RDMAOperation::PostReceive(t_ctx, rdma_req))
				{
					ERROR("doRecvContTransFileData__PostReceive Fail\n");
					break;
				}
			}
		}
		in.close();
		TRACE("文件 %s 发送完成\n", file_info.src_file_path);
	}
	else
	{
		long long writeSize = 0;

//                qDebug() << "about to get free size.\n" ;
//                struct statfs cDiskInfo;
//                statfs(file_info.target_file_path, &cDiskInfo);
//                unsigned long long lTotalSize = cDiskInfo.f_blocks;
//                unsigned long long lTotalFree = cDiskInfo.f_bfree;
//               // qDebug() << "totalSize: " << (long)lTotalSize << "\n";
//                printf("totalSize: %ld\n", lTotalSize);
//                //qDebug() << "totalFree: " << (long)lTotalFree << "\n";
//                printf("totalFree: %ld\n", lTotalFree);

//                float fPercent = (float)lTotalFree/(float)lTotalSize;
//                if(fPercent < (1 - fFillPercent))
//                {
//                    ERROR("No Enough Free Space on this Disk.\n");
//                    qDebug() << "No Enough Free Space on this Disk.\n" ;
//                    return false;
//                }
//                else
//                {
//                    qDebug() << "Space Percent : " << fPercent << "\n";
//                }

		// 打开文件写数据
		ofstream out;
		if(user_param->is_resume_trans)
		{
			if(file_info.file_resume_size > 0 && file_info.file_resume_size <= file_info.file_size)
			{
				out.open(file_info.target_file_path, ios::out /*| ios::binary*/ | ios::app);
				writeSize = file_info.file_resume_size;
			}
			else
				out.open(file_info.target_file_path, ios::out /*| ios::binary*/ | ios::trunc);
		}
		else
		{
			out.open(file_info.target_file_path, ios::out /*| ios::binary*/ | ios::trunc);
		}

		char recvBuf[DEF_BUF_SIZE - RDMA_BUF_HDR_SIZE];
		int recvSize = DEF_BUF_SIZE - RDMA_BUF_HDR_SIZE;
		int nRecvTotalCount = 1023;
		int nRecvCount = 0;
		long lRecvTestTime = 0;
		while (writeSize < file_info.file_size) {
			if(writeSize + recvSize > file_info.file_size)
				recvSize = file_info.file_size - writeSize;
			else
				recvSize = DEF_BUF_SIZE - RDMA_BUF_HDR_SIZE;
			memset(recvBuf, 0, DEF_BUF_SIZE - RDMA_BUF_HDR_SIZE);
			if(!doRecvFileData(t_ctx, recvBuf, recvSize))
			{
				ERROR("文件 %s 接收失败, 已接收大小: %lld, 文件大小: %ld\n", file_info.target_file_path, writeSize, file_info.file_size);
				out.close();
				return false;
			}

			// 接收方接收完数据后，立马补一个接收请求
			RDMAResource::GetRDMABuf(t_ctx, rdma_req->rdma_buf);

			if (rdma_req->rdma_buf == NULL) {
				ERROR("获取RDMA buffer失败.\n");
				out.close();
				return false;
			}
			rdma_req->num_of_oust = 1;
			rdma_req->data_size = DEF_BUF_SIZE;
			if (!RDMAOperation::PostReceive(t_ctx, rdma_req))
			{
				ERROR("POST Receive Fail\n");
				break;
			}

			out.write(recvBuf, recvSize);

			writeSize += recvSize;

			DEBUG("完成写文件大小： %lld, 进度: %.3f%%\n", writeSize, float(writeSize*100.0/file_info.file_size));

			nRecvCount++;

			// 文件已写完，直接退出循环
			if(writeSize >= file_info.file_size)
			{
				INFO("文件接收写入完成，数据写文件次数： %d, 文件大小： %lld, 进度: %.1f%%\n",
					 nRecvCount, writeSize, float(writeSize*100.0/file_info.file_size));
				break;
			}

			if(nRecvCount > nRecvTotalCount)
			{
				INFO("接收数据写文件次数：%d，累计写文件大小： %lld, 进度: %.3f%%, 通知发送端继续再发送 %d 次\n",
					 nRecvTotalCount, writeSize, float(writeSize*100.0/file_info.file_size), nRecvTotalCount);
				INFO("[doSendContTransFileData]lRecvTestTime:%ld, nRecvCount=%d\n", ++lRecvTestTime, nRecvCount );

				if(doSendContTransFileData(t_ctx, nRecvTotalCount))
				{
					nRecvCount = 0;
				}
				else
				{
					ERROR("doSendContTransFileData fail\n");
				}
			}
		}
		out.close();
		INFO("文件 %s 接收完成\n", file_info.target_file_path);
		TRACE("文件 %s 类型:%d\n", file_info.target_file_path, file_info.file_type);
		if (file_info.file_type == 1)
		{
			TRACE("文件 %s 是消息文件", file_info.target_file_path);
		}

	}

	return true;
}

bool RDMAThread::doSendFileData(struct thread_context_t *t_ctx, char *send_buf, int send_size)
{
	if(!t_ctx->is_requestor)
		return false;

	if(send_buf == NULL || send_size == 0)
		return false;

	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct rdma_req_t rdma_req;

	TRACE("\n=============== doSendFileData ======begin=================\n");
	char* rdma_send_payload;
	RDMAResource::GetRDMABuf(t_ctx, rdma_req.rdma_buf);
	if (rdma_req.rdma_buf == NULL) {
		ERROR("获取RDMA buffer失败.\n");
		return false;
	}
	TRACE("获取RDMA buffer成功.\n");

	rdma_req.num_of_oust = 1;
	RDMAResource::GetRDMAPayloadBuf(rdma_req.rdma_buf, rdma_send_payload);

	memcpy(rdma_send_payload, send_buf, send_size);
	rdma_req.data_size = RDMA_BUF_HDR_SIZE + send_size;
	rdma_req.opcode = user_param->opcode;

	if (!doRDMASend(t_ctx, &rdma_req)) {
		ERROR("doRDMASend失败.\n");
		return false;
	}

	TRACE("doRDMASend成功, 数据长度: %d\n", send_size);
	TRACE("=============== doSendFileData ======end=================\n");

	RDMAResource::PutRDMABuf(t_ctx, rdma_req.rdma_buf);
	return true;
}

bool RDMAThread::doRecvFileData(struct thread_context_t *t_ctx, char *recv_buf, int recv_size)
{
	struct rdma_buf_t *rdma_recv_buf;

	TRACE("\n=============== doRecvFileData ======begin=================\n");
	char* rdma_recv_payload;
	if (!doRDMAReceive(t_ctx, &rdma_recv_buf)) {
		ERROR("doRDMAReceive失败.\n");
		return false;
	}

	RDMAResource::GetRDMAPayloadBuf(rdma_recv_buf, rdma_recv_payload);

	memcpy(recv_buf, rdma_recv_payload, recv_size);
	TRACE("doRDMAReceive成功, 数据长度: %d\n", recv_size);

	RDMAResource::PutRDMABuf(t_ctx, rdma_recv_buf);

	TRACE("=============== doRecvFileData ======end=================\n");
	return true;
}


bool RDMAThread::doRecvContTransFileData(struct thread_context_t *t_ctx, int& totalCount)
{
	struct rdma_buf_t *rdma_recv_buf;

	TRACE("\n=============== doRecvContTransFileData ======begin=================\n");
	char* rdma_recv_payload;
	if (!doRDMAReceive(t_ctx, &rdma_recv_buf)) {
		ERROR("doRDMAReceive失败.\n");
		return false;
	}

	RDMAResource::GetRDMAPayloadBuf(rdma_recv_buf, rdma_recv_payload);

	TRACE("doRDMAReceive成功，Data: %s (长度: %d)\n", rdma_recv_payload, (int) strlen(rdma_recv_payload));

	sscanf(rdma_recv_payload, "continue %d", &totalCount);

	RDMAResource::PutRDMABuf(t_ctx, rdma_recv_buf);

	TRACE("=============== doRecvContTransFileData ======end=================\n");
	return true;
}

// 接收端流控：通知发送端可以再发送多少次数据
bool RDMAThread::doSendContTransFileData(struct thread_context_t *t_ctx, int totalCount)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	struct rdma_req_t rdma_req;

	TRACE("\n=============== doSendContTransFileData ======begin=================\n");
	char* rdma_send_payload = NULL;
	RDMAResource::GetRDMABuf(t_ctx, rdma_req.rdma_buf);
	if (rdma_req.rdma_buf == NULL) {
		ERROR("获取RDMA buffer失败.\n");
		return false;
	}
	TRACE("获取RDMA buffer成功.\n");

	rdma_req.num_of_oust = 1;
	RDMAResource::GetRDMAPayloadBuf(rdma_req.rdma_buf, rdma_send_payload);
	if (rdma_send_payload != NULL)
	{
		sprintf(rdma_send_payload, "continue %d", totalCount);
		INFO("[doSendContTransFileData]rdma_send_paylod:%s\n", rdma_send_payload);
	}
	rdma_req.data_size = RDMA_BUF_HDR_SIZE + strlen(rdma_send_payload);
	rdma_req.opcode = user_param->opcode;

	if (!doRDMASend(t_ctx, &rdma_req)) {
		ERROR("doRDMASend失败.\n");
		return false;
	}

	TRACE("doRDMASend成功, 数据长度: %d\n", (int)strlen(rdma_send_payload));
	TRACE("=============== doSendContTransFileData ======end=================\n");

	RDMAResource::PutRDMABuf(t_ctx, rdma_req.rdma_buf);
	return true;
}

bool RDMAThread::doRDMASend(struct thread_context_t *t_ctx, struct rdma_req_t *rdma_req)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct rdma_buf_t *rdma_buf = rdma_req->rdma_buf;
	struct ibv_wc wc;

	rdma_buf->slid = rdma_resource->port_attr.lid;
	rdma_buf->dlid = t_ctx->remote_lid;
	rdma_buf->sqpn = t_ctx->qp->qp_num;
	rdma_buf->dqpn = t_ctx->remote_qpn;

	if(!RDMAOperation::PostSend(t_ctx, rdma_req))
	{
		ERROR("PostSend执行失败.\n");
		return false;
	}
	TRACE("PostSend执行成功.\n");

	if(!getThreadWC(t_ctx, &wc, 1))
	{
		ERROR("获取wc失败.\n");
		return false;
	}
	TRACE("获取wc成功.\n");

	if (wc.status != IBV_WC_SUCCESS)
	{
		ERROR("拿到坏的完成，状态(wc.status): 0x%x, vendor syndrome: 0x%x, wc.qpnum:%d, wc.qpsrc:%d\n", wc.status, wc.vendor_err, wc.qp_num, wc.src_qp);
		return false;
	}

	return true;
}

bool RDMAThread::doRDMAReceive(struct thread_context_t *t_ctx, struct rdma_buf_t **rdma_buf)
{
	struct ibv_wc wc;

	if(!getThreadWC(t_ctx, &wc, 0))
	{
		ERROR("获取wc失败.\n");
		return false;
	}
	TRACE("获取wc成功.\n");

	if (wc.status != IBV_WC_SUCCESS)
	{
		ERROR("拿到坏的完成，状态: 0x%x, vendor syndrome: 0x%x. IBV_WC_RNR_RETRY_EXC_ERR:0x%x\n", wc.status, wc.vendor_err, IBV_WC_RNR_RETRY_EXC_ERR);
		return false;
	}

	*rdma_buf = (struct rdma_buf_t*) wc.wr_id;
	if(!verifyRDMABuf(t_ctx, *rdma_buf))
	{
		ERROR("验证rdma_buf头失败. buf_idx: %d\n", (*rdma_buf)->buf_idx);
		return false;
	}
	TRACE("验证rdma_buf头成功. buf_idx: %d\n", (*rdma_buf)->buf_idx);

	return true;
}

bool RDMAThread::getThreadWC(struct thread_context_t *t_ctx, struct ibv_wc *wc, int is_send)
{
	struct ibv_cq *cq;
	struct ibv_comp_channel *comp_channel;
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;
	struct user_param_t *user_param = &(rdma_resource->user_param);
	void *ectx;
	int rc = 0;

	if (is_send)
	{
		cq = t_ctx->send_cq;
		comp_channel = t_ctx->send_comp_channel;
	}
	else
	{
		cq = t_ctx->recv_cq;
		comp_channel = t_ctx->recv_comp_channel;
	}

	if (user_param->use_event)
	{
		rc = ibv_get_cq_event(comp_channel, &cq, &ectx);
		if (rc != 0)
		{
			ERROR("ibv_get_cq_event执行失败.\n");
			return false;
		}

		ibv_ack_cq_events(cq, 1);

		rc = ibv_req_notify_cq(cq, 0);
		if (rc != 0)
		{
			ERROR("ibv_get_cq_event执行失败.\n");
			return false;
		}
	}

	do {
		rc = ibv_poll_cq(cq, 1, wc);
		if (rc < 0)
		{
			ERROR("轮循CQ失败.\n");
			return false;
		}
	} while (!user_param->use_event && (rc == 0)); /// need timeout

	return true;
}

bool RDMAThread::verifyRDMABuf(struct thread_context_t *t_ctx, struct rdma_buf_t *rdma_buf)
{
	struct rdma_resource_t *rdma_resource = t_ctx->rdma_resource;

	if ((rdma_buf->dlid != rdma_resource->port_attr.lid) || (rdma_buf->dqpn != t_ctx->qp->qp_num))
	{
		ERROR("验证收到的rdma_buf失败, 收到 dlid=%d, dqpn=%d, 期望 dlid=%d,dqpn=%d.\n",
			rdma_buf->dlid, rdma_buf->dqpn, rdma_resource->port_attr.lid, t_ctx->qp->qp_num);
		return false;
	}

	TRACE("验证收到的rdma_buf成功, 收到 dlid=%d, dqpn=%d, 期望 dlid=%d,dqpn=%d.\n",
		rdma_buf->dlid, rdma_buf->dqpn, rdma_resource->port_attr.lid, t_ctx->qp->qp_num);

	return true;
}












