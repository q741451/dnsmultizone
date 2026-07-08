#ifndef _EPOLL_UTIL_H
#define _EPOLL_UTIL_H

int setnonblocking(SOCKET_FD fd);

void addfd(EPOLL_FD epollfd, SOCKET_FD fd, int ev, bool one_shot);

void removefd(EPOLL_FD epollfd, SOCKET_FD fd);

int modfd(EPOLL_FD epollfd, SOCKET_FD fd, int ev);


#endif
