#pragma once

#define WM_ICON_BALLOON		WM_USER + 1

extern HWND	hMainWnd;

extern void startSocketServer(const char* listenIp = 0);
extern void stopSocketServer();