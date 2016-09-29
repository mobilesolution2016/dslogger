#pragma once

enum LogType {
	kLogDebug,
	kLogVerbose,
	kLogInfo,
	kLogWarning,
	kLogError,
	kLogFault,
};

static const char szLogTypes[][16] = {
	{ "debug" },
	{ "verbose" },
	{ "info" },
	{ "warning" },
	{ "error" },
	{ "fault" },
};

struct PckHeader
{
	unsigned short		logType;
	unsigned short		msgLeng;
};