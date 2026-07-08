#ifndef _CONNECTION_MANAGER_H
#define _CONNECTION_MANAGER_H

typedef MapManager<SOCKET_FD, std::shared_ptr<BaseConnect>> ConnectionManager;

#ifdef WIN32
typedef MapManager<unsigned long long, SOCKET_FD> Win32ConnectionManager;
#endif

#endif
