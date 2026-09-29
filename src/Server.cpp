#include "stdafx.h"

#define MAX_EVENT_NUMBER 10000
#define DEF_GARBAGE_CLEAN_HIT 0x80

#ifdef _WIN32
static bool NetInit()
{
	WSADATA wsaData;
	WORD wVersionRequired = MAKEWORD(1, 1);
	if (WSAStartup(wVersionRequired, &wsaData) != 0)
		return false;

	return true;
}
#endif

SOCKET_FD gFdExitEvent = (SOCKET_FD)-1;
EPOLL_FD gFdEPollExit = (EPOLL_FD)-1;

static void SigEventInt(int sig)
{
	if(gFdEPollExit != (EPOLL_FD)-1 && gFdExitEvent != (SOCKET_FD)-1)
		modfd(gFdEPollExit, gFdExitEvent, EPOLLOUT);
}

// 以下三个 helper 抄自 quictun 的 quictun_socket_util.cc：
// 共享监听 socket 与每连接 socket 用同一套选项，IPv6 一律关掉 V6ONLY 走双栈

static bool SetReuseAddrAndPort(SOCKET_FD fd)
{
	int iReuse = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse)) != 0)
		return false;

#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (char*)&iReuse, sizeof(iReuse)) != 0)
		return false;
#endif

	return true;
}

// 绑到 IPv6 地址的 socket 同时接收 IPv4（v4-mapped）流量。
// 必须显式设置：net.ipv6.bindv6only 可能被系统改成 1，不能依赖内核默认值。
static bool SetIpv6OnlyDisabled(SOCKET_FD fd)
{
	int iV6Only = 0;

	return (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&iV6Only, sizeof(iV6Only)) == 0);
}

static SOCKET_FD CreateReusableUdpSocket(int iFamily)
{
	SOCKET_FD fd = socket(iFamily, SOCK_DGRAM, 0);

	if (fd == (SOCKET_FD)-1)
		return (SOCKET_FD)-1;

	if (SetReuseAddrAndPort(fd) != true)
		goto fail;

	if (iFamily == AF_INET6 && SetIpv6OnlyDisabled(fd) != true)
		goto fail;

	return fd;
fail:
	SOCKET_CLOSE(fd);
	return (SOCKET_FD)-1;
}

static bool CreateSockServer(const struct sockaddr_storage &ssBind, socklen_t slBindLen, SOCKET_FD *pFd)
{
	bool ret = false;

	*pFd = CreateReusableUdpSocket(ssBind.ss_family);

	if (*pFd == (SOCKET_FD)-1)
		goto end;

	if (bind(*pFd, (struct sockaddr*)&ssBind, slBindLen) < 0)
		goto end;

	ret = true;
end:
	return ret;
}

// 把新来的对端从共享监听 socket 迁到它自己的 socket 上：
// 建同样选项的 socket、bind 同一个监听地址、connect 到对端，
// 之后内核按四元组把该对端的包送到这个更具体的 socket
static SOCKET_FD UdpAccept(const struct sockaddr_storage &ssRemote, socklen_t slRemoteLen,
	const struct sockaddr_storage &ssBind, socklen_t slBindLen)
{
	bool ret = false;
	SOCKET_FD fd = CreateReusableUdpSocket(ssBind.ss_family);

	if (fd == (SOCKET_FD)-1)
		goto end;

	if (bind(fd, (struct sockaddr*)&ssBind, slBindLen) == -1)
		goto end;

	if (connect(fd, (struct sockaddr*)&ssRemote, slRemoteLen) != 0)
		goto end;

	ret = true;
end:
	if (ret == false)
	{
		if (fd != (SOCKET_FD)-1)
		{
			SOCKET_CLOSE(fd);
			fd = (SOCKET_FD)-1;
		}
	}
	return fd;
}

Server gServer;

