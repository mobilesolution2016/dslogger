#pragma once

#include "packets.h"

extern std::string	wxcfgToken, wxcfgSecret, wxcfgAppID;

class LogFormat
{
public:
	enum WhatEvent {
		kEventNormal,
		kEventClosed,
		kEventClearAll,
	};

	struct LogInfo {
		const char		*pszClientName;
		size_t			nameLeng;
		const void		*pData;
		size_t			size;
		WhatEvent		kEvent;
	};

public:
	virtual void onFormatData(std::string& strOut, const LogInfo& info) = 0;
};

class LogDump
{
public:
	virtual void onDumpData(const std::string& strMsg) = 0;
};

//////////////////////////////////////////////////////////////////////////
class ProjectConfig
{
public:
	// Socket����ģ��
	enum SocketMode {
		kSocketStandPackets,				// ��׼��DSLogger���ݰ�
		kSocketStrings,						// ���ַ��������ı�ʽ��
	};

	// Log��ʽ���õ�����
	enum LogFormatType {
		kLogFormatDefault,					// DSLogger���ø�ʽ
		kLogFormatCustom,
	};

	// Dump����
	enum DumpType {
		kDumpDispatchToWebSockets,			// ֱ�ӷַ������е�WebSockets
	};

public:
	ProjectConfig();
	~ProjectConfig();

	void addRef();
	int release();
	void setProjectId(uint32_t nId);

	static ProjectConfig* findExists(uint32_t nId);

public:
	SocketMode			kSocketMode;
	LogDump				*pDump;
	LogFormat			*pFormat;
	std::string			strProjectName;
	std::string			strApiToken;
	std::string			strLogBaseDir;
	int					iReferenceCount;
};

//////////////////////////////////////////////////////////////////////////
class LogFormatDefault : public LogFormat
{
public:
	void onFormatData(std::string& strOut, const LogInfo& info);
};

class LogDumpDispatchToWebSockets : public LogDump
{
public:
	void onDumpData(const std::string& strMsg);
};