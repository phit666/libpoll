#pragma once
/**
	@version libpoll 1.x.x
*/
#define _LIBPOLL_MAJOR_VER_ 0x01
#define _LIBPOLL_MINOR_VER_ 0x00
#define _LIBPOLL_PATCH_VER_ 0x00

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
#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NO_WARN_MBCS_MFC_DEPRECATION
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#pragma warning(disable : 4244)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267) // SIZE_T to INT
#pragma warning(disable : 4091) // ENUM

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <map>
#include <vector>
#include <mswsock.h>
#include <thread>
#include <mutex>
#include "libpoll-config.h"



#ifndef POL_MAX_IO_BUFFER_SIZE
#define POL_MAX_IO_BUFFER_SIZE		8192
#endif

#define POL_MAX_CONT_REALLOC_REQ		100			

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
	eINFO = 1,
	eWARNING = 2,
	eERROR = 4,
	eDEBUG = 8,
	eTEST = 16,
	eALL = eINFO | eWARNING | eERROR | eDEBUG,
	eSILENT = 32
};

/**
	Enum class type of Event callback.
*/
enum class epolstatus
{
	eCONNECTED,
	eCLOSED,
	eSOCKERROR,
	eNOEVENCB
};

typedef void pol;
typedef void polbase;

typedef void (*polloghandler)(epollogtype logtype, LPCSTR message);

/**
	muevent Read callback typedef.
*/
typedef bool (*polreadcb)(polbase* base, int event_id, LPVOID argument);

/**
	muevent Event callback typedef.
*/
typedef void (*poleventcb)(polbase* base, int event_id, epolstatus eventype, LPVOID argument);

/**
	muevent Accept callback typedef.
*/
typedef bool (*polacceptcb)(polbase* base, int event_id, LPVOID argument);


typedef struct _POL_PIO_CTX
{
	_POL_PIO_CTX()
	{
		clear();
	}
	void clear()
	{
		pBuffer = NULL;
		pBufferLen = 0;
		pReallocCounts = 0;
		nSecondOfs = 0;
		nTotalBytes = 0;
		nSentBytes = 0;
		nWaitIO = 0;
	}
	void clear2()
	{
		pReallocCounts = 0;
		nSecondOfs = 0;
		nTotalBytes = 0;
		nSentBytes = 0;
		nWaitIO = 0;
	}
	CHAR Buffer[POL_MAX_IO_BUFFER_SIZE];
	CHAR* pBuffer;
	int pBufferLen;
	int pReallocCounts;
	int nSecondOfs;
	int nTotalBytes;
	int nSentBytes;
	int nWaitIO;
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
		eventcb = NULL;
		m_conipaddr = 0;
		m_conport = 0;
		m_lastconnecttick = 0;
		IOContext[0].clear();
		IOContext[1].clear();
		arg = NULL;
		arg2 = NULL;
		_this = NULL;
	}

	void clear2()
	{
		m_socket = INVALID_SOCKET;
		m_index = -1;
		m_eventid = -1;
		m_type = -1;
		m_initbuflen = 0;
		m_connected = false;
		m_pendingsend = false;
		memset(m_ipaddr, 0, sizeof(m_ipaddr));
		recvcb = NULL;
		eventcb = NULL;
		m_conipaddr = 0;
		m_conport = 0;
		m_lastconnecttick = 0;
		IOContext[0].clear2();
		IOContext[1].clear2();
		arg = NULL;
		arg2 = NULL;
		_this = NULL;
	}


	intptr_t m_index;
	SOCKET m_socket;
	int m_eventid;
	_POL_PIO_CTX IOContext[2];
	BYTE m_type;
	char m_ipaddr[16];
	bool m_connected;
	bool m_pendingsend;
	DWORD m_conipaddr;
	DWORD m_conport;
	DWORD m_lastconnecttick;
	DWORD m_initbuflen;
	polreadcb recvcb;
	poleventcb eventcb;
	LPVOID arg;
	LPVOID arg2;
	LPVOID _this;
} POL_PS_CTX, *LPPOL_PS_CTX;



class clibpoll
{
public:
	clibpoll();
	~clibpoll();
	void init(polloghandler loghandler=NULL, DWORD logverboseflags = -1,
		size_t initclt2ndbufsize = NULL, size_t initsvr2ndbufsize = NULL);
	void dispatch();
	void dispatchbreak();
	void listen(int listenport, polacceptcb acceptcb, LPVOID arg, char* listenip=NULL);
	void setacceptcbargument(LPVOID arg);
	void setreadeventcbargument(int event_id, LPVOID arg);
	void setconnectcb(int event_id, polreadcb readcb, poleventcb eventcb, LPVOID arg = NULL);
	int makeconnect(const char* ipaddr, WORD port, intptr_t index, LPPOL_PS_CTX ctx=NULL);
	bool connect(int event_id, char* initData, int initLen);
	bool sendbuffer(int event_id, LPBYTE lpMsg, DWORD dwSize);
	size_t readbuffer(int event_id, char* buffer, size_t buffersize);
	void closesocket(int event_id);
	SOCKET getsocket(int event_id);
	void getipaddr(int event_id, char* ipaddr);
	void setindex(int event_id, intptr_t index);
	intptr_t getindex(int event_id);
	bool iseventidvalid(int event_id);
	void addlog(epollogtype type, const char* msg, ...);

	bool setctx(int event_id, LPPOL_PS_CTX ctx);
	void enablecustomctx() { this->m_customctx = true; }

	LPPOL_PS_CTX getctx(int event_id);
	void deletectx(LPPOL_PS_CTX ctx);

	std::recursive_mutex m;

private:

	WSAPOLLFD* m_pollfdarr;
	int m_pollfdcounts;
	intptr_t m_tindex;
	polloghandler fnc_loghandler;

	SOCKET createsocket(bool nonblocking=false);
	int createlistensocket(WORD port);

	bool handleaccept();
	bool handleconnect(LPPOL_PS_CTX ctx);
	bool handlereceive(LPPOL_PS_CTX ctx, DWORD dwIoSize);
	bool handlesend(LPPOL_PS_CTX ctx);
	void close(int event_id, epolstatus flag = epolstatus::eCLOSED);
	void clear();
	void remove(int event_id);

	void makepollfdarr();
	bool m_rebuildpollfdarr;

	polacceptcb m_acceptcb;
	LPVOID m_acceptarg;

	SOCKET m_listensocket;

	LPPOL_PS_CTX m_acceptctx;
	int m_accepteventid;

	size_t m_initcltextbuffsize;
	size_t m_initsvrextbuffsize;

	std::map<int, LPPOL_PS_CTX>m_polmaps;

	DWORD m_logverboseflags;
	int m_workers;
	
	DWORD m_listenip;
	WORD m_listenport;
	int m_eventid;

	int m_activeworkers;

	bool m_customctx;
	bool m_loopbreak;
};



