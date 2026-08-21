#ifndef _RFC_1035_H
#define _RFC_1035_H

class Rfc1035
{
public:
	static const unsigned short DEF_TYPE_A     = 0x0001;
	static const unsigned short DEF_TYPE_CNAME = 0x0005;
	static const unsigned short DEF_TYPE_AAAA  = 0x001C;
	static const unsigned short DEF_TYPE_SVCB  = 0x0040;
	static const unsigned short DEF_TYPE_HTTPS = 0x0041;

	static const unsigned short DEF_SVCPARAM_IPV4HINT = 0x0004;
	static const unsigned short DEF_SVCPARAM_IPV6HINT = 0x0006;

	// 应答里带有可建连 IP 的类型，只有这些能拿去和 zone 的 ipList 比对，
	// 其余类型不携带任何归属信息
	static bool IsIPBearingType(unsigned short uQType);

	static bool ParseRequestA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, unsigned short *uQType);
	static bool ParseResponseA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, unsigned short *uQType,
		std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s);

private:
	static bool ParseRequestAAndAnswers(AutoBuffer &aBuffer, unsigned short *uID, unsigned short *uFlag, unsigned short *uAnswers, std::string &sName, unsigned short *uQType);
	static bool GetBufferName(AutoBuffer &aBuffer, std::string &sName);
	static bool SkipBufferName(AutoBuffer &aBuffer);
	static bool ParseSvcParamHint(std::string &sData, std::list<unsigned int> &luIPs, std::list<IPv6Addr> &luIP6s);
};


#endif
