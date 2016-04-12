//*********************************************************
// 文件名：TimeCount.cpp
// 作  者：hek_win7
// 日  期：2011-08-10
// 目  的：精确计时类 ——实现文件
//*********************************************************

#include "TimeCount.h"

#ifdef Q_OS_WIN
LARGE_INTEGER CTimeCount::m_startTime = {0};
LARGE_INTEGER CTimeCount::m_frequency = {0};
LARGE_INTEGER CTimeCount::m_endTime = {0};

double CTimeCount::m_fUseTime = 0.0f;
double CTimeCount::m_fTotalTime = 0.0f;

#else
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h>

unsigned long CTimeCount::m_start_time_msec = 0;
unsigned long CTimeCount::m_end_time_msec = 0;
unsigned long CTimeCount::m_frequency_msec = 0;
	
unsigned long CTimeCount::m_use_time_msec = 0;
unsigned long CTimeCount::m_total_time_msec = 0;
#endif

CTimeCount::CTimeCount(void)
{
}

CTimeCount::~CTimeCount(void)
{
}

// 设置开始时间
void CTimeCount::SetStartTime()
{
#ifdef Q_OS_WIN
	QueryPerformanceFrequency(&m_frequency);
	// 记录启动时间
	QueryPerformanceCounter(&m_startTime);

	CString strStartTime;
	SYSTEMTIME sys;
	GetLocalTime( &sys );
	strStartTime.Format(_T("CTimeCount::SetStartTime() -- 开始操作时间：%02d:%02d:%02d.%03d"), sys.wHour,sys.wMinute,sys.wSecond,sys.wMilliseconds);
	OutputDebugString(strStartTime);
#else
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	m_start_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
#endif

}

// 设置结束时间
void CTimeCount::SetEndTime()
{
#ifdef Q_OS_WIN
	// 记录结束时间
	QueryPerformanceCounter(&m_endTime);

	CString strEndTime;
	SYSTEMTIME sys;
	GetLocalTime( &sys );
	strEndTime.Format(_T("CTimeCount::SetEndTime() -- 结束操作时间： %02d:%02d:%02d.%03d"), sys.wHour,sys.wMinute,sys.wSecond,sys.wMilliseconds);
	OutputDebugString(strEndTime);
#else
	struct timeval cur_time;
	gettimeofday(&cur_time, NULL);
	m_end_time_msec = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
#endif
}

// 获取用时
// bool bAccumulate -- [in] 是否累加时间
double CTimeCount::GetUseTime(bool bAccumulate/* = false*/)
{
#ifdef Q_OS_WIN
	// 计算用时 
	double dInterval = (double)(m_endTime.QuadPart - m_startTime.QuadPart);  
	m_fUseTime = dInterval / (double)m_frequency.QuadPart * 1000.0;

	CString strUseTime;
	strUseTime.Format(_T("CTimeCount::GetUseTime() -- 本次操作用时： %.3f ms"), m_fUseTime);
	OutputDebugString(strUseTime);

	if(bAccumulate)
	{
		m_fTotalTime += m_fUseTime;
	}
	
	return m_fUseTime;
#else
	m_frequency_msec = m_end_time_msec - m_start_time_msec;
	m_use_time_msec = m_frequency_msec;
	printf("本次操作用时: %ld ms", m_use_time_msec);
	
	if(bAccumulate)
	{
		m_total_time_msec += m_use_time_msec;
	}
	
#endif

	return (double)m_use_time_msec;
}

// 获取总时间
double CTimeCount::GetTotalTime()
{
#ifdef Q_OS_WIN
	CString strTotalTime;
	strTotalTime.Format(_T("CTimeCount::GetTotalTime() -- 总计用时： %.3f ms"), m_fTotalTime);
	OutputDebugString(strTotalTime);
	return m_fTotalTime;
#else
	printf("总计用时：%ld\n ms", m_total_time_msec);
	return (double)m_total_time_msec;
#endif
	
}

// 所有时间重置
void CTimeCount::ResetTime()
{
#ifdef Q_OS_WIN
	m_fUseTime = 0.0f;
	m_fTotalTime = 0.0f;
#else
	m_use_time_msec = 0;
	m_total_time_msec = 0;
#endif
}
