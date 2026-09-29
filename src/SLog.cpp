#include "stdafx.h"

static bool s_bDebug = false;
static bool s_bTimestamp = false;

void SLog::SetDebug(bool bDebug)
{
	s_bDebug = bDebug;
}

bool SLog::IsDebug()
{
	return s_bDebug;
}

void SLog::SetTimestamp(bool bTimestamp)
{
	s_bTimestamp = bTimestamp;
}

void SLog::LogDebug(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	LogLog(stdout, "DEBUG", format, args);
	va_end(args);
}

void SLog::LogInfo(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	LogLog(stdout, "INFO ", format, args);
	va_end(args);
}

void SLog::LogWarn(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	LogLog(stderr, "WARN ", format, args);
	va_end(args);
}

void SLog::LogError(const char* format, ...)
{
	va_list args;

	va_start(args, format);
	LogLog(stderr, "ERROR", format, args);
	va_end(args);
}

bool SLog::LogLog(FILE* fOut, const char* cTitile, const char* format, va_list pargs)
{
	char cHead[32] = { 0 };
	time_t time_seconds = time(NULL);
	struct tm now_time;

	if (s_bTimestamp)
	{
#ifdef _WIN32
		localtime_s(&now_time, &time_seconds);
#else
		localtime_r(&time_seconds, &now_time);
#endif
		snprintf(cHead, sizeof(cHead), "%02u%02u%02u%02u%02u ", now_time.tm_mon + 1, now_time.tm_mday,
			now_time.tm_hour, now_time.tm_min, now_time.tm_sec);
	}

	fprintf(fOut, "%s%s ", cHead, cTitile);
	vfprintf(fOut, format, pargs);

	return true;
}
