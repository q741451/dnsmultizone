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

static bool CreateSockServer(int iType, unsigned long ulAddress, unsigned short usPort, SOCKET_FD *pFd)
{
	struct sockaddr_in sBindAddress;
	int iReuse = 1;
	bool ret = false;

	*pFd = socket(PF_INET, iType, 0);

	if (*pFd == (SOCKET_FD)-1)
		goto end;

	if (setsockopt(*pFd, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse)) != 0)
		goto end;

	memset(&sBindAddress, 0, sizeof(struct sockaddr_in));
	sBindAddress.sin_family = PF_INET;
	sBindAddress.sin_addr.s_addr = htonl(ulAddress);
	sBindAddress.sin_port = htons(usPort);

	if (bind(*pFd, (struct sockaddr*)&sBindAddress, sizeof(sBindAddress)) < 0)
		goto end;

	if (iType == SOCK_STREAM)
	{
		if (listen(*pFd, 5) < 0)
			goto end;
	}

	ret = true;
end:
	return ret;
}

static SOCKET_FD UdpAccept(struct sockaddr_in &addrRemote,
	struct in_addr &addrBind, unsigned short uBindPort)
{
	bool ret = false;
	int iReuse = 1;
	struct sockaddr_in addrLocal;
	SOCKET_FD fd = socket(PF_INET, SOCK_DGRAM, 0);

	if (fd == (SOCKET_FD)-1)
		goto end;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse)) != 0)
		goto end;

	memset(&addrLocal, 0, sizeof(addrLocal));
	addrLocal.sin_family = PF_INET;
	addrLocal.sin_port = htons(uBindPort);
	addrLocal.sin_addr = addrBind;
	if (bind(fd, (struct sockaddr *) &addrLocal, sizeof(addrLocal)) == -1)
		goto end;

	if (connect(fd, (struct sockaddr*)&addrRemote, sizeof(addrRemote)) != 0)
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

	if (gConfig.LoadConfigJson() != true)
	{
		printf("Config.LoadConfigJson Fail\n");
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

	if (CreateSockServer(SOCK_DGRAM, ntohl(gConfig.m_iaBindAddress.s_addr), gConfig.m_uServerPort, &fdListenServer) != true)
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
					struct sockaddr_in addrClient;
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

					SOCKET_FD fdConn = UdpAccept(addrClient, gConfig.m_iaBindAddress, gConfig.m_uServerPort);

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