#pragma once
#include "framework.h"
#include "windows.h"
#include <Winsock2.h>
#include <mswsock.h>    //微软扩展的类库
#include "stdio.h"
#include <map>
#include <mutex>
#include <stack>

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Iocp_API __declspec(dllexport)
#else
#define Iocp_API __declspec(dllimport)
#endif // COMMON_LIB

#define DATA_BUFSIZE 20480
#define READ	0
#define WRITE	1
#define ACCEPT	2
#define CONNECT 3
#define WRITEEX 4

#define RECV		0
#define SEND		1
#define SEND_CLOSE	2
#define CLOSE		3

#define IOCP_TCP 0
#define IOCP_UDP 1

class BaseFactory;
class BaseProtocol;

//扩展的输入输出结构
typedef struct _IOCP_BUFF
{
	OVERLAPPED	overlapped;
	WSABUF		databuf;
	CHAR		recv_buffer[DATA_BUFSIZE];
	CHAR*		send_buffer;
	DWORD		buffer_len;
	BYTE		type;
	SOCKET		sock;
	bool		close;
}IOCP_BUFF;

//完成键
typedef struct _IOCP_SOCKET
{
	SOCKET				sock;
	uint8_t				iotype;
	CHAR				IP[16];
	int32_t				PORT;
	SOCKADDR_IN			SAddr;
	BaseFactory*		factory;
	std::mutex*			userlock;
	BaseProtocol**		user;
	IOCP_BUFF*			IocpBuff;
	time_t				timeout;
}IOCP_SOCKET, *HSOCKET;	//完成端口句柄

class Reactor 
{
public:
	Reactor() {};
	virtual ~Reactor();

public:
	HANDLE	ComPort = NULL;
	bool	Run = true;
	DWORD	CPU_COUNT = 1;

	LPFN_ACCEPTEX				lpfnAcceptEx = NULL;					 //AcceptEx函数指针
	LPFN_GETACCEPTEXSOCKADDRS	lpfnGetAcceptExSockaddrs = NULL;  //加载GetAcceptExSockaddrs函数指针

	std::map<short, BaseFactory*>	FactoryAll;
	std::stack<IOCP_SOCKET*>		HsocketPool;
	std::mutex						HsocketPoolLock;
	std::stack<IOCP_BUFF*>			HbuffPool;
	std::mutex						HbuffPoolLock;
};

class BaseProtocol
{
public:
	BaseProtocol() {};
	virtual ~BaseProtocol() {delete this->_protolock;};

public:
	BaseFactory*	_factory = NULL;
	BaseProtocol*	_self = NULL;
	std::mutex*		_protolock = new std::mutex;
	uint8_t			_sockCount = 0;

public:
	virtual void ConnectionMade(HSOCKET hsock, const char *ip, int port) = 0;
	virtual void ConnectionFailed(HSOCKET, const char *ip, int port) = 0;
	virtual void ConnectionClosed(HSOCKET hsock, const char *ip, int port) = 0;
	virtual void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len) = 0;
};

class BaseFactory
{
public:
	BaseFactory() {};
	virtual ~BaseFactory() {};

public:
	Reactor*		reactor = NULL;
	uint32_t		ServerPort = 0;
	SOCKET			sListen = NULL;
	virtual bool	FactoryInit() = 0;
	virtual bool	FactoryLoop() = 0;
	virtual bool	FactoryClose() = 0;
	virtual BaseProtocol* CreateProtocol() = 0;
	virtual bool	DeleteProtocol(BaseProtocol* proto) = 0;
};

#ifdef __cplusplus
extern "C"
{
#endif

Iocp_API int		IOCPServerStart(Reactor* reactor);
Iocp_API void		IOCPServerStop(Reactor* reactor);
Iocp_API int		IOCPFactoryRun(BaseFactory* fc);
Iocp_API int		IOCPFactoryStop(BaseFactory* fc);
Iocp_API HSOCKET	IOCPConnectEx(const char* ip, int port, BaseProtocol* proto, uint8_t iotype);
Iocp_API bool		IOCPPostSendEx(IOCP_SOCKET* IocpSock, const char* data, int len);
Iocp_API bool		IOCPCloseHsocket(IOCP_SOCKET* IocpSock);
Iocp_API bool		IOCPDestroyProto(BaseProtocol* proto);

#ifdef __cplusplus
}
#endif