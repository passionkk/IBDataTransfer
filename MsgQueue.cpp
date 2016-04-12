#include "MsgQueue.h"
//#include <QDebug>

CMsgQueue::CMsgQueue( void )
{

}

CMsgQueue::~CMsgQueue()
{

}

key_t CMsgQueue::FTok( const char* strMsgFilePath /*= MSG_FILE*/, int nProId /*= 'h'*/ )
{
	return ftok(strMsgFilePath, nProId);
}

int CMsgQueue::OpenMsgQue( int& nMsqid, key_t key )
{
	if ((nMsqid = msgget(key, IPC_CREAT|0777)) == -1)
	{
		return -1;
	}
	return 0;
}

int CMsgQueue::SendMsg( int nMsqid, const msg_form& msg )
{
	return msgsnd(nMsqid, &msg, sizeof(msg.mtext), 0);
}

int CMsgQueue::RecvMsg( int nMsqid, int nMsgType, msg_form& msg )
{
	return msgrcv(nMsqid, &msg, BUFSIZ, nMsgType, 0);
}

int CMsgQueue::CloseMsgQue( int nMsqid )
{
	if (msgctl(nMsqid, IPC_RMID, 0) == -1)
	{
		//qDebug() << "close message queue " << nMsqid << "failed";
		return -1;
	}
	return 0;
}

