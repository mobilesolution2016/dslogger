#pragma once

#include "configs.h"
#include "dbuffer.h"

#define WM_ICON_BALLOON		WM_USER + 1

extern HWND	hMainWnd;

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
	Client();
	~Client();

	char* getPacketBuf(uint32_t needSize);
	void backPacketBuf(char* mem);

	void start();
	void readLength(PckHeader& hd, size_t leng);
	void stop();	

	void onDisconnected(std::string* pstrErrMsg = 0);
	void onReadHeader(const boost::system::error_code& error);
	void onReadBody(char* mem, const boost::system::error_code& error);
	void onReadStrings(size_t bytes, const boost::system::error_code& error);

public:
	static void acceptOne();

public:
	tcpsocket			sock;
	union {
		PckHeader			pckHeader;	
		char				hdrBuffer[1024];
	};	
	PacketBuf			*firstBuf, *lastBuf;
	ProjectConfig		*pProject;
	DBuffer				recvBuffer;
	uint32_t			globalId;
	boost::mutex		bufLock;
	std::string			strClientName;
};


//////////////////////////////////////////////////////////////////////////
class Session : public WS::Session
{
public:
	void on_connect();
	void on_disconnect();
	void on_message(const string& msg);
};

//////////////////////////////////////////////////////////////////////////
typedef std::list<Client*> AllClients;
typedef std::list<Session*> WSessions;

extern boost::mutex clientsListLock;
extern boost::mutex sessionsListLock;

extern AllClients	clients;
extern WSessions	sessions;

extern void startSocketServer(const char* listenIp = 0);
extern void stopSocketServer();
extern void secondsCheckServer();