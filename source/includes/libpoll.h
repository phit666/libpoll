#pragma once
/**
	@file libpoll.h
*/

/**
	@version libpoll 1.x.x
*/
#define _LIBPOLL_MAJOR_VER_ 0x01
#define _LIBPOLL_MINOR_VER_ 0x09
#define _LIBPOLL_PATCH_VER_ 0x08

/*
 * MIT License
 *
 * Copyright (c) 2022 phit666
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifdef _WIN32
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <Ws2tcpip.h>
typedef SOCKET sock_t;
typedef WSAPOLLFD _pollfd;
#define SOCKERR WSAGetLastError()
#define _poll WSAPoll
#define SOCKCONNBLOCK WSAEWOULDBLOCK
#define SOCKCONNREFUSED WSAECONNREFUSED
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#define epoll_close close
#define closesocket close
#define ioctlsocket ioctl
#define SOCKCONNBLOCK EINPROGRESS
#define SOCKCONNREFUSED ECONNREFUSED
#define SOCKERR errno
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SD_BOTH SHUT_RDWR
typedef int sock_t;
typedef int HANDLE;
uint32_t GetTickCount();
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <assert.h>
#include "libpoll-config.h"

#ifndef POL_MAX_IO_BUFFER_SIZE
#define POL_MAX_IO_BUFFER_SIZE		8192
#endif

#define POL_MAX_CONT_REALLOC_REQ		100			
#define POL_MAX_EVENTS 256

#define DISPATCH_LOOP_ONCE 1
#define DISPATCH_DONT_BLOCK 2

enum class epoliotype
{
	eRECV_IO = 1,
	eSEND_IO = 2,
	eACCEPT_IO = 4,
	eCONNECT_IO = 8,
};

/**
	Enum class type of libpolladdlog function.
*/
enum class epollogtype
{
	/*equal to NULL, no message will be shown*/
	eSILENT = 0,
	/*it will show informational messages*/
	eINFO = 1,
	/*it will show non-fatal or recoverable errors messages*/
	eWARNING = 2,
	/*it will show fatal error messages*/
	eERROR = 4,
	/*it will show messages useable for debugging*/
	eDEBUG = 8,
	/*it will show some more info not included in eALL*/
	eTEST = 16,
	/*equal to eINFO | eWARNING | eERROR | eDEBUG*/
	eALL = eINFO | eWARNING | eERROR | eDEBUG
};

/**
	Enum class type of Event callback.
*/
enum class epolstatus
{
	/*socket is connected*/
	eCONNECTED,
	/*socket recv and send are disbled*/
	eSHUTDOWN,
	/*socket error*/
	eSOCKERROR,
	/*socket is closed*/
	eCLOSED,
};

typedef void pol;
typedef void polbase;

typedef void (*polloghandler)(epollogtype logtype, const char* message);

/**
	Read callback typedef.
*/
typedef bool (*polreadcb)(int event_id, void* argument);

/**
	Write callback typedef.
*/
typedef bool (*polwritecb)(int event_id, void* argument);

/**
	Event callback typedef.
*/
typedef void (*poleventcb)(int event_id, epolstatus eventype, void* argument);

/**
	Accept callback typedef.
*/
typedef bool (*polacceptcb)(int event_id, void* argument);

typedef struct _POL_BUFFER
{
	void clear()
	{
		memset(&buffer[0], 0, len);
		drained = 0;
		sent = 0;
	}
	char* buffer;
	int len;
	int drained;
	int sent;
} POL_BUFFER, * LP_POL_BUFFER;

typedef struct _POL_PIO_CTX
{
	_POL_PIO_CTX()
	{
		clear();
	}
	void clear()
	{
		vBuffer.clear();
		nTotalBytes = 0;
		nSentBytes = 0;
	}
	char Buffer[POL_MAX_IO_BUFFER_SIZE];
	std::vector<POL_BUFFER> vBuffer;
	int nTotalBytes;
	size_t nSentBytes;
} POL_PIO_CTX, * LPPOL_PIO_CTX;

