#ifndef _FILE_INOTIFY_H
#define _FILE_INOTIFY_H

#define NOTIFY_NAME_MAX 255
#define NOTIFY_NMAX_READ_BUFFER (sizeof(struct inotify_event) + NOTIFY_NAME_MAX)

class FileINotify
{
public:
	FileINotify(std::vector<std::shared_ptr<ZoneInfo>>& vsZoneInfos);

	bool Init(EPOLL_FD fdEPoll);
	void Exit(EPOLL_FD fdEPoll);
	void Reset();

	bool CheckInNotify(int iFd);

	bool INotify();

private:
	std::vector<std::shared_ptr<ZoneInfo>> &m_vsZoneInfos;
	std::set<int>						m_sfdFds;
	std::set<std::string>				m_ssWatchFile;
	std::string						m_sReadBuffer;
	int						m_fdINotify;
};

#endif