int main(int argc, char *argv[])
{
	int ret = -1;
	std::vector<epoll_event> eePollEvent(MAX_EVENT_NUMBER);
	std::shared_ptr<FileINotify> spFileINotify;
	EPOLL_FD fdEPoll = (EPOLL_FD)-1;
	SOCKET_FD fdListenServer = (SOCKET_FD)-1;
	SOCKET_FD fdExitEvent = (SOCKET_FD)-1;
	int nCnnCntLoop = 0;
	ConnectionManager::ITEM_TYPE spSession;
	WorkManager::ITEM_TYPE spDNSQureyWork;
#ifdef WIN32
	std::string sPreReadBuffer;
#endif

	if (gConfig.Init(argc, argv) != true)
	{
		printf("Config.Init Fail\n");
		goto end;
	}

	if (gConfig.LoadConfig() != true)
	{
		printf("Config.LoadConfig Fail\n");
		goto end;
	}

#ifdef _WIN32
	if (NetInit() != true)
	{
		printf("NetInit Fail\n");
		goto end;
	}
#endif

	signal(SIGINT, SigEventInt);	 /* Ctrl+C */

	if ((fdEPoll = epoll_create(5)) == (EPOLL_FD)-1)
	{
		printf("epoll_create error\n");
		goto end;
	}

	if (CreateSockServer(gConfig.m_ssBindAddress, gConfig.m_slBindAddressLen, &fdListenServer) != true)
	{
		printf("CreateSockServer Server Fail\n");
		goto end;
	}

	if ((fdExitEvent = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("CreateEvent Exit Fail\n");
		goto end;
	}

	// 进入后台模式
#ifndef _WIN32
	if (gConfig.m_bIsBackMode)
	{
		printf("Enter background mode running\n");
		daemon(1, gConfig.m_sFileLog.size() > 0 ? 1 : 0);
		if (gConfig.m_sFileLog.size() > 0)
		{
			freopen(gConfig.m_sFileLog.c_str(), "w", stdout);
			setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
			freopen(gConfig.m_sFileLog.c_str(), "a", stderr);
		}
	}
#endif

	gConfig.RefreshResolvConf();

#ifndef _WIN32

	spFileINotify = std::make_shared<FileINotify>(gConfig.m_vsZoneInfos);

	if (spFileINotify->Init(fdEPoll) != true)
	{
		SLOG_Error("FileINotify Init error!");
		goto end;
	}

#endif

	SLOG_Info("Server Start!");

	gFdExitEvent = fdExitEvent;
	gFdEPollExit = fdEPoll;
	addfd(fdEPoll, gFdExitEvent, EPOLLIN, true);

	addfd(fdEPoll, fdListenServer, EPOLLIN, false);

	while (1)
	{
		int number = epoll_wait(fdEPoll, &eePollEvent[0], MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR))
		{
			SLOG_Error("EPoll failure");
			goto end;
		}

		for (int i = 0; i < number; i++)
		{
			SOCKET_FD fdEventSock = eePollEvent[i].data.fd;

			if (fdEventSock == fdListenServer)
			{
				if (eePollEvent[i].events & EPOLLIN)
				{
					struct sockaddr_storage addrClient;
					socklen_t sockLenClient = sizeof(addrClient);

#ifdef WIN32
					unsigned long long ulClientInfoWin32 = 0ll;

					{
						SOCKET_FD fdWin32 = (SOCKET_FD)-1;
						int nRecvLen = 0;
						sPreReadBuffer.resize(0x400);

						if ((nRecvLen = recvfrom(fdEventSock, (char*)sPreReadBuffer.c_str(), (int)sPreReadBuffer.size(), 0, (sockaddr*)&addrClient, &sockLenClient)) < 0 && (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN))
							goto end;

						sPreReadBuffer.resize(nRecvLen);

						ulClientInfoWin32 = (((unsigned long long)addrClient.sin_port) << 32) + ntohl(addrClient.sin_addr.s_addr);
						if (gServer.m_spWin32ConnectionManager->GetItem(ulClientInfoWin32, fdWin32) == true)
						{
							fdEventSock = fdWin32;
							goto client_fd_label;
						}
					}
#endif

					spDNSQureyWork = WorkManager::AllocWork();

#ifdef WIN32
					if (sPreReadBuffer.size() > 0)
					{
						spDNSQureyWork->m_spServerConnect->SetPreReadBuff(sPreReadBuffer);
						sPreReadBuffer.clear();
					}
					spDNSQureyWork->m_spServerConnect->m_ulClientInfoWin32 = ulClientInfoWin32;
#endif
					if (spDNSQureyWork->m_spServerConnect->PrepareRecvByRecvFrom(fdEventSock, (sockaddr*)&addrClient, &sockLenClient) != true)
						continue;

					SOCKET_FD fdConn = UdpAccept(addrClient, sockLenClient,
						gConfig.m_ssBindAddress, gConfig.m_slBindAddressLen);

					if (fdConn == (SOCKET_FD) -1)
					{
						SLOG_Error("errno is: %d", errno);
						continue;
					}

					if (spDNSQureyWork->Init(fdConn, fdEPoll, addrClient) != true)
					{
						SLOG_Error("spDNSQureyWork Init failed!\n");
						goto end;
					}

					if (gServer.m_spWorkManager->SaveWork(fdConn, spDNSQureyWork) != true)
					{
						SLOG_Error("spWorkManager SaveWork failed!\n");
						goto end;
					}

					spDNSQureyWork->m_spServerConnect->OnRecvDataFirst();

					// 垃圾清理
					nCnnCntLoop++;
					if (nCnnCntLoop % DEF_GARBAGE_CLEAN_HIT == 0)
					{
						gServer.m_spWorkManager->ClearTimeout();
					}
				}
				else
				{
					SLOG_Error("fdListen Error!\n");
					goto end;
				}
			}
			else if (fdEventSock == fdExitEvent)
			{
				SLOG_Info("fdExitEvent acitve!\n");
				goto end;
			}
#ifndef WIN32
			else if (spFileINotify->CheckInNotify(fdEventSock))
			{
				if (spFileINotify->INotify() != true)
				{
					SLOG_Error("INotify Error!\n");
					goto end;
				}
			}
#endif
			else
			{
#ifdef WIN32
				client_fd_label:
#endif
				if (eePollEvent[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
				{
					if (gServer.m_spConnectionManager->GetItem(fdEventSock, spSession) != true)
					{
						SLOG_Error("spConnectionManagerDNS->GetItem Error failed!\n");
						removefd(fdEPoll, fdEventSock);
						SOCKET_CLOSE(fdEventSock);
					}
					else
					{
						spSession->Disconnect();
					}
				}
				else if (eePollEvent[i].events & EPOLLIN)
				{
					if (gServer.m_spConnectionManager->GetItem(fdEventSock, spSession) != true)
					{
						SLOG_Error("spConnectionManagerDNS->GetItem EPollIn failed! fd = %d", fdEventSock);
						removefd(fdEPoll, fdEventSock);
						SOCKET_CLOSE(fdEventSock);
					}
					else
					{
#ifdef WIN32
						if (sPreReadBuffer.size() > 0)
						{
							spSession->SetPreReadBuff(sPreReadBuffer);
							sPreReadBuffer.clear();
						}
#endif
						spSession->Read();
					}

					continue;
				}
				else if (eePollEvent[i].events & EPOLLOUT)
				{
					if (gServer.m_spConnectionManager->GetItem(fdEventSock, spSession) != true)
					{
						SLOG_Error("spConnectionManagerDNS->GetItem EPollOut failed! fd = %d", fdEventSock);
						removefd(fdEPoll, fdEventSock);
						SOCKET_CLOSE(fdEventSock);
					}
					else
					{
						spSession->Write();
						continue;
					}
				}
				else
				{
					SLOG_Error("Unknown EPoll!\n");
				}
			}
		}
	}

	ret = 0;
end:
	// 清空Session资源
	gServer.m_spWorkManager->ExitAndClear();

	if (fdListenServer != (SOCKET_FD)-1)
		SOCKET_CLOSE(fdListenServer);

	if (fdExitEvent != (SOCKET_FD)-1)
		SOCKET_CLOSE(fdExitEvent);

#ifndef WIN32
	if (spFileINotify && fdEPoll != (EPOLL_FD)-1)
	{
		spFileINotify->Exit(fdEPoll);
		spFileINotify->Reset();
	}
#endif

	if (fdEPoll != (EPOLL_FD)-1)
		EPOLL_CLOSE(fdEPoll);

	return ret;
}