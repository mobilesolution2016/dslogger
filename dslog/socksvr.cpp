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

std::string strListenIp = "127.0.0.1";

class Client
{
public:
	struct PacketBuf
	{
		uint32_t	used;
		uint32_t	total;
		uint32_t	cc;
		PacketBuf	*next;
	};
	typedef boost::asio::ip::tcp::socket tcpsocket;		

	const uint32_t PACKETBUFALLOCSIZE = 4096;
	const uint32_t PACKETBUFSIZE = 4096 - sizeof(PacketBuf);
	const uint32_t PACKETFREEMIN = 128;

public:
	Client() 
		: sock(ioService)
	{
		firstBuf = lastBuf = 0;
	}
	~Client() 
	{
		PacketBuf* n = firstBuf, *nn;
		while (n)
		{
			nn = n->next;
			free(n);
			n = nn;
		}
	}

	char* getPacketBuf(uint32_t needSize)
	{
		char* r = 0;
		PacketBuf* n = 0;

		bufLock.lock();
		n = firstBuf;
		while(n)
		{
			if (n->used + needSize + sizeof(PckHeader) <= n->total)
			{
				r = (char*)(n + 1);
				r += lastBuf->used;	
				break;
			}
			n = n->next;
		}
		
		if (!r)
		{
			uint32_t t = std::max((uint32_t)(needSize + sizeof(PckHeader) + sizeof(PacketBuf)), PACKETBUFALLOCSIZE);

			n = (PacketBuf*)malloc(t);
			n->total = t - sizeof(PacketBuf);
			n->used = 0;
			n->next = 0;
			n->cc = 0;

			r = (char*)(n + 1);
			if (lastBuf)
				lastBuf->next = n;
			else
				firstBuf = n;
			lastBuf = n;
		}

		n->cc ++;
		n->used += needSize + sizeof(PckHeader);
		bufLock.unlock();

		return r;
	}
	void backPacketBuf(char* mem)
	{
		bufLock.lock();
		PacketBuf* n = firstBuf, *prev = 0;
		while (n)
		{
			if (mem >= (char*)n && mem <= (char*)n + n->total)
			{
				if (-- n->cc && n->total - n->used < PACKETFREEMIN)
				{
					if (prev)
						prev->next = n->next;
					else
						firstBuf = n->next;
					if (!firstBuf)
						lastBuf = 0;
					bufLock.unlock();
					free(n);
				}
				else
					bufLock.unlock();
				break;
			}

			prev = n;
			n = n->next;
		}
	}

	void start()
	{
		boost::asio::async_read(sock,
			boost::asio::buffer((char*)&pckHeader, sizeof(PckHeader)),
			boost::bind(&Client::onReadHeader, this, boost::asio::placeholders::error)
		);
	}

	void readLength(PckHeader& hd, size_t leng)
	{
		if (leng)
		{
			PckHeader* dst = (PckHeader*)getPacketBuf(leng);
			*dst = hd;

			boost::asio::async_read(sock,
				boost::asio::buffer((char*)(dst + 1), leng),
				boost::bind(&Client::onReadData, this, (char*)dst, boost::asio::placeholders::error)
			);
		}
		else
		{
			start();
		}
	}

	void stop()
	{
		boost::system::error_code ec;
		sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		sock.close(ec);
	}	
	static void acceptOne();

