#ifndef _SERVER_H
#define _SERVER_H

class Server
{
public:
	Server()
	{
		m_spConnectionManager = std::make_shared<ConnectionManager>();
		m_spWorkManager = std::make_shared<WorkManager>();
#ifdef WIN32
		m_spWin32ConnectionManager = std::make_shared<Win32ConnectionManager>();
#endif
	}
	std::shared_ptr<ConnectionManager> m_spConnectionManager;
	std::shared_ptr<WorkManager> m_spWorkManager;
#ifdef WIN32
	std::shared_ptr<Win32ConnectionManager> m_spWin32ConnectionManager;
#endif
};

extern Server gServer;

#endif