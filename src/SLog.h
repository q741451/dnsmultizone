#ifndef _SLOG_H
#define _SLOG_H

#ifdef _WIN32
#define SLOG_Debug(fmt, ...)	SLog::LogDebug("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Info(fmt, ...)		SLog::LogInfo("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Warn(fmt, ...)		SLog::LogWarn("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Error(fmt, ...)	SLog::LogError("[%s\t%s\t%d]\t" ##fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#else
#define SLOG_Debug(fmt, ...)	SLog::LogDebug("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Info(fmt, ...)		SLog::LogInfo("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Warn(fmt, ...)		SLog::LogWarn("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#define SLOG_Error(fmt, ...)	SLog::LogError("[%s\t%s\t%d]\t" fmt"\r\n",		__FILE__,__FUNCTION__,__LINE__,##__VA_ARGS__)
#endif

namespace SLog
{
	void LogDebug(const char* format, ...);

	void LogInfo(const char* format, ...);

	void LogWarn(const char* format, ...);

	void LogError(const char* format, ...);

	bool LogLog(const char* cTitile, const char* format, va_list pargs);
}

#endif

