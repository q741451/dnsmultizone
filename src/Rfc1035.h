#ifndef _RFC_1035_H
#define _RFC_1035_H

class Rfc1035
{
public:
	static bool ParseRequestA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, bool *bIsA);
	static bool ParseResponseA(std::string &sBuffer, unsigned short *uID, unsigned short *uFlag, std::string &sName, bool *bIsA, std::list<unsigned int> &luIPs);

private:
	static bool ParseRequestAAndAnswers(AutoBuffer &aBuffer, unsigned short *uID, unsigned short *uFlag, unsigned short *uAnswers, std::string &sName, bool *bIsA);
	static bool GetBufferName(AutoBuffer &aBuffer, std::string &sName);
	static bool SkipBufferName(AutoBuffer &aBuffer);
};


#endif
