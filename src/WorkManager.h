#ifndef _WORK_MANAGER_H
#define _WORK_MANAGER_H

class WorkManager : public MapManager<SOCKET_FD, std::shared_ptr<DNSQureyWork>>, public InterfaceDNSQureyWork
{
public:
	WorkManager() {}
	~WorkManager() {}

	static ITEM_TYPE AllocWork();
	bool SaveWork(SOCKET_FD fd, ITEM_TYPE &wkWork);

	virtual void DNSQureyWorkClose(SOCKET_FD fdServer);

	void ClearTimeout();

	void ExitAndClear();

private:
	static const unsigned int DEF_TIME_OUT = 10000; // 10�볬ʱ
};


#endif