	void onDisconnected(std::string* pstrErrMsg = 0);
	void onReadHeader(const boost::system::error_code& error);
	void onReadData(char* mem, const boost::system::error_code& error);

public:
	tcpsocket			sock;
	PckHeader			pckHeader;	
	PacketBuf			*firstBuf, *lastBuf;
	boost::mutex		bufLock;
	std::string			strClientName;
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

boost::thread *pServerThread = NULL, *pWebSocketThread = NULL;

void serverThreadProc()
{
	boost::asio::ip::tcp::resolver::query query(
		strListenIp,
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
#ifdef _WINDOWS_
			MessageBoxA(GetActiveWindow(), ex.what(), "Socket Catched Exception", MB_OK | MB_ICONERROR);
#else
			printf("Socket Catched Exception: %s\n", e.what());
#endif
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
}

void Session::on_connect()
{
	sessionsListLock.lock();
	sessions.push_back(this);
	sessionsListLock.unlock();

#ifdef _WINDOWS_
	std::wstring* msg = new std::wstring(_T("日志察看者已连接"));
	std::wstring* title = new std::wstring(_T("DSLogger 提醒"));

	PostMessage(hMainWnd, WM_ICON_BALLOON, (WPARAM)msg, (LPARAM)title);
#endif
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

void threadWebSocketProc()
{
	boost::asio::ip::tcp::resolver::query query(
		strListenIp,
		boost::lexical_cast< std::string >(9080)
	);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

	auto server = WS::Server::create<>(wsService, endpoint);
	server->handle_resource<Session>("/dslogger");

	bWSRunning = true;
	while (bEventLooping)
	{
		try
		{
			wsService.run();
		}
		catch (std::exception& e)
		{
#ifdef _WINDOWS_
			MessageBoxA(GetActiveWindow(), e.what(), "WS Catched Exception", MB_OK | MB_ICONERROR);
#else
			printf("WS Catched Exception: %s\n", e.what());
#endif
		}
	}
	bWSRunning = false;
}

//////////////////////////////////////////////////////////////////////////
void startSocketServer(const char* listenIp)
{
	if (listenIp && listenIp[0])
		strListenIp = listenIp;

	pServerThread = new boost::thread(&serverThreadProc);
	pWebSocketThread = new boost::thread(&threadWebSocketProc);

	pServerThread->detach();
	pWebSocketThread->detach();
}

void stopSocketServer()
{
	bEventLooping = false;
	ioService.stop();
	wsService.stop();

	pServerThread->interrupt();
	pWebSocketThread->interrupt();

	while(bMainRunning || bWSRunning)
		Sleep(50);

	delete pServerThread;
	delete pWebSocketThread;
}

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

	// tip
	//char buf[256];
	//size_t leng = sprintf(buf, "日志产生者<%s:%u>已连接", c->sock.remote_endpoint().address().to_string().c_str(), c->sock.remote_endpoint().port());
	//int wcch = MultiByteToWideChar(CP_ACP, 0, buf, leng, 0, 0);

	//std::wstring* msg = new std::wstring();
	//std::wstring* title = new std::wstring(_T("DSLogger 提醒"));

	//msg->resize(wcch);
	//MultiByteToWideChar(CP_ACP, 0, buf, leng, const_cast<wchar_t*>(msg->data()), wcch);
	//PostMessage(hMainWnd, WM_ICON_BALLOON, (WPARAM)msg, (LPARAM)title);

	Client::acceptOne();
}

//////////////////////////////////////////////////////////////////////////
void Client::acceptOne()
{
	Client* c = new Client();
	acceptor.async_accept(c->sock, boost::bind(&onAccept, c, boost::asio::placeholders::error));
}

void Client::onDisconnected(std::string* pstrErrMsg)
{
	// tip
	//char buf[256];
	//size_t leng = sprintf(buf, "日志产生者<%s:%u>已断开连接", sock.remote_endpoint().address().to_string().c_str(), sock.remote_endpoint().port());

	//int wcch = MultiByteToWideChar(CP_ACP, 0, buf, leng, 0, 0);
	//if (pstrErrMsg)
	//	wcch += MultiByteToWideChar(CP_ACP, 0, pstrErrMsg->c_str(), pstrErrMsg->length(), 0, 0) + 1;

	//std::wstring* msg = new std::wstring();
	//std::wstring* title = new std::wstring(_T("DSLogger 提醒"));

	//msg->resize(wcch);
	//wchar_t* dst = const_cast<wchar_t*>(msg->data());

	//dst += MultiByteToWideChar(CP_ACP, 0, buf, leng, dst, wcch);
	//if (pstrErrMsg)
	//{
	//	*dst ++ = ':';
	//	MultiByteToWideChar(CP_ACP, 0, pstrErrMsg->c_str(), pstrErrMsg->length(), dst, wcch);
	//}

	//PostMessage(hMainWnd, WM_ICON_BALLOON, (WPARAM)msg, (LPARAM)title);

	// 
	clientsListLock.lock();
	AllClients::iterator ite = std::find(clients.begin(), clients.end(), this);
	if (ite == clients.end())
	{
		clientsListLock.unlock();
		return ;
	}
	clients.erase(ite);
	clientsListLock.unlock();

	stop();

#if 0
	std::string strMsg;
	formatNameAndTime(strMsg, "*closed");
	strMsg += strClientName;
	strMsg += ":gone away";

	sessionsListLock.lock();
	for (WSessions::iterator el = sessions.begin(); el != sessions.end(); ++ el)
		(*el)->write(strMsg);
	sessionsListLock.unlock();
#endif

	delete this;
}

void Client::onReadHeader(const boost::system::error_code& error)
{
	if (error)
	{		
		onDisconnected(&error.message());
		return;
	}

	if (pckHeader.msgLeng == 0)
		onReadData((char*)&pckHeader, error);
	else
		readLength(pckHeader, pckHeader.msgLeng);
}

void Client::onReadData(char* mem, const boost::system::error_code& error)
{
	if (error)
	{
		onDisconnected(&error.message());
		return;
	}

	std::string strMsg;
	PckHeader* hd = (PckHeader*)mem;

	if (hd->logType >= kLogCommandStart)
	{
		switch (hd->logType)
		{
		case kCmdSetName:
			strClientName.append((char*)(hd + 1), hd->msgLeng);
			break;
		case kCmdClear:
			strMsg = "*clear";
			break;
		}
	}
	else
	{
		strMsg.reserve(hd->msgLeng + 160);

		// type
		strMsg += szLogTypes[hd->logType];

		// time
		formatNameAndTime(strMsg, NULL);

		// name
		strMsg += strClientName;
		strMsg += ':';

		// msg
		strMsg.append((char*)(hd + 1), hd->msgLeng);
	}

	// send
	if (strMsg.length())
	{
		sessionsListLock.lock();
		for (WSessions::iterator el = sessions.begin(); el != sessions.end(); ++ el)
			(*el)->write(strMsg);
		sessionsListLock.unlock();
	}

	if (hd->msgLeng > 0)
		backPacketBuf(mem);

	start();
}