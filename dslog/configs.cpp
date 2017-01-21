#include "stdafx.h"
#include "socksvr.h"

std::string	wxcfgToken, wxcfgSecret, wxcfgAppID;

ProjectConfig::ProjectConfig()
	: pDump		(NULL)
	, pFormat	(NULL)
{

}

ProjectConfig::~ProjectConfig()
{

}

//////////////////////////////////////////////////////////////////////////
void formatNameAndTime(std::string& strOutput, const char* name)
{
	size_t leng;
	char szFmtBuf[64];
	SYSTEMTIME curTime;

#ifdef _WINDOWS_
	GetLocalTime(&curTime);
	if (name)
		leng = sprintf(szFmtBuf, "%s(%04d-%04d-%04d %02d:%02d:%02d)", name, curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour, curTime.wMinute, curTime.wSecond);
	else
		leng = sprintf(szFmtBuf, "(%04d-%04d-%04d %02d:%02d:%02d)", curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour, curTime.wMinute, curTime.wSecond);
#else
	time_t tcur;

	time(&tcur);
	struct tm* t = localtime(&tcur);

	if (name)
		leng = sprintf(szFmtBuf, "%s(%04d-%04d-%04d %02d:%02d:%02d)", name, t->tm_year + 1900, t->tm_mon + 1, t->tm_wday + 1, t->tm_hour, t->tm_sec, t->tm_sec);
	else
		leng = sprintf(szFmtBuf, "(%04d-%04d-%04d %02d:%02d:%02d)", t->tm_year + 1900, t->tm_mon + 1, t->tm_wday + 1, t->tm_hour, t->tm_sec, t->tm_sec);
#endif

	strOutput.append(szFmtBuf, leng);
}

void LogFormatDefault::onFormatData(std::string& strOut, const LogInfo& info)
{
	char idBuf[32] = { 0 };
	std::string& strMsg = strOut;	
	PckHeader* hd = (PckHeader*)info.pData;

	strMsg.reserve(hd->msgLeng + 200);

	if (info.kEvent == kEventNormal)
	{
		// name
		strMsg.append(info.pszClientName, info.nameLeng);
		strMsg += '|';

		// type
		strMsg += szLogTypes[hd->logType];

		// time
		formatNameAndTime(strMsg, NULL);

		// msg
		strMsg.append((char*)(hd + 1), hd->msgLeng);
	}
	else if (info.kEvent == kEventClearAll)
	{
		formatNameAndTime(strMsg, "*clear");
	}
	else if (info.kEvent == kEventClosed)
	{
		// name
		strMsg.append(info.pszClientName, info.nameLeng);
		strMsg += '|';

		// time
		formatNameAndTime(strMsg, "*closed");
	}
}

//////////////////////////////////////////////////////////////////////////
void LogDumpDispatchToWebSockets::onDumpData(const std::string& strMsg)
{
	// send
	sessionsListLock.lock();
	for (WSessions::iterator el = sessions.begin(); el != sessions.end(); ++ el)
		(*el)->write(strMsg);
	sessionsListLock.unlock();
}