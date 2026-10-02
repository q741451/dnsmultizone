#ifndef _SERVER_H
#define _SERVER_H

class Server
{
public:
	Server()
	{
		m_spConnectionManager = std::make_shared<ConnectionManager>();
		m_spWorkManager = std::make_shared<WorkManager>();
		m_fdListen = (SOCKET_FD)-1;
		m_fdEPoll = (EPOLL_FD)-1;
	}

	// 共享监听 socket：收所有客户端的查询，按来源地址交给事务；应答也从它发回
	void OnListenRead();
	void OnListenWrite();
	bool SendTo(const sockaddr_storage &ssAddr, const std::string &sData);

	std::shared_ptr<ConnectionManager> m_spConnectionManager;	// 上游 socket
	std::shared_ptr<WorkManager> m_spWorkManager;
	SOCKET_FD m_fdListen;
	EPOLL_FD m_fdEPoll;

private:
	void UpdateListenEvents();

	// 发送缓冲满时暂存的应答
	std::list<std::pair<sockaddr_storage, std::string>> m_lsSendQueue;
};

extern Server gServer;

#endif
