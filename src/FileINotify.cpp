#include "stdafx.h"

#ifndef _WIN32

FileINotify::FileINotify(std::vector<std::shared_ptr<ZoneInfo>>& vsZoneInfos) : m_vsZoneInfos(vsZoneInfos)
{
	Reset();
}

bool FileINotify::Init(EPOLL_FD fdEPoll)
{
	bool ret = false;
	std::vector<std::shared_ptr<ZoneInfo>>::iterator iter;
	std::shared_ptr<ZoneInfo> spZoneInfo;
	std::string sRealFile;
	std::string sRealPath;
	std::string sRealFileName;
	bool bIsLink = false;
	size_t rFindChar = 0;
	int wd = -1;
	int iMaxLink = 20;

	if ((m_fdINotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC)) == -1)
	{
		SLOG_Error("inotify_init1 Error!");
		goto end;
	}

	addfd(fdEPoll, m_fdINotify, EPOLLIN, false);

	for (iter = m_vsZoneInfos.begin(); iter != m_vsZoneInfos.end(); ++iter)
	{
		spZoneInfo = (*iter);
		if (spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.size() == 0)
			continue;

		// 需要挂监视

		// 求真实文件
		sRealFile = spZoneInfo->m_rcfResolvConf.m_sResolvConfFile;
		do 
		{
			if (Util::ReadLinkAll(sRealFile.c_str(), sRealFile, &bIsLink) != true)
			{
				SLOG_Error("ReadLink Error, Ori File = %s, File = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealFile.c_str());
				goto end;
			}

			if (iMaxLink-- == 0)
			{
				SLOG_Error("Max link reach, Ori File = %s, File = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealFile.c_str());
				goto end;
			}

		} while (bIsLink == true);

		// 取文件夹
		if ((rFindChar = sRealFile.rfind('/')) == std::string::npos)
		{
			SLOG_Error("path none / Ori File = %s, File = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealFile.c_str());
			goto end;
		}
		sRealPath = sRealFile.substr(0, rFindChar);

		// 取文件
		sRealFileName = sRealFile.substr(rFindChar + 1);
		if (sRealFileName.size() == 0)
		{
			SLOG_Error("Real file name empty Ori File = %s, File = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealFile.c_str());
			goto end;
		}

		SLOG_Info("Add watch, Ori File = %s, Path = %s, Name = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealPath.c_str(), sRealFileName.c_str());

		if ((wd = inotify_add_watch(m_fdINotify, sRealPath.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO)) == -1)
		{
			SLOG_Error("Add watch failed, Ori File = %s, Path = %s", spZoneInfo->m_rcfResolvConf.m_sResolvConfFile.c_str(), sRealPath.c_str());
			goto end;
		}

		m_sfdFds.insert(wd);
		m_ssWatchFile.insert(sRealFileName);
	}

	ret = true;
end:
	if (ret != true)
	{
		Exit(fdEPoll);
		Reset();
	}
	return ret;
}

void FileINotify::Exit(EPOLL_FD fdEPoll)
{
	std::set<int>::iterator iter;

	if (m_fdINotify == -1)
		return;

	for (iter = m_sfdFds.begin(); iter != m_sfdFds.end(); ++iter)
	{
		inotify_rm_watch(m_fdINotify, *iter);
	}

	removefd(fdEPoll, m_fdINotify);

	close(m_fdINotify);
}

void FileINotify::Reset()
{
	m_fdINotify = -1;
	m_sReadBuffer.resize(NOTIFY_NMAX_READ_BUFFER);
	m_ssWatchFile.clear();
	m_sfdFds.clear();
}

bool FileINotify::CheckInNotify(int iFd)
{
	if (iFd == m_fdINotify)
		return true;
	else
		return false;
}

bool FileINotify::INotify()
{
	bool ret = false;
	struct inotify_event *in;
	char* p;
	int hit = 0;
	int rc = -1;

	rc = read(m_fdINotify, (char*)m_sReadBuffer.c_str(), m_sReadBuffer.size());

	if (rc < 0)
	{
		if (errno == EAGAIN)
			ret = true;
		goto end;
	}

	for (p = (char*)m_sReadBuffer.c_str(); rc - (p - (char*)m_sReadBuffer.c_str()) >= (int)sizeof(struct inotify_event); p += sizeof(struct inotify_event) + in->len)
	{
		size_t namelen;
		in = (struct inotify_event*)p;

		/* ignore emacs backups and dotfiles */
		if (in->len == 0 || (namelen = strlen(in->name)) == 0 ||
			in->name[namelen - 1] == '~' ||
			(in->name[0] == '#' && in->name[namelen - 1] == '#') ||
			in->name[0] == '.')
		{
			continue;
		}

		if (m_ssWatchFile.find(std::string(in->name)) != m_ssWatchFile.end())
		{
			hit = 1;
			SLOG_Info("INotify hit wd = %d, mask = %d, cookie = %d len = %d", in->wd, in->mask, in->cookie, in->len);
			if (in->len > 0)
				SLOG_Info("INotify hit name = %s", in->name);
		}

	}

	if (hit == 1)
	{
		SLOG_Info("INotify hit");
		gConfig.RefreshResolvConf();
	}

	ret = true;
end:
	return ret;
}

#endif
