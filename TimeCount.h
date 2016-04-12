//*********************************************************
// 文件名：TimeCount.h
// 作  者：hek_win7
// 日  期：2011-08-10
// 目  的：精确计时类 ——头文件
//*********************************************************

#pragma once

/// @brief 精确计时类
///
/// 使用说明：\n
/// SetStartTime()和SetEndTime()需要成对使用，看作一次操作计时，GetUseTime()返回该次操作的精确计时（毫秒）\n
/// 同时，GetUseTime()传参为TRUE的话，可以累计多次操作的总时间，使用GetTotalTime()得到。\n
class CTimeCount
{
public:
	CTimeCount(void);
	~CTimeCount(void);

	/// 设置开始时间
	static void SetStartTime();

	/// 设置结束时间
	static void SetEndTime();

	/// 获取用时
	static double GetUseTime(bool bAccumulate = false);

	/// 获取总时间
	static double GetTotalTime();

	/// 所有时间重置
	static void ResetTime();

private:
#ifdef Q_OS_WIN
	static LARGE_INTEGER	m_startTime;	///< 启动时间
	static LARGE_INTEGER	m_endTime;		///< 结束时间
	static LARGE_INTEGER	m_frequency;	///< CPU时钟频率
	
	static double		m_fUseTime;		///< 每次用时(毫秒)
	static double		m_fTotalTime;	///< 总计用时(毫秒)
	
#else
	static unsigned long m_start_time_msec;
	static unsigned long m_end_time_msec;
	static unsigned long m_frequency_msec;
	
	static unsigned long m_use_time_msec;
	static unsigned long m_total_time_msec;
#endif
	
	
};
