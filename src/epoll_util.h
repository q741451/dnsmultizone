#ifndef _EPOLL_UTIL_H
#define _EPOLL_UTIL_H

int setnonblocking(int fd);

void addfd(int epollfd, int fd, int ev, bool one_shot);

void removefd(int epollfd, int fd);

int modfd(int epollfd, int fd, int ev);


#endif
