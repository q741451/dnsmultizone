#ifndef _SLOG_H
#define _SLOG_H

// DEBUG 带源码位置，仅在 -v 时格式化输出
#ifdef _WIN32
#define SLOG_Debug(fmt, ...)	do { if (SLog::IsDebug()) SLog::LogDebug("[%s:%d %s] " fmt "\r\n", __FILE__,__LINE__,__FUNCTION__,##__VA_ARGS__); } while (0)
#define SLOG_Info(fmt, ...)		SLog::LogInfo(fmt "\r\n", ##__VA_ARGS__)
#define SLOG_Warn(fmt, ...)		SLog::LogWarn(fmt "\r\n", ##__VA_ARGS__)
#define SLOG_Error(fmt, ...)	SLog::LogError(fmt "\r\n", ##__VA_ARGS__)
#else
#define SLOG_Debug(fmt, ...)	do { if (SLog::IsDebug()) SLog::LogDebug("[%s:%d %s] " fmt"\n", __FILE__,__LINE__,__FUNCTION__,##__VA_ARGS__); } while (0)
#define SLOG_Info(fmt, ...)		SLog::LogInfo(fmt"\n", ##__VA_ARGS__)
#define SLOG_Warn(fmt, ...)		SLog::LogWarn(fmt"\n", ##__VA_ARGS__)
#define SLOG_Error(fmt, ...)	SLog::LogError(fmt"\n", ##__VA_ARGS__)
#endif

namespace SLog
{
	void SetDebug(bool bDebug);

	bool IsDebug();

	// 写日志文件时带时间戳；交给 syslog 等时由对方记录时间
	void SetTimestamp(bool bTimestamp);

	void LogDebug(const char* format, ...);

	void LogInfo(const char* format, ...);

	void LogWarn(const char* format, ...);

	void LogError(const char* format, ...);

	bool LogLog(FILE* fOut, const char* cTitile, const char* format, va_list pargs);
}

#endif
