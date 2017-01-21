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
	// Socket数据模型
	enum SocketMode {
		kSocketStandPackets,				// 标准的DSLogger数据包
		kSocketStrings,						// 纯字符串（即文本式）
	};

	// Log格式可用的类型
	enum LogFormatType {
		kLogFormatDefault,					// DSLogger内置格式
		kLogFormatCustom,
	};

	// Dump类型
	enum DumpType {
		kDumpDispatchToWebSockets,			// 直接分发给所有的WebSockets
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