typedef struct _POL_PS_CTX
{
	_POL_PS_CTX()
	{
		clear();
	}

	void clear()
	{
		m_socket = INVALID_SOCKET;
		m_index = -1;
		m_eventid = -1;
		m_type = 0;
		m_initbuflen = 0;
		m_connected = false;
		memset(m_ipaddr, 0, sizeof(m_ipaddr));
		recvcb = NULL;
		sendcb = NULL;
		eventcb = NULL;
		m_conipaddr = 0;
		m_conport = 0;
		IOContext[0].clear();
		IOContext[1].clear();
		arg = NULL;
		arg2 = NULL;
		_this = NULL;
		m_once = false;
		m_rawread = false;
		m_rawwrite = false;
		m_events = 0;
	}

	intptr_t m_index;
	sock_t m_socket;
	int m_eventid;
	_POL_PIO_CTX IOContext[2];
	unsigned char m_type;
	char m_ipaddr[16];
	bool m_connected;
	uint32_t m_conipaddr;
	uint32_t m_conport;
	uint32_t m_initbuflen;
	polreadcb recvcb;
	polwritecb sendcb;
	poleventcb eventcb;
	void* arg;
	void* arg2;
	void* _this;
	bool m_once;
	bool m_rawread;
	bool m_rawwrite;
	uint32_t m_events;
} POL_PS_CTX, *LPPOL_PS_CTX;


class clibpoll
{
public:
	clibpoll();
	~clibpoll();
	void init(polloghandler loghandler=NULL, unsigned int logverboseflags = -1);

	void dispatch_threads(int threadcounts, uint32_t timeout = INFINITE, int maxevents = 10, unsigned int flags = 0);

	void dispatch(uint32_t timeout=INFINITE, int maxevents=10, unsigned int flags=0);
	void dispatchbreak();

	void listen(int listenport, polacceptcb acceptcb, void* arg, char* listenip=NULL);

	void setacceptcbargument(void* arg);
	void setreadeventcbargument(int event_id, void* arg);
	void setconnectcb(int event_id, polreadcb readcb, polreadcb writecb, poleventcb eventcb, void* arg = NULL);

	void setraw(int event_id, bool read, bool write);

	int makeconnect(const char* ipaddr, unsigned short int port, int flag=0);
	bool connect(int event_id, char* initData, int initLen);

	bool sendbuffer(int event_id, unsigned char* lpMsg, size_t dwSize);
	size_t readbuffer(int event_id, char* buffer, size_t buffersize);

	void closefd(int event_id);

	sock_t getsocket(int event_id);
	void getipaddr(int event_id, char* ipaddr);

	void setindex(int event_id, intptr_t index);
	intptr_t getindex(int event_id);

	size_t getsentbytes(int event_id);
	void reqwrite(int event_id);

	bool iseventidvalid(int event_id);
	bool isconnected(int event_id);
	void addlog(epollogtype type, const char* msg, ...);

	bool setctx(int event_id, LPPOL_PS_CTX ctx);

	LPPOL_PS_CTX getctx(int event_id);
	void deletectx(LPPOL_PS_CTX ctx);

	std::recursive_mutex m;

	int gettcounts() { return this->m_tcount; }

private:

	std::recursive_mutex _m;

	void dispatch_thread_run();

	int loop(uint32_t timeout, std::thread::id tid, struct epoll_event * events, int maxevents);
	void setepolevent(LPPOL_PS_CTX ctx, uint32_t cmd, uint32_t flags);

	intptr_t m_tindex;
	polloghandler fnc_loghandler;

	sock_t createsocket(bool nonblocking=false);
	int createlistensocket(unsigned short int port);

	bool handleaccept();
	bool handleconnect(LPPOL_PS_CTX ctx);
	int handlereceive(LPPOL_PS_CTX ctx);
	bool handlesend(LPPOL_PS_CTX ctx);

	void closeeventid(int event_id, epolstatus flag = epolstatus::eCLOSED);
	void clear();
	void deleventid(int eventid);

	polacceptcb m_acceptcb;
	void* m_acceptarg;

	sock_t m_listensocket;

	LPPOL_PS_CTX m_acceptctx;
	uint32_t m_accepteventid;

	std::map<int, LPPOL_PS_CTX>m_polmaps;
	bool m_polmapsupdated;

	unsigned int m_logverboseflags;
	
	unsigned int m_listenip;
	unsigned short int m_listenport;
	int m_eventid;

	int m_activeworkers;

	bool m_loopbreak;

	unsigned int m_dispatchflags;
	bool m_tstarted;
	HANDLE m_epollfd;
	int m_tcount;
	uint32_t m_ttimeout;
	int m_tmaxevents;
	unsigned int m_tflags;
	std::thread* m_t;
};



