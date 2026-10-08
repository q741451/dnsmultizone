#include "stdafx.h"

#define MAX_EVENT_NUMBER 10000
#define DEF_CLEAN_INTERVAL 1000		// ms：超时事务的清理间隔，也是 epoll_wait 的超时
#define DEF_CLIENT_PKG_LEN 0x400
#define DEF_READ_PER_EVENT 64		// 监听 socket 一次事件最多读几个包，余下的下一轮再读

int gFdExitEvent = -1;
int gFdEPollExit = -1;

static void SigEventInt(int)
{
	if(gFdEPollExit != -1 && gFdExitEvent != -1)
		modfd(gFdEPollExit, gFdExitEvent, EPOLLOUT);
}

// 监听 socket：SO_REUSEADDR 允许别的实例绑定同一端口（包只交给其中一个）。绑到 IPv6 地址时
// 关掉 V6ONLY 走双栈，必须显式设置：net.ipv6.bindv6only 可能被系统改成 1，不能依赖内核默认值；
// 并打开 IPV6_RECVPKTINFO，取每个查询发到的本机地址（见 SendReply）
static bool CreateSockServer(const struct sockaddr_storage &ssBind, socklen_t slBindLen, int *pFd)
{
	int iReuse = 1;
	int iV6Only = 0;
	int iPktInfo = 1;

	if ((*pFd = socket(ssBind.ss_family, SOCK_DGRAM, 0)) == -1)
		return false;

	if (setsockopt(*pFd, SOL_SOCKET, SO_REUSEADDR, (char*)&iReuse, sizeof(iReuse)) != 0)
		return false;

	if (ssBind.ss_family == AF_INET6 &&
		(setsockopt(*pFd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&iV6Only, sizeof(iV6Only)) != 0 ||
		setsockopt(*pFd, IPPROTO_IPV6, IPV6_RECVPKTINFO, (char*)&iPktInfo, sizeof(iPktInfo)) != 0))
		return false;

	return bind(*pFd, (struct sockaddr*)&ssBind, slBindLen) == 0;
}

static socklen_t AddrLen(const sockaddr_storage &ssAddr)
{
	return ssAddr.ss_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
}

// IPv6 查询发到的本机地址；IPv4（含双栈 socket 上的 v4-mapped）不取，照旧由内核选源地址
static bool GetLocalAddr(msghdr &mhMsg, const sockaddr_storage &ssClient, in6_pktinfo *pLocal)
{
	cmsghdr *pCmsg = NULL;

	if (ssClient.ss_family != AF_INET6 || IN6_IS_ADDR_V4MAPPED(&((const sockaddr_in6*)&ssClient)->sin6_addr))
		return false;

	for (pCmsg = CMSG_FIRSTHDR(&mhMsg); pCmsg != NULL; pCmsg = CMSG_NXTHDR(&mhMsg, pCmsg))
	{
		if (pCmsg->cmsg_level == IPPROTO_IPV6 && pCmsg->cmsg_type == IPV6_PKTINFO)
		{
			memcpy(pLocal, CMSG_DATA(pCmsg), sizeof(*pLocal));
			return true;
		}
	}
	return false;
}

// IPv6 应答从查询发到的本机地址发出。查询被 REDIRECT 过来时，目的地址改成了接口上的第一个
// 地址；接口有多个前缀时内核按客户端另选的源地址可能不同，回包对不上 conntrack，客户端收不到
static ssize_t SendReply(int fd, const ReplyAddr &raAddr, const std::string &sData)
{
	char cControl[CMSG_SPACE(sizeof(in6_pktinfo))];
	iovec ivData;
	msghdr mhMsg;
	cmsghdr *pCmsg = NULL;

	if (!raAddr.bHasLocal)
		return sendto(fd, sData.c_str(), sData.size(), 0, (const sockaddr*)&raAddr.ssClient, AddrLen(raAddr.ssClient));

	ivData.iov_base = (void*)sData.c_str();
	ivData.iov_len = sData.size();
	memset(&mhMsg, 0, sizeof(mhMsg));
	memset(cControl, 0, sizeof(cControl));
	mhMsg.msg_name = (void*)&raAddr.ssClient;
	mhMsg.msg_namelen = AddrLen(raAddr.ssClient);
	mhMsg.msg_iov = &ivData;
	mhMsg.msg_iovlen = 1;
	mhMsg.msg_control = cControl;
	mhMsg.msg_controllen = sizeof(cControl);
	pCmsg = CMSG_FIRSTHDR(&mhMsg);
	pCmsg->cmsg_level = IPPROTO_IPV6;
	pCmsg->cmsg_type = IPV6_PKTINFO;
	pCmsg->cmsg_len = CMSG_LEN(sizeof(in6_pktinfo));
	memcpy(CMSG_DATA(pCmsg), &raAddr.piLocal, sizeof(in6_pktinfo));
	return sendmsg(fd, &mhMsg, 0);
}

