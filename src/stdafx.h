#ifndef _STDAFX_H
#define _STDAFX_H

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <exception>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <string>
#include <signal.h>
#include <ctype.h>

#ifdef _WIN32

#define HAVE_STRUCT_TIMESPEC

#define SOCKET_FD SOCKET
#define SOCKET_CLOSE closesocket
#define EPOLL_FD HANDLE
#define EPOLL_CLOSE CloseHandle
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#include "WinLib/Src/wepoll.h"
#include "WinLib/Src/getopt.h"
#include <WinSock2.h>
#include <ws2tcpip.h>

#else

#define SOCKET_FD int
#define SOCKET_CLOSE close
#define EPOLL_FD int
#define EPOLL_CLOSE close

#include <unistd.h>
#include <getopt.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>

#endif

#include "AutoBuffer.h"
#include "cJSON.h"
#include "Util.h"
#include "SLog.h"
#include "ConfigType.h"
#include "Config.h"
#include "epoll_util.h"
#include "FileINotify.h"
#include "BaseConnect.h"
#include "MapManager.h"
#include "ConnectionManager.h"
#include "Rfc1035.h"
#include "DNSConnect.h"
#include "ServerConnect.h"
#include "DNSQueryWorkItem.h"
#include "DNSQueryWork.h"
#include "WorkManager.h"
#include "Server.h"

#endif
