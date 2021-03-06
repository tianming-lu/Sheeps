﻿/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _TASK_MANAGER_H_
#define _TASK_MANAGER_H_
#include <vector>
#include <list>
#include <map>
#include <mutex>
#include "Reactor.h"

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl
#if defined STRESS_EXPORTS
#define Task_API __declspec(dllexport)
#else
#define Task_API __declspec(dllimport)
#endif
#else
#define STDCALL
#define CDECL
#define Task_API
#endif // __WINDOWS__

extern int clogId;
extern bool TaskManagerRuning;
#ifndef _LOG_H_
enum loglevel { LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE };
#endif

enum {
	NOTHING = 0x00,	//复合动作,什么都不做
	DELAY = 0x01,	//复合动作,延迟消息并暂停
	//发送录制协议时返回确认发送数据包或者丢弃数据包，默认是DOSEND
	DOSEND = 0x02,
	GIVEUP = 0x04,
	//确定用户播放状态
	NORMAL = 0x20,
	PAUSE = 0x40,
	FAST = 0x80
};

enum{
	TYPE_CONNECT = 0,
	TYPE_SEND,
	TYPE_CLOSE,
	TYPE_REINIT = 99
};

enum {
	PLAY_NORMAL = 0,
	PLAY_PAUSE,
	PLAY_FAST
};

enum {
	TASK_RUNNING = 0,
	TASK_CLEANING,
};

typedef struct {
	uint32_t	index;
	uint64_t	start_record;		//微秒
	uint64_t	start_real;			//微秒
	uint64_t	last;
}MSGPointer, *HMSGPOINTER;

class ReplayProtocol;

typedef struct {
	uint8_t		type;
	char		ip[16];
	union {
		uint16_t port;
		uint16_t isloop;
	};
	uint64_t	recordtime;		//微秒
	char*		content;
	uint32_t	contentlen;
	bool		udp;
}t_cache_message, *HMESSAGE;

typedef struct _UserEvent {
#ifdef __WINDOWS__
	HANDLE			timer;
	ReplayProtocol* user;
	//HTASKCFG		task;
#else
	HSOCKET			timer;
	ReplayProtocol* user;
#endif // __WINDOWS__
}UserEvent, * HUserEvent;

typedef struct {
	int			logfd;
	uint8_t		status;    //默认TASK_RUNNING

	uint8_t		taskID;
	uint8_t		projectID;
	uint8_t		machineID;
	uint16_t	userCount;
	bool		ignoreErr;
	char		parms[128];

	std::vector<t_cache_message*>*	messageList;    //任务消息缓存
	bool							stopMessageCache;
#ifdef __WINDOWS__
	HANDLE hTimerQueue;
#endif

	uint16_t	userNumber;
	std::list<HUserEvent>* userAll;
	std::list<HUserEvent>*	userDes;			//运行结束用户列表
	std::mutex*			userDesLock;

}t_task_config, *HTASKCFG;

typedef ReplayProtocol* (*CREATE_CALLBACK)();
typedef void(*DESTORY_CALLBACK)(ReplayProtocol*);
typedef int(*TASK_CALLBACK)(HTASKCFG);
typedef int(*UNPACK)(const char*, int, int, const char*, int);
typedef struct dllAPI
{
	CREATE_CALLBACK   create;
	DESTORY_CALLBACK  destory;
	TASK_CALLBACK		taskstart;
	TASK_CALLBACK		taskstop;
	TASK_CALLBACK		unpack;
}t_replay_dll;

class ReplayProtocol :
	public BaseProtocol
{
public:
	ReplayProtocol() {};
	virtual ~ReplayProtocol() {};

public:
#ifndef __WINDOWS__
	HUserEvent		_timerevent;
#endif
	HTASKCFG	Task = NULL;
	int			UserNumber = 0;
	bool		SelfDead = false;
	uint8_t		PlayMode = PLAY_NORMAL;
	MSGPointer	MsgPointer = { 0x0 };
	char		LastError[128] = {0x0};

public:
	virtual void EventInit() = 0;
	virtual void EventConnectOpen(const char* ip, int port, bool udp) = 0;
	virtual void EventConnectClose(const char* ip, int port, bool udp) = 0;
	virtual void EventConnectSend(const char* ip, int port, const char* content, int clen, bool udp) = 0;
	virtual void EventTimeOut() = 0;
	virtual void EventReset() = 0;
	virtual void EventDestroy() = 0;
	virtual void ConnectionMade(HSOCKET hsock) = 0;
	virtual void ConnectionFailed(HSOCKET hsock, int err) = 0;
	virtual void ConnectionClosed(HSOCKET hsock, int err) = 0;
	virtual void ConnectionRecved(HSOCKET hsock, const char* data, int len) = 0;
	
};

//受控端逻辑API

#ifdef __cplusplus
extern "C"
{
#endif

int		__STDCALL	create_new_task(uint8_t taskid, uint8_t projectid, uint8_t machineid, bool ignorerr, int userconut, int loglevel, const char* parms, BaseFactory* factory);
bool	__STDCALL	insert_message_by_taskId(uint8_t taskID, uint8_t type, char* ip, uint32_t port, char* content, uint64_t timestamp, uint32_t microsecond, bool udp);
bool	__STDCALL	stop_task_by_id(uint8_t taskID);
int		__STDCALL	task_add_user_by_taskid(uint8_t taskid, int userCount, BaseFactory* factory);
void	__STDCALL	set_task_log_level(uint8_t level, uint8_t taskID);

//项目业务逻辑API
Task_API void	__STDCALL	TaskManagerRun(int projectid, CREATE_CALLBACK create, DESTORY_CALLBACK destory, TASK_CALLBACK taskstart, TASK_CALLBACK taskstop, bool server);
Task_API bool	__CDECL__	TaskUserDead(ReplayProtocol* proto, const char* fmt, ...);
Task_API bool	__STDCALL	TaskUserSocketClose(HSOCKET hsock);
Task_API void	__CDECL__	TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...);
Task_API void	__CDECL__	TaskLog(HTASKCFG task, uint8_t level, const char* fmt, ...);
#define		TaskUserSocketConnet(proto, ip, port, iotype)	HsocketConnect(proto, ip, port, iotype)
#define		TaskUserSocketSend(hsock, data, len)			HsocketSend(hsock, data, len)
#define		TaskUserSocketSkipBuf(hsock, len)				HsocketSkipBuf(hsock, len)

#ifdef __cplusplus
}
#endif

#endif // !_TASK_MANAGER_H_