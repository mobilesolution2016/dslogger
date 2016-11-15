// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料
// Windows 头文件: 
#include <windows.h>
#include <WinInet.h>
#include <urlmon.h>

#pragma comment (lib, "Wininet.lib")
#pragma comment (lib, "urlmon.lib")

#undef min
#undef max

// C 运行时头文件
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <iostream>
#include <algorithm>

// TODO:  在此处引用程序需要的其他头文件
#include "SystemTraySDK.h"
#include "server.h"
#include "session.h"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>