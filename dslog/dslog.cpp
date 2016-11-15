// thanks https://github.com/Jontte/miniws for websocket server

#include "stdafx.h"
#include "dslog.h"
#include "socksvr.h"

#define MAX_LOADSTRING 100
#define	WM_ICON_NOTIFY WM_APP+10

// ȫ�ֱ���: 
HWND hMainWnd;
HINSTANCE hInst;                                // ��ǰʵ��
WCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
WCHAR szWindowClass[MAX_LOADSTRING];            // ����������
CSystemTray TrayIcon;

std::vector<std::string> localAddress;
extern std::string strListenIp;

// �˴���ģ���а����ĺ�����ǰ������: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    IpWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: �ڴ˷��ô��롣
	WSADATA wsa;
	WSAStartup(MAKELONG(2, 2), &wsa);

	char ac[80] = { 0 };

	localAddress.push_back("127.0.0.1");
	if (gethostname(ac, sizeof(ac)) != SOCKET_ERROR)
	{
		struct hostent *phe = gethostbyname(ac);
		if (phe)
		{			
			for (int i = 0; phe->h_addr_list[i] != 0; ++ i)
			{
				struct in_addr addr;
				memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
				localAddress.push_back(inet_ntoa(addr));
			}
		}
	}

    // ��ʼ��ȫ���ַ���
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DSLOG, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // ִ��Ӧ�ó����ʼ��: 
    if (!InitInstance (hInstance, nCmdShow))
        return FALSE;
	
	// ѡ��IP
	if (localAddress.size() > 1)
	{
		if (DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG_IP), hMainWnd, IpWndProc) == IDCANCEL)
		{
			DestroyWindow(hMainWnd);
			return 0;
		}
	}

	// ��ʼ����
	startSocketServer();

	MSG msg;
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DSLOG));    

    // ����Ϣѭ��: 
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	stopSocketServer();

	WSACleanup();
    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DSLOG));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����

	hMainWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hMainWnd)
		return FALSE;

   	if (!TrayIcon.Create(hInstance, hMainWnd, WM_ICON_NOTIFY, _T("����Ҽ��ᵯ���˵� (DSLogger)"), ::LoadIcon(hInstance, (LPCTSTR)IDI_SMALL), IDC_DSLOG))
        return FALSE;

	ShowWindow(hMainWnd, SW_HIDE);
	UpdateWindow(hMainWnd);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hMainWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_ICON_NOTIFY:
		return TrayIcon.OnTrayNotification(wParam, lParam);

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // �����˵�ѡ��: 
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hMainWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hMainWnd);
                break;
            default:
                return DefWindowProc(hMainWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hMainWnd, &ps);

            EndPaint(hMainWnd, &ps);
        }
        break;
	case WM_ICON_BALLOON:
	{
		std::wstring* msg = (std::wstring*)wParam;
		std::wstring* title = (std::wstring*)lParam;

		TrayIcon.ShowBalloon(msg->c_str(), title->c_str());

		delete msg;
		delete title;
	}
		break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hMainWnd, message, wParam, lParam);
    }
    return 0;
}

// �����ڡ������Ϣ�������
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


INT_PTR CALLBACK IpWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		for (size_t i = 0; i < localAddress.size(); ++ i)
			SendDlgItemMessageA(hDlg, IDC_IP_LIST, LB_ADDSTRING, 0, (LPARAM)localAddress[i].c_str());

		RECT rc;
		GetWindowRect(hDlg, &rc);
		SetWindowPos(hDlg, 0, (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

		SendDlgItemMessage(hDlg, IDC_IP_LIST, LB_SETCURSEL, 0, 0);
	}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			size_t i = SendDlgItemMessage(hDlg, IDC_IP_LIST, LB_GETCURSEL, 0, 0);
			if (i >= 0 && i < localAddress.size())
				strListenIp = localAddress[i];

			EndDialog(hDlg, LOWORD(wParam));
		}
		else if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
		}
		
		return (INT_PTR)TRUE;
	}
	return (INT_PTR)FALSE;
}