void Server::OnListenRead()
{
	std::string sBuffer;
	sockaddr_storage addrClient;
	char cControl[CMSG_SPACE(sizeof(in6_pktinfo))];
	iovec ivData;
	msghdr mhMsg;
	in6_pktinfo piLocal;
	std::string sKey;
	WorkManager::ITEM_TYPE spDNSQureyWork;
	int iLen = 0;
	int i = 0;

	for (i = 0; i < DEF_READ_PER_EVENT; i++)
	{
		sBuffer.resize(DEF_CLIENT_PKG_LEN);
		ivData.iov_base = (void*)sBuffer.c_str();
		ivData.iov_len = sBuffer.size();
		memset(&mhMsg, 0, sizeof(mhMsg));
		mhMsg.msg_name = &addrClient;
		mhMsg.msg_namelen = sizeof(addrClient);
		mhMsg.msg_iov = &ivData;
		mhMsg.msg_iovlen = 1;
		mhMsg.msg_control = cControl;
		mhMsg.msg_controllen = sizeof(cControl);
		if ((iLen = recvmsg(m_fdListen, &mhMsg, 0)) <= 0)
			break;
		sBuffer.resize(iLen);

		sKey = Util::AddrToString(addrClient);
		if (m_spWorkManager->GetItem(sKey, spDNSQureyWork) != true)
		{
			spDNSQureyWork = WorkManager::AllocWork();
			if (spDNSQureyWork->Init(addrClient, m_fdEPoll) != true || m_spWorkManager->SaveWork(sKey, spDNSQureyWork) != true)
			{
				SLOG_Error("client %s: work init failed", sKey.c_str());
				spDNSQureyWork->Exit();
				continue;
			}
		}

		spDNSQureyWork->m_spServerConnect->SetLocalAddr(GetLocalAddr(mhMsg, addrClient, &piLocal) ? &piLocal : NULL);
		spDNSQureyWork->m_spServerConnect->OnRecvData(sBuffer);
	}
}

void Server::OnListenWrite()
{
	while (m_lsSendQueue.size() > 0)
	{
		if (SendReply(m_fdListen, m_lsSendQueue.front().first, m_lsSendQueue.front().second) < 0 &&
			(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN))
			break;
		// 发出去了，或者是发不出去的错误：都不再留着
		m_lsSendQueue.pop_front();
	}

	UpdateListenEvents();
}

bool Server::SendTo(const ReplyAddr &raAddr, const std::string &sData)
{
	if (m_lsSendQueue.size() == 0)
	{
		if (SendReply(m_fdListen, raAddr, sData) >= 0)
			return true;
		if (errno != EINTR && errno != EWOULDBLOCK && errno != EAGAIN)
			return false;
	}

	m_lsSendQueue.push_back(std::make_pair(raAddr, sData));
	UpdateListenEvents();
	return true;
}

void Server::UpdateListenEvents()
{
	epoll_event event;

	event.data.fd = m_fdListen;
	event.events = EPOLLIN;
	if (m_lsSendQueue.size() > 0)
		event.events |= EPOLLOUT;
	epoll_ctl(m_fdEPoll, EPOLL_CTL_MOD, m_fdListen, &event);
}

Server gServer;

