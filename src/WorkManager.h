#ifndef _WORK_MANAGER_H
#define _WORK_MANAGER_H

// 按客户端地址（Util::AddrToString）存放事务
class WorkManager : public MapManager<std::string, std::shared_ptr<DNSQureyWork>>, public InterfaceDNSQureyWork
{
public:
	WorkManager() {}
	~WorkManager() {}

	static ITEM_TYPE AllocWork();
	bool SaveWork(const std::string &sKey, ITEM_TYPE &wkWork);

	// 事务没有待答的查询了：先记下，等本轮事件处理完再关
	virtual void DNSQureyWorkClose(const std::string &sKey);

	// 以下只在一轮事件处理完之后调用
	void CloseIdle();
	void ClearTimeout();

	void ExitAndClear();

private:
	static const unsigned int DEF_TIME_OUT = 10000; // 10秒超时

	std::set<std::string> m_ssIdle;
};


#endif
