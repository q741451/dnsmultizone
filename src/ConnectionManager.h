#ifndef _CONNECTION_MANAGER_H
#define _CONNECTION_MANAGER_H

typedef MapManager<SOCKET_FD, std::shared_ptr<BaseConnect>> ConnectionManager;

#endif
