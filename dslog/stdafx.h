// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // �� Windows ͷ���ų�����ʹ�õ�����
// Windows ͷ�ļ�: 
#include <windows.h>
#include <WinInet.h>
#include <urlmon.h>

#pragma comment (lib, "Wininet.lib")
#pragma comment (lib, "urlmon.lib")

#undef min
#undef max

// C ����ʱͷ�ļ�
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <iostream>
#include <algorithm>

// TODO:  �ڴ˴����ó�����Ҫ������ͷ�ļ�
#include "SystemTraySDK.h"
#include "server.h"
#include "session.h"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/unordered_map.hpp>