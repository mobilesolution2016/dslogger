#include "stdafx.h"
#include "socksvr.h"

static bool bEventLooping = false;
static bool bMainRunning = false, bWSRunning = false;
static time_t lastSockServerCheckedTime = 0;
static time_t lastWSServerCheckedTime = 0;

boost::asio::io_service ioService;
boost::asio::io_service wsService;
boost::asio::io_service::work work(ioService);
boost::asio::ip::tcp::acceptor acceptor(ioService);
boost::asio::ip::tcp::resolver resolver(ioService);

boost::mutex clientsListLock;
boost::mutex sessionsListLock;

std::string strListenIp = "127.0.0.1";
uint32_t gGlobalId = 0;

//////////////////////////////////////////////////////////////////////////
AllClients clients;
WSessions	sessions;

boost::thread *pServerThread = NULL, *pWebSocketThread = NULL;

//////////////////////////////////////////////////////////////////////////
void serverThreadProc()
{
	boost::asio::ip::address addr;
	addr.from_string(strListenIp);
	boost::asio::ip::tcp::endpoint endpoint(addr, short(9880));

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

void threadWebSocketProc()
{
	boost::asio::ip::address addr;
	addr.from_string(strListenIp);
	boost::asio::ip::tcp::endpoint endpoint(addr, short(9980));

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

void secondsCheckServer()
{
	time_t t = time(NULL);
	if (lastSockServerCheckedTime == 0)
		lastSockServerCheckedTime = t;
	if (lastWSServerCheckedTime == 0)
		lastWSServerCheckedTime = t;
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
Client::Client() 
	: sock(ioService)
	, globalId(0)
	, pProject(NULL)
{
	firstBuf = lastBuf = 0;
}
Client::~Client() 
{
	PacketBuf* n = firstBuf, *nn;
	while (n)
	{
		nn = n->next;
		free(n);
		n = nn;
	}
}

char* Client::getPacketBuf(uint32_t needSize)
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
void Client::backPacketBuf(char* mem)
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

void Client::start()
{
	if (pProject->kSocketMode == ProjectConfig::kSocketStandPackets)
	{
		boost::asio::async_read(sock,
			boost::asio::buffer((char*)&pckHeader, sizeof(PckHeader)),
			boost::bind(&Client::onReadHeader, this, boost::asio::placeholders::error)
		);
	}
	else if (pProject->kSocketMode == ProjectConfig::kSocketStrings)
	{
		sock.async_read_some(
			boost::asio::buffer(hdrBuffer, sizeof(hdrBuffer)),
			boost::bind(&Client::onReadStrings, this, boost::asio::placeholders::bytes_transferred, boost::asio::placeholders::error)
		);		
	}
}

void Client::readLength(PckHeader& hd, size_t leng)
{
	if (leng)
	{
		PckHeader* dst = (PckHeader*)getPacketBuf(leng);
		*dst = hd;

		boost::asio::async_read(sock,
			boost::asio::buffer((char*)(dst + 1), leng),
			boost::bind(&Client::onReadBody, this, (char*)dst, boost::asio::placeholders::error)
		);
	}
	else
	{
		start();
	}
}

void Client::stop()
{
	boost::system::error_code ec;
	sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
	sock.close(ec);
}	

void Client::acceptOne()
{
	Client* c = new Client();
	c->globalId = ++ gGlobalId;
	acceptor.async_accept(c->sock, boost::bind(&onAccept, c, boost::asio::placeholders::error));
}

void Client::onDisconnected(std::string* pstrErrMsg)
{
	// 断开
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

	// format
	std::string strMsg;
	LogFormat::LogInfo info = { 0 };

	info.pszClientName = strClientName.c_str();
	info.nameLeng = strClientName.length();
	if (pstrErrMsg)
	{
		info.pData = pstrErrMsg->c_str();
		info.size = pstrErrMsg->length();
	}

	pProject->pFormat->onFormatData(strMsg, info);

	// dump
	pProject->pDump->onDumpData(strMsg);

	delete this;
}

void Client::onReadHeader(const boost::system::error_code& error)
{
	if (error)
	{
		onDisconnected(&error.message());
		return;
	}

	assert(pProject->kSocketMode == ProjectConfig::kSocketStandPackets);
	if (pckHeader.msgLeng == 0)
		onReadBody((char*)&pckHeader, error);
	else
		readLength(pckHeader, pckHeader.msgLeng);
}

void Client::onReadBody(char* mem, const boost::system::error_code& error)
{
	if (error)
	{
		onDisconnected(&error.message());
		return;
	}

	std::string strMsg;
	LogFormat::LogInfo info;
	PckHeader* hd = (PckHeader*)mem;

	info.pszClientName = strClientName.c_str();
	info.nameLeng = strClientName.length();
	info.pData = mem;
	info.size = hd->msgLeng + sizeof(PckHeader);	

	if (hd->logType >= kLogCommandStart)
	{
		switch (hd->logType)
		{
		case kCmdSetName:
			strClientName.append((char*)(hd + 1), hd->msgLeng);
			break;
		case kCmdClear:
			info.kEvent = LogFormat::kEventClearAll;
			pProject->pFormat->onFormatData(strMsg, info);
			break;
		}
	}
	else
	{
		info.kEvent = LogFormat::kEventNormal;
		pProject->pFormat->onFormatData(strMsg, info);
	}	

	// dump
	pProject->pDump->onDumpData(strMsg);

	if (hd->msgLeng > 0)
		backPacketBuf(mem);

	start();
}

void Client::onReadStrings(size_t bytes, const boost::system::error_code& error)
{
	assert(pProject->kSocketMode == ProjectConfig::kSocketStrings);

	recvBuffer.append(hdrBuffer, bytes);
	start();


}

//////////////////////////////////////////////////////////////////////////
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