#include "stdafx.h"
#include "packets.h"
#include "socksvr.h"

static bool bEventLooping = false;
static bool bMainRunning = false, bWSRunning = false;

boost::asio::io_service ioService;
boost::asio::io_service wsService;
boost::asio::io_service::work work(ioService);
boost::asio::ip::tcp::acceptor acceptor(ioService);
boost::asio::ip::tcp::resolver resolver(ioService);

boost::mutex clientsListLock;
boost::mutex sessionsListLock;

class Client
{
public:
	typedef boost::asio::ip::tcp::socket tcpsocket;	

public:
	Client() 
		: sock(ioService)
	{}
	~Client() 
	{}

	void start()
	{
		boost::asio::async_read(sock,
			boost::asio::buffer((char*)&pckHeader, sizeof(PckHeader)),
			boost::bind(&Client::onReadHeader, this, boost::asio::placeholders::error)
		);
	}

	void readLength(PckHeader& hd, size_t leng)
	{
		PckHeader* dst = (PckHeader*)malloc(leng + sizeof(hd));
		*dst = hd;

		boost::asio::async_read(sock,
			boost::asio::buffer((char*)(dst + 1), leng),
			boost::bind(&Client::onReadData, this, (char*)dst, boost::asio::placeholders::error)
		);
	}

	void stop()
	{
		boost::system::error_code ec;
		sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		sock.close(ec);
	}

	static void acceptOne();

	void onReadHeader(const boost::system::error_code& error);
	void onReadData(char* mem, const boost::system::error_code& error);

public:
	tcpsocket			sock;
	PckHeader			pckHeader;	
};

class Session : public WS::Session
{
public:
	void on_connect();
	void on_disconnect();
	void on_message(const string& msg);
};

//////////////////////////////////////////////////////////////////////////
typedef std::list<Client*> AllClients;
AllClients clients;

typedef std::list<Session*> WSessions;
WSessions	sessions;

DWORD WINAPI serverThread(LPVOID)
{
	boost::asio::ip::tcp::resolver::query query(
		"127.0.0.1",
		boost::lexical_cast< std::string >(9880)
		);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

	acceptor.open(endpoint.protocol());
	acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(false));
	acceptor.bind(endpoint);
	acceptor.listen(boost::asio::socket_base::max_connections);

	Client::acceptOne();

	bMainRunning = true;
	bEventLooping = true;

	boost::system::error_code ec;
	while (bEventLooping)
	{
		try
		{
			ioService.run(ec);

			break;
		}
		catch (std::exception & ex)
		{
			MessageBoxA(GetActiveWindow(), ex.what(), "Socket Catched Exception", MB_OK | MB_ICONERROR);
		}
	}

	acceptor.close(ec);

	clientsListLock.lock();
	while (clients.size())
	{
		Client* c = clients.front();
		clients.pop_front();
		c->stop();
		delete c;
	}
	clientsListLock.unlock();

	ioService.reset();

	bMainRunning = false;

	return 0;
}

void Session::on_connect()
{
	sessionsListLock.lock();
	sessions.push_back(this);
	sessionsListLock.unlock();

	std::wstring* msg = new std::wstring(_T("日志察看者已连接"));
	std::wstring* title = new std::wstring(_T("DSLog 提醒"));

	PostMessage(hMainWnd, WM_ICON_BALLOON, (WPARAM)msg, (LPARAM)title);
}

void Session::on_disconnect()
{
	sessionsListLock.lock();
	sessions.remove(this);
	sessionsListLock.unlock();
}

void Session::on_message(const string& msg)
{
	
}

DWORD WINAPI threadWebSocket(LPVOID)
{
	auto server = WS::Server::create<>(wsService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 9080));
	server->handle_resource<Session>("/dslog");

	bWSRunning = true;
	while (bEventLooping)
	{
		try
		{
			wsService.run();
		}
		catch (std::exception& e)
		{
			MessageBoxA(GetActiveWindow(), e.what(), "WS Catched Exception", MB_OK | MB_ICONERROR);
		}
	}
	bWSRunning = false;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void startSocketServer()
{
	DWORD dwId = 0;
	HANDLE h = CreateThread(0, 0, &serverThread, 0, 0, &dwId);
	CloseHandle(h);

	h = CreateThread(0, 0, &threadWebSocket, 0, 0, &dwId);
	CloseHandle(h);
}

void stopSocketServer()
{
	bEventLooping = false;
	ioService.stop();
	wsService.stop();

	while(bMainRunning || bWSRunning)
		Sleep(50);
}

void onAccept(Client* c, const boost::system::error_code& error)
{
	if (error)
	{
		delete c;
		return;
	}

	clientsListLock.lock();
	clients.push_back(c);
	clientsListLock.unlock();

	c->start();

	Client::acceptOne();
}

//////////////////////////////////////////////////////////////////////////
void Client::acceptOne()
{
	Client* c = new Client();
	acceptor.async_accept(c->sock, boost::bind(&onAccept, c, boost::asio::placeholders::error));
}

void Client::onReadHeader(const boost::system::error_code& error)
{
	if (error)
	{
		clientsListLock.lock();
		clients.remove(this);
		clientsListLock.unlock();

		stop();
		delete this;
		return;
	}

	readLength(pckHeader, pckHeader.msgLeng);
}

void Client::onReadData(char* mem, const boost::system::error_code& error)
{
	if (error)
	{
		clientsListLock.lock();
		clients.remove(this);
		clientsListLock.unlock();

		stop();
		delete this;
		return;
	}

	std::string strMsg;
	char szFmtBuf[64];
	SYSTEMTIME curTime;
	PckHeader* hd = (PckHeader*)mem;

	strMsg.reserve(hd->msgLeng + 128);
	GetLocalTime(&curTime);

	strMsg += szLogTypes[hd->logType];
	size_t leng = sprintf(szFmtBuf, "(%04d-%04d-%04d %02d:%02d:%02d)", curTime.wYear, curTime.wMonth, curTime.wDay, curTime.wHour, curTime.wMinute, curTime.wSecond);
	strMsg.append(szFmtBuf, leng);
	strMsg.append((char*)(hd + 1), hd->msgLeng);

	sessionsListLock.lock();
	for (WSessions::iterator el = sessions.begin(); el != sessions.end(); ++ el)
	{
		(*el)->write(strMsg);
	}
	sessionsListLock.unlock();

	start();
}