int main(int argc, char *argv[])
{
	int ret = -1;
	std::vector<epoll_event> eePollEvent(MAX_EVENT_NUMBER);
	std::shared_ptr<FileINotify> spFileINotify;
	int fdEPoll = -1;
	int fdListenServer = -1;
	int fdExitEvent = -1;
	unsigned long long llLastClean = 0;
	unsigned long long llNow = 0;
	ConnectionManager::ITEM_TYPE spSession;

	// procd / syslog 读的是管道，行缓冲才能逐行及时送达
	setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);

	if (gConfig.Init(argc, argv) != true)
	{
		SLOG_Error("Config.Init Fail");
		goto end;
	}

	if (gConfig.LoadConfig() != true)
	{
		SLOG_Error("Config.LoadConfig Fail");
		goto end;
	}

	signal(SIGINT, SigEventInt);	 /* Ctrl+C */

	if ((fdEPoll = epoll_create(5)) == -1)
	{
		SLOG_Error("epoll_create error");
		goto end;
	}

	if (CreateSockServer(gConfig.m_ssBindAddress, gConfig.m_slBindAddressLen, &fdListenServer) != true)
	{
		SLOG_Error("CreateSockServer Server Fail");
		goto end;
	}

	if ((fdExitEvent = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		SLOG_Error("CreateEvent Exit Fail");
		goto end;
	}

	// 进入后台模式
	if (gConfig.m_bIsBackMode)
	{
		SLOG_Info("Enter background mode running");
		if (daemon(1, gConfig.m_sFileLog.size() > 0 ? 1 : 0) != 0)
		{
			SLOG_Error("daemon Fail");
			goto end;
		}
		if (gConfig.m_sFileLog.size() > 0)
		{
			if (freopen(gConfig.m_sFileLog.c_str(), "w", stdout) == NULL ||
				freopen(gConfig.m_sFileLog.c_str(), "a", stderr) == NULL)
			{
				SLOG_Error("Open log file %s Fail", gConfig.m_sFileLog.c_str());
				goto end;
			}
			setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
			SLog::SetTimestamp(true);
		}
	}

	gConfig.RefreshResolvConf();

	spFileINotify = std::make_shared<FileINotify>(gConfig.m_vsZoneInfos);

	if (spFileINotify->Init(fdEPoll) != true)
	{
		SLOG_Error("FileINotify Init error!");
		goto end;
	}

	SLOG_Info("Server start, listen %s", Util::AddrToString(gConfig.m_ssBindAddress).c_str());

	gFdExitEvent = fdExitEvent;
	gFdEPollExit = fdEPoll;
	addfd(fdEPoll, gFdExitEvent, EPOLLIN, true);

	gServer.m_fdListen = fdListenServer;
	gServer.m_fdEPoll = fdEPoll;
	addfd(fdEPoll, fdListenServer, EPOLLIN, false);

	while (1)
	{
		int number = epoll_wait(fdEPoll, &eePollEvent[0], MAX_EVENT_NUMBER, DEF_CLEAN_INTERVAL);
		if ((number < 0) && (errno != EINTR))
		{
			SLOG_Error("EPoll failure");
			goto end;
		}

		// 本轮只处理事件，不关任何 socket：事务答完只是记下，超时清理也放到本轮之后，
		// 这样同一批事件里引用的对象和 fd 都还有效
		for (int i = 0; i < number; i++)
		{
			int fdEventSock = eePollEvent[i].data.fd;

			if (fdEventSock == fdListenServer)
			{
				if ((eePollEvent[i].events & (EPOLLIN | EPOLLOUT)) == 0)
				{
					SLOG_Error("fdListen Error!");
					goto end;
				}
				if (eePollEvent[i].events & EPOLLIN)
					gServer.OnListenRead();
				if (eePollEvent[i].events & EPOLLOUT)
					gServer.OnListenWrite();
			}
			else if (fdEventSock == fdExitEvent)
			{
				SLOG_Info("fdExitEvent active");
				goto end;
			}
			else if (spFileINotify->CheckInNotify(fdEventSock))
			{
				if (spFileINotify->INotify() != true)
				{
					SLOG_Error("INotify Error!");
					goto end;
				}
			}
			else if (gServer.m_spConnectionManager->GetItem(fdEventSock, spSession) != true)
			{
				SLOG_Error("unknown fd %d in epoll", fdEventSock);
				removefd(fdEPoll, fdEventSock);
			}
			else if (eePollEvent[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
				spSession->Disconnect();
			else if (eePollEvent[i].events & EPOLLIN)
				spSession->Read();
			else if (eePollEvent[i].events & EPOLLOUT)
				spSession->Write();
		}

		gServer.m_spWorkManager->CloseIdle();

		llNow = Util::GetRuntimeInMs();
		if (llNow - llLastClean >= DEF_CLEAN_INTERVAL)
		{
			gServer.m_spWorkManager->ClearTimeout();
			llLastClean = llNow;
		}
	}

	ret = 0;
end:
	// 清空Session资源
	gServer.m_spWorkManager->ExitAndClear();

	if (fdListenServer != -1)
		close(fdListenServer);

	if (fdExitEvent != -1)
		close(fdExitEvent);

	if (spFileINotify && fdEPoll != -1)
	{
		spFileINotify->Exit(fdEPoll);
		spFileINotify->Reset();
	}

	if (fdEPoll != -1)
		close(fdEPoll);

	return ret;
}