#include "stdafx.h"


#ifdef _WIN32
int setnonblocking(SOCKET_FD fd)
{
	unsigned long ul = 1;
	int r = ioctlsocket(fd, FIONBIO, &ul);
	return r;
}
#else
int setnonblocking(SOCKET_FD fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}
#endif


void addfd(EPOLL_FD epollfd, SOCKET_FD fd, int ev, bool one_shot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLRDHUP;
	if (one_shot)
	{
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}


void removefd(EPOLL_FD epollfd, SOCKET_FD fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	SOCKET_CLOSE(fd);
}

int modfd(EPOLL_FD epollfd, SOCKET_FD fd, int ev)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
	return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}


