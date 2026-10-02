#ifndef _STDAFX_H
#define _STDAFX_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "AutoBuffer.h"
#include "cJSON.h"
#include "Util.h"
#include "SLog.h"
#include "ConfigType.h"
#include "Config.h"
#include "epoll_util.h"
#include "FileINotify.h"
#include "MapManager.h"
#include "Rfc1035.h"
#include "DNSConnect.h"
#include "ConnectionManager.h"
#include "ServerConnect.h"
#include "DNSQueryWorkItem.h"
#include "DNSQueryWork.h"
#include "WorkManager.h"
#include "Server.h"

#endif
