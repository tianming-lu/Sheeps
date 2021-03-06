/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#pragma once
#include "./../Sheeps/TaskManager.h"
#include <map>

typedef struct {
	HSOCKET hsock;
}t_connection_info;

class UserProtocol :
	public ReplayProtocol
{
public:
	UserProtocol();
	~UserProtocol();

public:
	std::map<int, t_connection_info> Connection;

public:
	void EventInit();
	void EventConnectOpen(const char* ip, int port, bool udp);
	void EventConnectClose(const char* ip, int port, bool udp);
	void EventConnectSend(const char* ip, int port, const char* content, int clen, bool udp);
	void EventTimeOut();
	void EventReset();
	void EventDestroy();
	void ConnectionMade(HSOCKET hsock);
	void ConnectionFailed(HSOCKET hsock, int err);
	void ConnectionClosed(HSOCKET hsock, int err);
	void ConnectionRecved(HSOCKET hsock, const char* data, int len);
	

	HSOCKET* GetScokFromConnection(const char* ip, int port);
	bool CloseAllConnection();
};

#ifdef __cplusplus
extern "C" {
#endif

ReplayProtocol* CreateUser(void);
void	DestoryUser(ReplayProtocol* hdl);
int		TaskStart(HTASKCFG task);
int		TaskStop(HTASKCFG task);

#ifdef __cplusplus
}
#endif
