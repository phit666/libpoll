/*@file libpoll.cpp
 *
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
#include "third_party/wepoll.h"
#endif
#include "includes/libpoll.h"
#include <time.h>


clibpoll::clibpoll()
{
	this->m_listensocket = INVALID_SOCKET;
	this->m_acceptctx = NULL;
	this->m_acceptcb = NULL;
	this->m_polmaps.clear();
	this->m_acceptarg = NULL;
	this->m_listenport = 0;
	this->m_eventid = 0;
	this->m_activeworkers = 0;
	this->m_listenip = 0;
	this->m_loopbreak = false;
	this->m_dispatchflags = 0;
	this->m_tstarted = false;
	this->m_polmapsupdated = false;
	this->m_tcount = 0;
	this->m_logverboseflags = (unsigned int)epollogtype::eINFO | (unsigned int)epollogtype::eERROR;
	this->fnc_loghandler = NULL;
}

clibpoll::~clibpoll()
{
	this->addlog(epollogtype::eINFO, "%s(), class destructor called.", __func__);
	this->clear();
	this->addlog(epollogtype::eINFO, "%s(), clear done.", __func__);
	epoll_close(this->m_epollfd);
	this->addlog(epollogtype::eINFO, "%s(), epoll_close done.", __func__);
#ifdef _WIN32
	WSACleanup();
	this->addlog(epollogtype::eINFO, "%s(), WSACleanup done.", __func__);
#endif
}

void clibpoll::clear()
{
	LPPOL_PS_CTX _ctx = NULL;
	std::map <int, LPPOL_PS_CTX>::iterator iterq;
	for (iterq = this->m_polmaps.begin(); iterq != this->m_polmaps.end(); iterq++)
	{
		_ctx = iterq->second;
		if (_ctx != NULL) {
			this->addlog(epollogtype::eINFO, "%s(), closing event id %d.", __func__, _ctx->m_eventid);
			if (_ctx->m_socket != INVALID_SOCKET) {
				this->setepolevent(_ctx, EPOLL_CTL_DEL, 0);
				closesocket(_ctx->m_socket);
			}
			this->deletectx(_ctx);
		}
	}
	this->m_polmaps.clear();
}

void clibpoll::deleventid(int eventid) 
{
	LPPOL_PS_CTX _ctx = NULL;
	std::map <int, LPPOL_PS_CTX>::iterator iterq;
	iterq = this->m_polmaps.find(eventid);
	if (iterq != this->m_polmaps.end()) {
		this->m_polmaps.erase(iterq);
		this->addlog(epollogtype::eDEBUG, "%s(), event id %d deleted.", __func__, eventid);
	}
}

void clibpoll::deletectx(LPPOL_PS_CTX ctx)
{
	this->addlog(epollogtype::eDEBUG, "%s(), event id %d context deleted.", __func__, ctx->m_eventid);
	std::vector<POL_BUFFER>::iterator iter;
	iter = ctx->IOContext[1].vBuffer.begin();
	while (iter != ctx->IOContext[1].vBuffer.end()) {
		delete iter->buffer;
		iter++;
	}
	ctx->IOContext[1].vBuffer.clear();
	delete ctx;
}

bool clibpoll::iseventidvalid(int event_id)
{
	bool bret = false;
	if (event_id == -1)
		return bret;
	if (this->getctx(event_id) != NULL)
		bret = true;
	return bret;
}

bool clibpoll::isconnected(int event_id)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX ctx = this->getctx(event_id);
	if (ctx == NULL)
		return false;
	return ctx->m_connected;
}

LPPOL_PS_CTX clibpoll::getctx(int event_id)
{
	std::map<int, LPPOL_PS_CTX>::iterator Iter;
	if ((sock_t)event_id == INVALID_SOCKET)
		return NULL;
	std::lock_guard<std::recursive_mutex> lk(m);
	Iter = this->m_polmaps.find(event_id);
	if (Iter == this->m_polmaps.end()) {
		return NULL;
	}
	LPPOL_PS_CTX ctx = Iter->second;
	return ctx;
}

void clibpoll::init(polloghandler loghandler, unsigned int logverboseflags)
{
#ifdef _WIN32
	HANDLE tmphandle = NULL;
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	int enableopt = 1;
	wVersionRequested = MAKEWORD(2, 2);
#endif
	this->m_logverboseflags = (logverboseflags != (unsigned int)-1) ? logverboseflags : this->m_logverboseflags;
	this->fnc_loghandler = (loghandler != NULL) ? loghandler : this->fnc_loghandler;

	this->addlog(epollogtype::eDEBUG, "libpoll version %02d.%02d.%02d", _LIBPOLL_MAJOR_VER_, _LIBPOLL_MINOR_VER_, _LIBPOLL_PATCH_VER_);

#ifdef _WIN32
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		this->addlog(epollogtype::eERROR, "%s(), WSAStartup failed. with error: %d.", __func__, GetLastError());
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		this->addlog(epollogtype::eERROR, "%s(), WINSOCK wVersion not matched.", __func__);
		return;
	}
#endif
	this->m_listensocket = createsocket(true);

	if (setsockopt(this->m_listensocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&enableopt, sizeof(enableopt)) < 0) {
		this->addlog(epollogtype::eERROR, "%s(), setsockopt failed.", __func__);
		return;
	}

	this->m_epollfd = epoll_create1(0);

	if (this->m_epollfd == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), epoll_create1 failed.", __func__);
		return;
	}

}

int clibpoll::loop(uint32_t timeout, std::thread::id tid, epoll_event* events, int maxevents)
{
	int _maxevents = maxevents;
	if (_maxevents > POL_MAX_EVENTS)
		_maxevents = POL_MAX_EVENTS;

	if (this->m_tcount > 1) {
		std::lock_guard<std::recursive_mutex> _lk(_m);
	}

	int nfds = epoll_wait(this->m_epollfd, events, _maxevents, timeout);

	if (nfds == -1) {
		return -1;
	}

	std::lock_guard<std::recursive_mutex> lk(m);

	if (nfds > 0) {

		for (int n = 0; n < nfds; n++) {

			sock_t s = events[n].data.sock;

			if (s == INVALID_SOCKET)
				continue;

			LPPOL_PS_CTX ctx = this->getctx((int)s);

			if (ctx == NULL) {
				//this->addlog(epollogtype::eDEBUG, "%s, event id %d ctx is NULL.", __func__, (int)s);
				continue;
			}

			this->addlog(epollogtype::eTEST, "%s, event id %d events %d.", __func__, (int)s, events[n].events);

			if (events[n].events & EPOLLHUP) {
				this->addlog(epollogtype::eTEST, "%s, event id %d EPOLLHUP %d (%d %d).", __func__, ctx->m_eventid, (EPOLLHUP & events[n].events), events[n].events, EPOLLHUP);
				this->closeeventid(ctx->m_eventid, epolstatus::eCLOSED);
				continue;
			}

			if (events[n].events & EPOLLERR) {
				this->addlog(epollogtype::eTEST, "%s, event id %d EPOLLERR.", __func__, ctx->m_eventid);
				this->closeeventid(ctx->m_eventid, epolstatus::eSOCKERROR);
				continue;
			}

			if ((events[n].events & EPOLLRDNORM)) {
				if ((ctx->m_type & (unsigned char)epoliotype::eACCEPT_IO) == (unsigned char)epoliotype::eACCEPT_IO) {
					this->addlog(epollogtype::eTEST, "%s, event id %d eACCEPT_IO", __func__, ctx->m_eventid);
					this->handleaccept();
					continue;
				}
			}

			if ((ctx->m_type & (unsigned char)epoliotype::eCONNECT_IO) == (unsigned char)epoliotype::eCONNECT_IO) {
				this->addlog(epollogtype::eTEST, "%s, event id %d eCONNECT_IO, %d", __func__, ctx->m_eventid, events[n].events);
				this->handleconnect(ctx);
				continue;
			}

			if (((events[n].events & EPOLLIN) || (events[n].events & EPOLLPRI))
				&& ((ctx->m_type & (unsigned char)epoliotype::eRECV_IO) == (unsigned char)epoliotype::eRECV_IO)) {
				this->addlog(epollogtype::eTEST, "%s, event id %d EPOLLIN, tid:%d", __func__, ctx->m_eventid, tid);
				if (!this->handlereceive(ctx)) {
					this->closeeventid(ctx->m_eventid, epolstatus::eSOCKERROR);
					continue;
				}
			}

			if (events[n].events & EPOLLOUT && 
				((ctx->m_type & (unsigned char)epoliotype::eSEND_IO) == (unsigned char)epoliotype::eSEND_IO)) {
				this->addlog(epollogtype::eTEST, "%s, event id %d EPOLLOUT, tid:%d", __func__, ctx->m_eventid, tid);
				if (!this->handlesend(ctx)) {
					this->closeeventid(ctx->m_eventid, epolstatus::eSOCKERROR);
					continue;
				}
			}
		}
	}

	return 0;
}

void clibpoll::dispatch(uint32_t timeout, int maxevents, unsigned int flags)
{
	this->m_dispatchflags = flags;
	epoll_event events[POL_MAX_EVENTS] = { 0 };
	std::thread::id tid = std::this_thread::get_id();
	this->m_tcount += 1;
	this->addlog(epollogtype::eINFO, "%s, started with thread id: %u", __func__, tid);

	while (true) {
		this->loop(timeout, tid, events, maxevents);
		if (this->m_loopbreak || (flags & DISPATCH_DONT_BLOCK)) {
			break;
		}
	}
}

void clibpoll::dispatch_thread_run()
{
	this->m_dispatchflags = this->m_tflags;
	epoll_event events[POL_MAX_EVENTS] = { 0 };
	std::thread::id tid = std::this_thread::get_id();

	this->m_tcount += 1;

	this->addlog(epollogtype::eINFO, "%s, started with thread id: %u", __func__, tid);

	while (true) {
		if (this->loop(this->m_ttimeout, tid, events, this->m_tmaxevents) == -1) {
			this->addlog(epollogtype::eERROR, "%s, loop failed, thread id: %u errno: %d", __func__, tid, errno);
			break;
		}
		if (this->m_loopbreak || (this->m_tflags & DISPATCH_DONT_BLOCK)) {
			this->addlog(epollogtype::eINFO, "%s, loop break, thread id: %u", __func__, tid);
			break;
		}
	}

	this->m_tcount -= 1;
}

void clibpoll::dispatch_threads(int threadcounts, uint32_t timeout, int maxevents, unsigned int flags)
{
	this->m_t = new std::thread[threadcounts];

	this->m_ttimeout = timeout;
	this->m_tmaxevents = maxevents;
	this->m_tflags = flags;

	for (int n = 0; n < threadcounts; n++) {
		this->m_t[n] = std::thread([this]() { dispatch_thread_run(); });
		this->m_t[n].detach();
	}
}

void clibpoll::dispatchbreak()
{
	this->m_loopbreak = true;
	if (this->m_listensocket != INVALID_SOCKET) {
		closesocket(this->m_listensocket);
	}
}

void clibpoll::setacceptcbargument(void* arg)
{
	this->m_acceptarg = arg;
}

void clibpoll::setreadeventcbargument(int event_id, void* arg)
{
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return;
	}
	_ctx->arg = arg;
}

void clibpoll::listen(int listenport, polacceptcb acceptcb, void* arg, char* listenip)
{
	this->m_acceptctx = new POL_PS_CTX;
	this->m_acceptctx->clear();

	this->m_acceptcb = acceptcb;
	this->m_acceptarg = arg;
	this->m_listenport = listenport;

	if (listenip != NULL) {
		struct hostent* h = gethostbyname(listenip);
		this->m_listenip = (h != NULL) ? ntohl(*(unsigned int*)h->h_addr) : 0;
	}

	if (!this->createlistensocket(listenport)) {
		delete this->m_acceptctx;
		return;
	}

	this->m_accepteventid = (int)this->m_listensocket;

	this->m_acceptctx->m_socket = this->m_listensocket;
	this->m_acceptctx->m_eventid = this->m_accepteventid;
	this->m_acceptctx->m_type = (unsigned char)epoliotype::eACCEPT_IO;

	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(this->m_accepteventid, this->m_acceptctx));
	this->setepolevent(this->m_acceptctx, EPOLL_CTL_ADD, EPOLLRDNORM | EPOLLHUP);
	this->addlog(epollogtype::eINFO, "%s() ok, listen port is %d.", __func__, this->m_listenport);
}

void clibpoll::setepolevent(LPPOL_PS_CTX ctx, uint32_t cmd, uint32_t flags)
{
	epoll_event pollfd;
	std::lock_guard<std::recursive_mutex> lk(m);
	sock_t s = ctx->m_socket;
	if (s == INVALID_SOCKET || ctx->m_events == flags)
		return;
	this->addlog(epollogtype::eDEBUG, "%s(), socket:%d cmd:%d flag:%d.", __func__, (int)s, (int)cmd, (int)flags);
	pollfd.events = flags;
	pollfd.data.sock = s;
	if (epoll_ctl(this->m_epollfd, cmd, s, &pollfd) == -1) {
		this->addlog(epollogtype::eINFO, "%s(), failed with socket:%d errno %d.", __func__, (int)s, errno);
	}
	ctx->m_events = flags;
}

void clibpoll::setconnectcb(int event_id, polreadcb readcb, polreadcb writecb, poleventcb eventcb, void* arg)
{
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return;
	}
	_ctx->recvcb = readcb;
	_ctx->sendcb = writecb;
	_ctx->eventcb = eventcb;
	_ctx->arg = arg;
}

void clibpoll::setraw(int event_id, bool read, bool write)
{
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return;
	}
	_ctx->m_rawread = read;
	_ctx->m_rawwrite = write;
}

int clibpoll::createlistensocket(unsigned short int port)
{
	sockaddr_in InternetAddr;
	int nRet, on = 1;

	if (port == 0) {
		return 1;
	}

	if (this->m_listensocket == SOCKET_ERROR)
	{
		this->addlog(epollogtype::eERROR, "%s(), WSASocket() failed with error %d.", __func__, SOCKERR);
		return 0;
	}
	else
	{
		InternetAddr.sin_family = AF_INET;
#ifdef _WIN32
		InternetAddr.sin_addr.S_un.S_addr = htonl(this->m_listenip);
#else
		InternetAddr.sin_addr.s_addr = htonl(this->m_listenip);
#endif
		InternetAddr.sin_port = htons(port);

		if (setsockopt(this->m_listensocket, SOL_SOCKET, SO_REUSEADDR,
			(char*)&on, sizeof(on)) == SOCKET_ERROR) {
			this->addlog(epollogtype::eERROR, "%s(), setsockopt() failed with error %d.", __func__, SOCKERR);
			return 0;
		}

		nRet = ::bind(this->m_listensocket, (sockaddr*)&InternetAddr, 16);

		if (nRet == SOCKET_ERROR)
		{
			this->addlog(epollogtype::eERROR, "%s(), bind() failed with error %d.", __func__, SOCKERR);
			return 0;
		}
		else
		{
			nRet = ::listen(this->m_listensocket, 5);
			if (nRet == SOCKET_ERROR)
			{
				this->addlog(epollogtype::eERROR, "%s(), listen() failed with error %d.", __func__, SOCKERR);
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}
}

sock_t clibpoll::createsocket(bool nonblocking)
{
	sock_t s = INVALID_SOCKET;
	unsigned long uNonBlockingMode = 1;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), WSASocket failed: %d.", __func__, SOCKERR);
		return s;
	}
	if (nonblocking == true && ioctlsocket(s,
		FIONBIO,
		&uNonBlockingMode
	) == SOCKET_ERROR) {
		this->addlog(epollogtype::eERROR, "%s(), ioctlsocket failed: %d.", __func__, SOCKERR);
		return INVALID_SOCKET;
	}

	return s;
}

bool clibpoll::sendbuffer(int event_id, unsigned char* lpMsg, size_t dwSize)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX lpPerSocketContext = this->getctx(event_id);

	if (lpPerSocketContext == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d lpPerSocketContext is NULL.", __func__, event_id);
		return false;
	}

	if (lpPerSocketContext->m_rawwrite == true) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d write is set to raw.", __func__, event_id);
		return false;
	}

	if (lpPerSocketContext->m_socket == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d m_socket is INVALID_SOCKET.", __func__, event_id);
		return false;
	}

	POL_BUFFER _buffer;
	_buffer.buffer = new char[dwSize];
	_buffer.len = dwSize;
	_buffer.clear();
	memcpy(_buffer.buffer, lpMsg, dwSize);
	lpPerSocketContext->IOContext[1].vBuffer.push_back(_buffer);

	this->addlog(epollogtype::eDEBUG, "%s(), event id %d add %d bytes to buffer.", __func__, event_id, dwSize);

	this->setepolevent(lpPerSocketContext, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT);
	return true;
}

bool clibpoll::connect(int event_id, char* initData, int initLen)
{
	struct sockaddr_in remote_address;
	unsigned int dwSentBytes = 0;

	LPPOL_PS_CTX pSocketContext = this->getctx(event_id);

	if (pSocketContext == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d pSocketContext is NULL.", __func__, event_id);
		return false;
	}

	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = htonl(pSocketContext->m_conipaddr);
	remote_address.sin_port = htons(pSocketContext->m_conport);
	if (initLen > 0) {
		this->addlog(epollogtype::eDEBUG, "%s(), initial buffer len is %d.", __func__, initLen);
		this->sendbuffer(event_id, (unsigned char*)initData, initLen);
	}

	if (::connect(pSocketContext->m_socket, (sockaddr*)&remote_address, sizeof(remote_address)) == SOCKET_ERROR) {
		int error = SOCKERR;
		if (SOCKCONNBLOCK != error)
		{
			this->addlog(epollogtype::eERROR, "%s(), event id %d connect failed with error %d.", __func__, event_id, error);
			return false;
		}
	}

	this->addlog(epollogtype::eDEBUG, "%s(), event id %d Port:%d succeeded.", __func__, event_id, pSocketContext->m_conport);
	return true;
}

int clibpoll::makeconnect(const char* ipaddr, unsigned short int port, int flag)
{
	int nRet = 0;
	unsigned int bytes = 0;
	unsigned int Flags = 0;
	unsigned int initBufLen = 0;
	struct sockaddr_in addr;
	struct sockaddr_in remote_address;

	this->addlog(epollogtype::eDEBUG, "%s(), Host:%s Port:%d...", __func__, ipaddr, port);

	LPPOL_PS_CTX pSocketContext = NULL;

	pSocketContext = new POL_PS_CTX;
	pSocketContext->clear();


	sock_t s = this->createsocket(true);

	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), createsocket failed.", __func__);
		delete pSocketContext;
		return -1;
	}

	int event_id = (int)s;

	pSocketContext->m_index = 0;
	pSocketContext->m_socket = s;
	pSocketContext->m_conport = port;
	pSocketContext->m_eventid = event_id;
	pSocketContext->m_type = (unsigned char)epoliotype::eCONNECT_IO;

	struct hostent* h = gethostbyname(ipaddr);
	pSocketContext->m_conipaddr = (h != NULL) ? ntohl(*(unsigned int*)h->h_addr) : 0;

	memset(&addr, 0, sizeof(addr));
	memset(&remote_address, 0, sizeof(remote_address));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	if (bind(s, (sockaddr*)&addr, sizeof(addr))) {
		this->addlog(epollogtype::eERROR, "%s(), bind() failed: %d.", __func__, SOCKERR);
		closesocket(s);
		delete pSocketContext;
		return -1;
	}

	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, pSocketContext));
	this->setepolevent(pSocketContext, EPOLL_CTL_ADD, EPOLLWRNORM | EPOLLHUP);
	return event_id;
}

bool clibpoll::setctx(int event_id, LPPOL_PS_CTX ctx)
{
	unsigned long uNonBlockingMode = 1;
	if (ctx == NULL)
		return false;
	std::lock_guard<std::recursive_mutex> lk(m);

	if (ctx->m_socket != INVALID_SOCKET && ioctlsocket(ctx->m_socket,
		FIONBIO,
		&uNonBlockingMode
	) == SOCKET_ERROR) {
		this->addlog(epollogtype::eERROR, "%s(), ioctlsocket failed: %d.", __func__, SOCKERR);
		return false;
	}
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, ctx));
	if (ctx->m_socket != INVALID_SOCKET) {
		this->setepolevent(ctx, EPOLL_CTL_ADD, EPOLLIN | EPOLLHUP);
	}
	return true;
}

bool clibpoll::handleaccept()
{
	u_long uNonBlockingMode = 1;
	char strIP[16] = { 0 };
	struct sockaddr_in peer_addr;
	socklen_t peer_addrlen = sizeof(peer_addr);

	sock_t s = accept(this->m_listensocket, (struct sockaddr*)&peer_addr, &peer_addrlen);

	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eWARNING, "%s(), invalid sock_t, error %d.", __func__, SOCKERR);
		return false;
	}

	if (ioctlsocket(s,
		FIONBIO,
		&uNonBlockingMode
	) == SOCKET_ERROR) {
		this->addlog(epollogtype::eERROR, "%s(), ioctlsocket failed: %d.", __func__, SOCKERR);
		return false;
	}

	inet_ntop(AF_INET, &peer_addr.sin_addr,
		strIP, sizeof(strIP));

	int event_id = (int)s;

	LPPOL_PS_CTX lpAcceptSocketContext = NULL;

	lpAcceptSocketContext = new POL_PS_CTX;
	lpAcceptSocketContext->clear();

	if (event_id == -1) {
		this->addlog(epollogtype::eWARNING, "%s(), no available event id.", __func__);
		closesocket(s);
		delete lpAcceptSocketContext;
		return false;
	}

	memcpy(lpAcceptSocketContext->m_ipaddr, strIP, sizeof(lpAcceptSocketContext->m_ipaddr));
	lpAcceptSocketContext->m_socket = s;
	lpAcceptSocketContext->m_type = (unsigned char)epoliotype::eRECV_IO | (unsigned char)epoliotype::eSEND_IO;
	lpAcceptSocketContext->_this = this;
	lpAcceptSocketContext->m_connected = true;
	lpAcceptSocketContext->m_eventid = event_id;
	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, lpAcceptSocketContext));

	this->setepolevent(lpAcceptSocketContext, EPOLL_CTL_ADD, EPOLLIN | EPOLLHUP);


	if (!this->m_acceptcb(event_id, this->m_acceptarg)) {
		this->addlog(epollogtype::eWARNING, "%s(), Accept Callback returned false.", __func__);
		this->closeeventid(event_id);
		return false;
	}


	this->addlog(epollogtype::eDEBUG, "%s(), event id %d sock_t:%d accepted.", __func__, event_id, lpAcceptSocketContext->m_socket);

	if(lpAcceptSocketContext->eventcb != NULL)
		lpAcceptSocketContext->eventcb(event_id, epolstatus::eCONNECTED, lpAcceptSocketContext->arg);
	return true;
}

bool clibpoll::handleconnect(LPPOL_PS_CTX ctx)
{
	unsigned int Flags = 0;

	int event_id = ctx->m_eventid;
	LPPOL_PIO_CTX lpIOContext = (LPPOL_PIO_CTX)&ctx->IOContext[0];

	ctx->m_connected = true;
	ctx->m_type = (unsigned char)epoliotype::eRECV_IO | (unsigned char)epoliotype::eSEND_IO;
	this->setepolevent(ctx, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT | EPOLLHUP);

	if (ctx->eventcb != NULL) {
		ctx->eventcb(event_id, epolstatus::eCONNECTED, ctx->arg);
		if (!this->iseventidvalid(event_id)) {
			return false;
		}
	}

	this->addlog(epollogtype::eTEST, "%s, event id %d sock_t:%d connected.", __func__, event_id, (int)ctx->m_socket);
	return true;
}

int clibpoll::handlereceive(LPPOL_PS_CTX ctx)
{
	LPPOL_PIO_CTX	lpIOContext = (LPPOL_PIO_CTX)&ctx->IOContext[0];
	int eventid = ctx->m_eventid;

	if (ctx->m_rawread == false) {
		int size = recv(ctx->m_socket, ctx->IOContext[0].Buffer, POL_MAX_IO_BUFFER_SIZE, 0);
		this->addlog(epollogtype::eTEST, "%s, event id %d received %d bytes", __func__, eventid, size);
		if (size <= 0) {
			if (size == SOCKET_ERROR) {
				this->addlog(epollogtype::eERROR, "%s, event id %d recv error %d.", __func__, (int)eventid, SOCKERR);
			}
			return 0;
		}
		lpIOContext->nSentBytes += size;
	}

	if (ctx->recvcb != NULL && !ctx->recvcb(eventid, ctx->arg)) { // receive callback
		return 0;
	}

	//if (!this->iseventidvalid(eventid)) {
	//	this->addlog(epollogtype::eWARNING, "%s(), event id %d is invalid.", __func__, eventid);
	//	return 0;
	//}
	return 1;
}

void clibpoll::reqwrite(int event_id)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return;
	if (_ctx->m_rawwrite == false)
		return;
	this->setepolevent(_ctx, EPOLL_CTL_MOD, EPOLLIN | EPOLLOUT | EPOLLHUP);
}

bool clibpoll::handlesend(LPPOL_PS_CTX ctx)
{
	if (ctx->m_rawwrite == true) {
		if (ctx->sendcb != NULL) {
			if (!ctx->sendcb(ctx->m_eventid, ctx->arg)) {
				return false;
			}
			this->setepolevent(ctx, EPOLL_CTL_MOD, EPOLLIN | EPOLLHUP);
			return true;
		}
	}
	else {
		if (ctx->IOContext[1].vBuffer.size() > 0) {

			std::vector<POL_BUFFER>::iterator iter;
			iter = ctx->IOContext[1].vBuffer.begin();

			int len = iter->len - iter->drained;

			if (len > POL_MAX_IO_BUFFER_SIZE) { // break the packets to chunks, default 8192 bytes chunk
				len = POL_MAX_IO_BUFFER_SIZE;
			}

			if (send(ctx->m_socket, &iter->buffer[iter->drained], len, 0) == SOCKET_ERROR) {
				this->addlog(epollogtype::eERROR, "%s, event id %d send error %d.", __func__, (int)ctx->m_eventid, SOCKERR);
				return false;
			}

			iter->drained += len;
			iter->sent = len;

			if (ctx->sendcb != NULL && !ctx->sendcb(ctx->m_eventid, ctx->arg)) {
				return false;
			}

			this->addlog(epollogtype::eDEBUG, "%s, event id %d sent %d bytes", __func__, (int)ctx->m_eventid, len);

			if (iter->drained >= iter->len) { // fully drained so remove now from vector
				this->addlog(epollogtype::eTEST, "%s, event id %d drained %d bytes from total of %d bytes",
					__func__, (int)ctx->m_eventid, iter->drained, iter->len);
				delete iter->buffer;
				iter = ctx->IOContext[1].vBuffer.erase(iter);
			}

			if (ctx->IOContext[1].vBuffer.size() == 0) {
				this->setepolevent(ctx, EPOLL_CTL_MOD, EPOLLIN | EPOLLHUP);
			}
		}
		else {
			this->setepolevent(ctx, EPOLL_CTL_MOD, EPOLLIN | EPOLLHUP);
			return true;
		}
	}
	return true;
}

size_t clibpoll::readbuffer(int event_id, char* buffer, size_t buffersize)
{
	size_t readbytes = 0;

	LPPOL_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return 0;
	}

	if (ctx->m_rawwrite == true) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d read is set to raw.", __func__, event_id);
		return 0;
	}

	LPPOL_PIO_CTX	lpIOContext = (LPPOL_PIO_CTX)&ctx->IOContext[0];

	if (lpIOContext->nSentBytes <= buffersize) {
		memcpy(buffer, lpIOContext->Buffer, lpIOContext->nSentBytes);
		memset(lpIOContext->Buffer, 0, sizeof(lpIOContext->Buffer));
		readbytes = lpIOContext->nSentBytes;
		lpIOContext->nSentBytes = 0;
	}
	else {
		memcpy(buffer, lpIOContext->Buffer, buffersize);
		readbytes = buffersize;
		lpIOContext->nSentBytes -= readbytes;
		memcpy(lpIOContext->Buffer, &lpIOContext->Buffer[buffersize], lpIOContext->nSentBytes);
		memset(&lpIOContext->Buffer[buffersize], 0, readbytes);
	}

	return readbytes;
}

void clibpoll::closefd(int event_id)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	this->closeeventid(event_id);
}

void clibpoll::closeeventid(int event_id, epolstatus flag)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(epollogtype::eDEBUG, "%s(), event id %d is invalid.", __func__, event_id);
		return;
	}

	switch (flag) {
	case epolstatus::eSOCKERROR:
		this->addlog(epollogtype::eDEBUG, "%s(), event id %d socket %d error.", __func__, event_id, (int)ctx->m_socket);
		if (ctx->eventcb != NULL)
			ctx->eventcb(event_id, epolstatus::eSOCKERROR, ctx->arg);
		this->closeeventid(event_id, epolstatus::eCLOSED);
		break;
	case epolstatus::eCLOSED:

		if (ctx->eventcb != NULL)
			ctx->eventcb(event_id, epolstatus::eCLOSED, ctx->arg);

		this->setepolevent(ctx, EPOLL_CTL_DEL, 0);
		this->deleventid(event_id);

		if (ctx->m_socket != INVALID_SOCKET) {
			::closesocket(ctx->m_socket);
			this->addlog(epollogtype::eDEBUG, "%s(), event id %d socket %d closed.", __func__, event_id, (int)ctx->m_socket);
			ctx->m_socket = INVALID_SOCKET;
		}
		this->deletectx(ctx);
		break;
	}
}

void clibpoll::addlog(epollogtype type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;

	if (((unsigned int)type & this->m_logverboseflags) != (unsigned int)type)
		return;

	std::lock_guard<std::recursive_mutex> lk(m);

	if (this->fnc_loghandler == NULL) {
		sprintf(szBuffer, "[libpoll] (%d) ", type);
		va_start(pArguments, msg);
		size_t iSize = strlen(szBuffer);
		vsprintf(&szBuffer[iSize], msg, pArguments);
		va_end(pArguments);
#ifdef _WIN32
		OutputDebugStringA(szBuffer);
#else
		printf("%s\n", szBuffer);
#endif
	}
	else {
		va_start(pArguments, msg);
		vsprintf(szBuffer, msg, pArguments);
		va_end(pArguments);
		this->fnc_loghandler(type, szBuffer);
	}
}

sock_t clibpoll::getsocket(int event_id) {
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return -1;
	return _ctx->m_socket;
}

void clibpoll::getipaddr(int event_id, char* ipaddr) {
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL || ipaddr == NULL)
		return;
	strcpy(ipaddr, _ctx->m_ipaddr);
}

void clibpoll::setindex(int event_id, intptr_t index) {
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return;
	_ctx->m_index = index;
}

intptr_t clibpoll::getindex(int event_id) {
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return -1;
	return _ctx->m_index;
}

size_t clibpoll::getsentbytes(int event_id)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return 0;
	return _ctx->IOContext[1].vBuffer[0].sent;
}

#ifndef _WIN32
#include <unistd.h>
uint32_t GetTickCount() {
	struct timespec ts;
	unsigned theTick = 0U;
	clock_gettime(CLOCK_REALTIME, &ts);
	theTick = ts.tv_nsec / 1000000;
	theTick += ts.tv_sec * 1000;
	return theTick;
}
#endif
