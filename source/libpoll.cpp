#include "includes/libpoll.h"
#include "includes/libpoll-wrapper.h"
#include <assert.h>

#pragma comment(lib, "ws2_32.lib")


clibpoll::clibpoll()
{
	this->m_listensocket = SOCKET_ERROR;
	this->m_acceptctx = NULL;
	this->m_acceptcb = NULL;
	this->m_polmaps.clear();
	this->m_workers = 0;
	this->m_acceptarg = NULL;
	this->m_listenport = 0;
	this->m_eventid = 0;
	this->m_activeworkers = 0;
	this->m_listenip = 0;
	this->m_customctx = false;
	this->m_loopbreak = false;
	this->m_rebuildpollfdarr = false;

	this->m_initcltextbuffsize = POL_MAX_IO_BUFFER_SIZE;
	this->m_initsvrextbuffsize = POL_MAX_IO_BUFFER_SIZE;
	this->m_logverboseflags = (DWORD)epollogtype::eINFO | (DWORD)epollogtype::eERROR;
	this->fnc_loghandler = NULL;

	this->m_pollfdarr = NULL;
	this->m_pollfdcounts = 0;
}

clibpoll::~clibpoll()
{
	this->addlog(epollogtype::eDEBUG, "%s(), deallocator called.", __func__);
	this->clear();
	if (this->m_pollfdarr != NULL)
		free(this->m_pollfdarr);
	WSACleanup();
}

void clibpoll::clear()
{
	LPPOL_PS_CTX _ctx = NULL;
	std::map <int, LPPOL_PS_CTX>::iterator iterq;
	std::lock_guard<std::recursive_mutex> lk(m);
	if (this->m_customctx == false) {
		for (iterq = this->m_polmaps.begin(); iterq != this->m_polmaps.end(); iterq++)
		{
			_ctx = iterq->second;
			if (_ctx != NULL) {
				this->addlog(epollogtype::eDEBUG, "%s(), closing event id %d.", __func__, _ctx->m_eventid);
				if (_ctx->m_socket != INVALID_SOCKET)
					::closesocket(_ctx->m_socket);
				if (_ctx->IOContext[1].pBuffer != NULL) {
					::free(_ctx->IOContext[1].pBuffer);
				}
				delete _ctx;
			}
		}
	}
	this->m_polmaps.clear();
}

void clibpoll::makepollfdarr()
{
	WSAPOLLFD pollfd;
	SOCKET s;
	LPPOL_PS_CTX ctx;
	int pollfdsize = 0;
	std::map <int, LPPOL_PS_CTX>::iterator iterq;

	assert(this->m_pollfdarr != NULL);

	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_rebuildpollfdarr = false;
	this->m_pollfdcounts = 0;
	BYTE* tmpbuffer = (BYTE*)realloc((BYTE*)this->m_pollfdarr, this->m_polmaps.size() * sizeof(WSAPOLLFD));
	if (tmpbuffer == NULL)
		return;
	for (iterq = this->m_polmaps.begin(); iterq != this->m_polmaps.end(); iterq++)
	{
		s = (SOCKET)iterq->first;
		ctx = iterq->second;
		pollfd.fd = s;
		switch (ctx->m_type) {
		case (BYTE)epoliotype::eACCEPT_IO:
			pollfd.events = POLLRDNORM;
			break;
		case (BYTE)epoliotype::eCONNECT_IO:
			pollfd.events = POLLWRNORM;
			break;
		default:
			pollfd.events = POLLIN;
			if (ctx->m_pendingsend == true)
				pollfd.events |= POLLOUT;
		}
		pollfd.revents = 0;
		memcpy(&tmpbuffer[pollfdsize], (LPBYTE)&pollfd, sizeof(WSAPOLLFD));
		pollfdsize += sizeof(WSAPOLLFD);
	}
	this->m_pollfdarr = (WSAPOLLFD*)tmpbuffer;
	this->m_pollfdcounts = this->m_polmaps.size();
}

void clibpoll::deletectx(LPPOL_PS_CTX ctx)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	if (ctx->m_socket != INVALID_SOCKET)
		::closesocket(ctx->m_socket);
	if (ctx->IOContext[1].pBuffer != NULL) {
		::free(ctx->IOContext[1].pBuffer);
	}
	delete ctx;
}

void clibpoll::remove(int event_id)
{
	std::map<int, LPPOL_PS_CTX>::iterator Iter;
	LPPOL_PS_CTX _ctx = NULL;

	std::lock_guard<std::recursive_mutex> lk(m);

	Iter = this->m_polmaps.find(event_id);

	if (Iter == this->m_polmaps.end()) {
		return;
	}

	_ctx = Iter->second;

	if (this->m_customctx == false) {
		if (_ctx->IOContext[1].pBuffer != NULL) {
			::free(_ctx->IOContext[1].pBuffer);
		}
		delete _ctx;
	}
	else {
		_ctx->clear2();
	}

	this->m_polmaps.erase(Iter);
	this->m_rebuildpollfdarr = true;
	this->addlog(epollogtype::eDEBUG, "%s(), delete event id %d.", __func__, event_id);
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

LPPOL_PS_CTX clibpoll::getctx(int event_id)
{
	std::map<int, LPPOL_PS_CTX>::iterator Iter;
	if ((SOCKET)event_id == INVALID_SOCKET)
		return NULL;
	Iter = this->m_polmaps.find(event_id);
	if (Iter == this->m_polmaps.end()) {
		return NULL;
	}
	return Iter->second;
}

void clibpoll::init(polloghandler loghandler, DWORD logverboseflags, size_t initclt2ndbufsize, size_t initsvr2ndbufsize)
{
	HANDLE tmphandle = NULL;

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	this->m_initcltextbuffsize = (initclt2ndbufsize != NULL) ? initclt2ndbufsize : this->m_initcltextbuffsize;
	this->m_initsvrextbuffsize = (initsvr2ndbufsize != NULL) ? initsvr2ndbufsize : this->m_initsvrextbuffsize;
	this->m_logverboseflags = (logverboseflags != (DWORD)-1) ? logverboseflags : this->m_logverboseflags;
	this->fnc_loghandler = (loghandler != NULL) ? loghandler : this->fnc_loghandler;

	this->addlog(epollogtype::eDEBUG, "clibpoll version %02d.%02d.%02d", _LIBPOLL_MAJOR_VER_, _LIBPOLL_MINOR_VER_, _LIBPOLL_PATCH_VER_);

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

	this->m_listensocket = createsocket(true);
	BYTE* tmpbuffer = (BYTE*)calloc(sizeof(WSAPOLLFD), sizeof(BYTE));
	if (tmpbuffer == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), m_pollfdarr is NULL.", __func__);
		return;
	}
	this->m_pollfdarr = (WSAPOLLFD*)tmpbuffer;
}

void clibpoll::dispatch()
{
	while (true) {
		if (this->m_loopbreak)
			break;
		//if(this->m_rebuildpollfdarr)
		this->makepollfdarr();
		if (this->m_pollfdcounts == 0) {
			Sleep(1000);
			continue;
		}
		int err = WSAPoll(this->m_pollfdarr, this->m_pollfdcounts, 1000);
		if (err == SOCKET_ERROR) {
			this->addlog(epollogtype::eDEBUG, "WSAPOLL, Error : %d", WSAGetLastError());
			continue;
		}
		if (err > 0) {
			for (int n = 0; n < this->m_pollfdcounts; n++) {

				//this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d...", this->m_pollfdarr[n].fd);

				if (this->m_pollfdarr[n].fd == INVALID_SOCKET || this->m_pollfdarr[n].revents == 0)
					continue;
				
				LPPOL_PS_CTX ctx = this->getctx(this->m_pollfdarr[n].fd);

				if (ctx == NULL) {
					this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d ctx is NULL.", this->m_pollfdarr[n].fd);
					continue;
				}

				if (this->m_pollfdarr[n].revents & POLLHUP) {
					this->close((int)this->m_pollfdarr[n].fd, epolstatus::eSOCKERROR);
					this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d POLLHUP", this->m_pollfdarr[n].fd);
					continue;
				}

				if (this->m_pollfdarr[n].revents & POLLERR) {
					this->close((int)this->m_pollfdarr[n].fd, epolstatus::eSOCKERROR);
					this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d POLLERR", this->m_pollfdarr[n].fd);
					continue;
				}

				if (this->m_pollfdarr[n].revents & POLLRDNORM) {
					if ((ctx->m_type & (BYTE)epoliotype::eACCEPT_IO) == (BYTE)epoliotype::eACCEPT_IO) {
						this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d eACCEPT_IO", this->m_pollfdarr[n].fd);
						this->handleaccept();
						continue;
					}
				}

				if (this->m_pollfdarr[n].revents & POLLWRNORM) {
					if ((ctx->m_type & (BYTE)epoliotype::eCONNECT_IO) == (BYTE)epoliotype::eCONNECT_IO) {
						this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d eCONNECT_IO", this->m_pollfdarr[n].fd);
						this->handleconnect(ctx);
						continue;
					}
				}

				if ((this->m_pollfdarr[n].revents & POLLIN) && ((ctx->m_type & (BYTE)epoliotype::eRECV_IO) == (BYTE)epoliotype::eRECV_IO)) {
					int size = recv(this->m_pollfdarr[n].fd, ctx->IOContext[0].Buffer, POL_MAX_IO_BUFFER_SIZE, 0);
					this->addlog(epollogtype::eDEBUG, "WSAPOLL, event id %d received %d bytes", this->m_pollfdarr[n].fd, size);
					if (size > 0) {
						this->handlereceive(ctx, size);
					}
					else {
						this->close((int)this->m_pollfdarr[n].fd, epolstatus::eSOCKERROR);
						continue;
					}
				}

				if (this->m_pollfdarr[n].revents & POLLOUT && ((ctx->m_type & (BYTE)epoliotype::eSEND_IO) == (BYTE)epoliotype::eSEND_IO)) {
					this->handlesend(ctx);
				}
			}
		}
	}
}

void clibpoll::dispatchbreak()
{
	this->m_loopbreak = true;
}

void clibpoll::setacceptcbargument(LPVOID arg)
{
	this->m_acceptarg = arg;
}

void clibpoll::setreadeventcbargument(int event_id, LPVOID arg)
{
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return;
	}
	_ctx->arg = arg;
}

void clibpoll::listen(int listenport, polacceptcb acceptcb, LPVOID arg, char* listenip)
{
	this->m_acceptctx = new POL_PS_CTX;
	this->m_acceptctx->clear();

	this->m_acceptcb = acceptcb;
	this->m_acceptarg = arg;
	this->m_listenport = listenport;

	if (listenip != NULL) {
		struct hostent* h = gethostbyname(listenip);
		this->m_listenip = (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;
	}

	if (!this->createlistensocket(listenport)) {
		delete this->m_acceptctx;
		return;
	}

	this->m_accepteventid = (int)this->m_listensocket;

	this->m_acceptctx->m_socket = this->m_listensocket;
	this->m_acceptctx->m_eventid = this->m_accepteventid;
	this->m_acceptctx->m_type = (BYTE)epoliotype::eACCEPT_IO;

	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(this->m_accepteventid, this->m_acceptctx));
	this->m_rebuildpollfdarr = true;
	this->addlog(epollogtype::eINFO, "%s() ok, listen port is %d.", __func__, this->m_listenport);
}

void clibpoll::setconnectcb(int event_id, polreadcb readcb, poleventcb eventcb, LPVOID arg)
{
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		return;
	}
	_ctx->recvcb = readcb;
	_ctx->eventcb = eventcb;
	_ctx->arg = arg;
}

int clibpoll::createlistensocket(WORD port)
{
	sockaddr_in InternetAddr;
	int nRet;

	if (port == 0) {
		return 1;
	}

	if (this->m_listensocket == SOCKET_ERROR)
	{
		this->addlog(epollogtype::eERROR, "%s(), WSASocket() failed with error %d.", __func__, WSAGetLastError());
		return 0;
	}
	else
	{
		InternetAddr.sin_family = AF_INET;
		InternetAddr.sin_addr.S_un.S_addr = htonl(this->m_listenip);
		InternetAddr.sin_port = htons(port);
		nRet = ::bind(this->m_listensocket, (sockaddr*)&InternetAddr, 16);

		if (nRet == SOCKET_ERROR)
		{
			this->addlog(epollogtype::eERROR, "%s(), bind() failed with error %d.", __func__, WSAGetLastError());
			return 0;
		}
		else
		{
			nRet = ::listen(this->m_listensocket, 5);
			if (nRet == SOCKET_ERROR)
			{
				this->addlog(epollogtype::eERROR, "%s(), listen() failed with error %d.", __func__, WSAGetLastError());
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}
}

SOCKET clibpoll::createsocket(bool nonblocking)
{
	SOCKET s = INVALID_SOCKET;
	ULONG uNonBlockingMode = 1;
	s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, NULL);
	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), WSASocket failed: %d.", __func__, WSAGetLastError());
		return s;
	}

	if (nonblocking == true && ioctlsocket(s,
		FIONBIO,
		&uNonBlockingMode
	) == SOCKET_ERROR) {
		this->addlog(epollogtype::eERROR, "%s(), ioctlsocket failed: %d.", __func__, WSAGetLastError());
		return INVALID_SOCKET;
	}

	return s;
}

bool clibpoll::sendbuffer(int event_id, LPBYTE lpMsg, DWORD dwSize)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX lpPerSocketContext = this->getctx(event_id);

	if (lpPerSocketContext == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d lpPerSocketContext is NULL.", __func__, event_id);
		return false;
	}

	if (lpPerSocketContext->m_socket == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d m_socket is INVALID_SOCKET.", __func__, event_id);
		return false;
	}

	LPPOL_PIO_CTX	lpIoCtxt = (LPPOL_PIO_CTX)&lpPerSocketContext->IOContext[1];

	if (lpIoCtxt->pBuffer == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d pBuffer is NULL.", __func__, event_id);
		return false;
	}

	if (lpIoCtxt->nWaitIO > 0)
	{
		if ((lpIoCtxt->nSecondOfs + dwSize) > lpIoCtxt->pBufferLen)
		{
			if (lpIoCtxt->pReallocCounts >= POL_MAX_CONT_REALLOC_REQ) {
				this->addlog(epollogtype::eERROR, "%s(), event id %d pReallocCounts (1) is max out, 0x%x", __func__, event_id, POL_MAX_CONT_REALLOC_REQ);
				::free(lpIoCtxt->pBuffer);
				lpIoCtxt->pBufferLen = POL_MAX_IO_BUFFER_SIZE;
				lpIoCtxt->pBuffer = (CHAR*)calloc(lpIoCtxt->pBufferLen, sizeof(CHAR));
				lpIoCtxt->pReallocCounts = 0;
				this->close(event_id);
				return false;
			}

			while (true) {
				lpIoCtxt->pBufferLen *= 2;
				if (lpIoCtxt->pBufferLen > (lpIoCtxt->nSecondOfs + dwSize))
					break;
			}

			CHAR* tmpBuffer = (CHAR*)realloc(lpIoCtxt->pBuffer, lpIoCtxt->pBufferLen);

			if (tmpBuffer == NULL) {
				this->addlog(epollogtype::eERROR, "%s(), event id %d realloc (1) failed, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
				this->close(event_id);
				return false;
			}

			lpIoCtxt->pBuffer = tmpBuffer;
			lpIoCtxt->pReallocCounts++;

			this->addlog(epollogtype::eDEBUG, "%s(), event id %d realloc (1) succeeded, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
		}

		this->addlog(epollogtype::eTEST, "%s(), event id %d copied %d bytes to pBuffer, pbuffer size now is %d.", __func__,
			event_id, dwSize, lpIoCtxt->nSecondOfs + dwSize);

		memcpy(&lpIoCtxt->pBuffer[lpIoCtxt->nSecondOfs], lpMsg, dwSize);
		lpIoCtxt->nSecondOfs += dwSize;

		lpPerSocketContext->m_pendingsend = true;

		return true;
	}

	lpIoCtxt->nTotalBytes = 0;

	if (lpIoCtxt->nSecondOfs > 0)
	{
		if (lpIoCtxt->nSecondOfs <= POL_MAX_IO_BUFFER_SIZE) {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, lpIoCtxt->nSecondOfs);
			lpIoCtxt->nTotalBytes = lpIoCtxt->nSecondOfs;
			lpIoCtxt->nSecondOfs = 0;
		}
		else {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, POL_MAX_IO_BUFFER_SIZE);
			lpIoCtxt->nTotalBytes = POL_MAX_IO_BUFFER_SIZE;
			lpIoCtxt->nSecondOfs -= POL_MAX_IO_BUFFER_SIZE;
			memcpy(lpIoCtxt->pBuffer, &lpIoCtxt->pBuffer[POL_MAX_IO_BUFFER_SIZE], lpIoCtxt->nSecondOfs);
		}
	}

	if ((lpIoCtxt->nTotalBytes + dwSize) > POL_MAX_IO_BUFFER_SIZE)
	{
		int bufflen = (lpIoCtxt->nTotalBytes + dwSize) - POL_MAX_IO_BUFFER_SIZE; // 808
		int difflen = POL_MAX_IO_BUFFER_SIZE - lpIoCtxt->nTotalBytes; // 192

		if ((lpIoCtxt->nSecondOfs + bufflen) > lpIoCtxt->pBufferLen) {

			if (lpIoCtxt->pReallocCounts >= POL_MAX_CONT_REALLOC_REQ) {
				this->addlog(epollogtype::eERROR, "%s(), event id %d pReallocCounts (2) is max out, %d", __func__, event_id, POL_MAX_CONT_REALLOC_REQ);
				::free(lpIoCtxt->pBuffer);
				lpIoCtxt->pBufferLen = POL_MAX_IO_BUFFER_SIZE;
				lpIoCtxt->pBuffer = (CHAR*)calloc(lpIoCtxt->pBufferLen, sizeof(CHAR));
				lpIoCtxt->pReallocCounts = 0;
				this->close(event_id);
				return false;
			}

			while (true) {
				lpIoCtxt->pBufferLen *= 2;
				if (lpIoCtxt->pBufferLen > (lpIoCtxt->nSecondOfs + dwSize))
					break;
			}

			CHAR* tmpBuffer = (CHAR*)realloc(lpIoCtxt->pBuffer, lpIoCtxt->pBufferLen);

			if (tmpBuffer == NULL) {
				this->addlog(epollogtype::eERROR, "%s(), event id %d realloc (2) failed, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
				this->close(event_id);
				return false;
			}

			lpIoCtxt->pBuffer = tmpBuffer;
			lpIoCtxt->pReallocCounts++;

			this->addlog(epollogtype::eDEBUG, "%s(), event id %d realloc (2) succeeded, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
		}

		memcpy(&lpIoCtxt->pBuffer[lpIoCtxt->nSecondOfs], &lpMsg[difflen], bufflen);
		lpIoCtxt->nSecondOfs += bufflen;

		if (difflen > 0) {
			memcpy(&lpIoCtxt->Buffer[lpIoCtxt->nTotalBytes], lpMsg, difflen);
			lpIoCtxt->nTotalBytes += difflen;
		}
	}
	else {
		memcpy(&lpIoCtxt->Buffer[lpIoCtxt->nTotalBytes], lpMsg, dwSize);
		lpIoCtxt->nTotalBytes += dwSize;
	}

	lpPerSocketContext->m_pendingsend = true;
	lpIoCtxt->nWaitIO = 1;
	return true;
}

bool clibpoll::connect(int event_id, char* initData, int initLen)
{
	struct sockaddr_in remote_address;
	DWORD dwSentBytes = 0;
	std::lock_guard<std::recursive_mutex> lk(m);
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
		this->sendbuffer(event_id, (LPBYTE)initData, initLen);
	}

	if (::connect(pSocketContext->m_socket, (SOCKADDR*)&remote_address, sizeof(remote_address)) == SOCKET_ERROR) {
		int error = WSAGetLastError();
		if (WSAEWOULDBLOCK != error)
		{
			this->addlog(epollogtype::eERROR, "%s(), event id %d connect failed with error %d.", __func__, event_id, error);
			return false;
		}
	}

	this->addlog(epollogtype::eDEBUG, "%s(), event id %d Port:%d succeeded.", __func__, event_id, pSocketContext->m_conport);
	pSocketContext->m_lastconnecttick = GetTickCount();
	return true;
}

int clibpoll::makeconnect(const char* ipaddr, WORD port, intptr_t index, LPPOL_PS_CTX ctx)
{
	int nRet = 0;
	DWORD bytes = 0;
	DWORD Flags = 0;
	DWORD initBufLen = 0;
	struct sockaddr_in addr;
	struct sockaddr_in remote_address;

	this->addlog(epollogtype::eDEBUG, "%s(), Index %d Port:%d...", __func__, (int)index, port);

	LPPOL_PS_CTX pSocketContext = NULL;

	if (this->m_customctx == true) {
		pSocketContext = ctx;
	}
	else {
		pSocketContext = new POL_PS_CTX;
	}

	if (this->m_customctx == false) {
		pSocketContext->clear();
	}

	SOCKET s = this->createsocket(true);

	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eERROR, "%s(), createsocket failed.", __func__);
		delete pSocketContext;
		return -1;
	}

	int event_id = (int)s;

	pSocketContext->m_index = index;
	pSocketContext->m_socket = s;
	pSocketContext->m_conport = port;
	pSocketContext->m_eventid = event_id;
	pSocketContext->m_type = (BYTE)epoliotype::eCONNECT_IO;

	struct hostent* h = gethostbyname(ipaddr);
	pSocketContext->m_conipaddr = (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;
	
	if (this->m_customctx == false) {
		pSocketContext->IOContext[1].pBuffer = (CHAR*)calloc(this->m_initsvrextbuffsize, sizeof(CHAR));
		pSocketContext->IOContext[1].pBufferLen = this->m_initsvrextbuffsize;
	}

	ZeroMemory(&addr, sizeof(addr));
	ZeroMemory(&remote_address, sizeof(remote_address));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	if (bind(s, (SOCKADDR*)&addr, sizeof(addr))) {
		this->addlog(epollogtype::eERROR, "%s(), bind() failed: %d.", __func__, WSAGetLastError());
		if (this->m_customctx == false) {
			::free(pSocketContext->IOContext[1].pBuffer);
		}
		::closesocket(s);
		delete pSocketContext;
		return -1;
	}

	this->addlog(epollogtype::eDEBUG, "%s(), Index %d Port:%d succeeded.", __func__, (int)index, port);
	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, pSocketContext));
	this->m_rebuildpollfdarr = true;
	return event_id;
}

bool clibpoll::setctx(int event_id, LPPOL_PS_CTX ctx)
{
	if (ctx == NULL)
		return false;
	std::lock_guard<std::recursive_mutex> lk(m);
	this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, ctx));
	this->m_rebuildpollfdarr = true;
	return true;
}

bool clibpoll::handleaccept()
{
	u_long uNonBlockingMode = 1;
	char strIP[16] = { 0 };
	struct sockaddr_in peer_addr;
	socklen_t peer_addrlen = sizeof(peer_addr);

	std::lock_guard<std::recursive_mutex> lk(m);

	SOCKET s = accept(this->m_listensocket, (struct sockaddr*)&peer_addr, &peer_addrlen);

	if (s == INVALID_SOCKET) {
		this->addlog(epollogtype::eWARNING, "%s(), invalid socket, error %d.", __func__, WSAGetLastError());
		return false;
	}

	if (ioctlsocket(s,
		FIONBIO,
		&uNonBlockingMode
	) == SOCKET_ERROR) {
		this->addlog(epollogtype::eERROR, "%s(), ioctlsocket failed: %d.", __func__, WSAGetLastError());
		return false;
	}

	inet_ntop(AF_INET, &peer_addr.sin_addr,
		strIP, sizeof(strIP));

	int event_id = (int)s;

	LPPOL_PS_CTX lpAcceptSocketContext = NULL;

	if (this->m_customctx == false) {

		lpAcceptSocketContext = new POL_PS_CTX;
		lpAcceptSocketContext->clear();

		if (event_id == -1) {
			this->addlog(epollogtype::eWARNING, "%s(), no available event id.", __func__);
			::closesocket(s);
			delete lpAcceptSocketContext;
			return false;
		}

		memcpy(lpAcceptSocketContext->m_ipaddr, strIP, sizeof(lpAcceptSocketContext->m_ipaddr));
		if (this->m_customctx == false) {
			lpAcceptSocketContext->IOContext[1].pBuffer = (CHAR*)calloc(this->m_initcltextbuffsize, sizeof(CHAR));
			lpAcceptSocketContext->IOContext[1].pBufferLen = this->m_initcltextbuffsize;
		}
		lpAcceptSocketContext->m_socket = s;
		lpAcceptSocketContext->m_type = (BYTE)epoliotype::eRECV_IO | (BYTE)epoliotype::eSEND_IO;
		lpAcceptSocketContext->_this = this;
		lpAcceptSocketContext->m_connected = true;
		lpAcceptSocketContext->m_eventid = event_id;
		this->m_polmaps.insert(std::pair<int, LPPOL_PS_CTX>(event_id, lpAcceptSocketContext));
		this->m_rebuildpollfdarr = true;
	}
	else {
		if (event_id == -1) {
			this->addlog(epollogtype::eWARNING, "%s(), no available event id.", __func__);
			::closesocket(s);
			return false;
		}
	}

	if (this->m_customctx == true) {
		LPstpolConnectInfo connectinfo = (LPstpolConnectInfo)this->m_acceptarg;
		if (connectinfo != NULL) {
			memcpy(connectinfo->ipaddr, strIP, sizeof(connectinfo->ipaddr));
			connectinfo->s = s;
		}
	}

	if (!this->m_acceptcb(this, event_id, this->m_acceptarg)) {
		this->addlog(epollogtype::eWARNING, "%s(), Accept Callback returned false.", __func__);
		this->close(event_id);
		return false;
	}

	if (this->m_customctx == true) {
		lpAcceptSocketContext = this->getctx(event_id);

		if (lpAcceptSocketContext == NULL) {
			this->addlog(epollogtype::eWARNING, "%s(), event id %d ctx is NULL.", __func__, event_id);
			return false;
		}

		memcpy(lpAcceptSocketContext->m_ipaddr, strIP, sizeof(lpAcceptSocketContext->m_ipaddr));
		if (this->m_customctx == false) {
			lpAcceptSocketContext->IOContext[1].pBuffer = (CHAR*)calloc(this->m_initcltextbuffsize, sizeof(CHAR));
			lpAcceptSocketContext->IOContext[1].pBufferLen = this->m_initcltextbuffsize;
		}
		lpAcceptSocketContext->m_socket = s;
		lpAcceptSocketContext->m_type = (BYTE)epoliotype::eRECV_IO | (BYTE)epoliotype::eSEND_IO;
		lpAcceptSocketContext->_this = this;
		lpAcceptSocketContext->m_eventid = event_id;
		lpAcceptSocketContext->m_connected = true;
	}

	this->addlog(epollogtype::eDEBUG, "%s(), event id %d socket:%d accepted.", __func__, event_id, lpAcceptSocketContext->m_socket);

	if(lpAcceptSocketContext->eventcb != NULL)
		lpAcceptSocketContext->eventcb(this, event_id, epolstatus::eCONNECTED, lpAcceptSocketContext->arg);
	return true;
}

bool clibpoll::handleconnect(LPPOL_PS_CTX ctx)
{
	DWORD Flags = 0;
	std::lock_guard<std::recursive_mutex> lk(m);

	int event_id = ctx->m_eventid;
	LPPOL_PIO_CTX lpIOContext = (LPPOL_PIO_CTX)&ctx->IOContext[0];

	if (lpIOContext == NULL) {
		this->addlog(epollogtype::eERROR, "%s(), event id %d lpIOContext is NULL.", __func__, event_id);
		this->close(event_id, epolstatus::eSOCKERROR);
		return false;
	}

	ctx->m_connected = true;
	ctx->m_type = (BYTE)epoliotype::eRECV_IO | (BYTE)epoliotype::eSEND_IO;

	if (ctx->eventcb != NULL) {
		ctx->eventcb(this, event_id, epolstatus::eCONNECTED, ctx->arg);
		if (!this->iseventidvalid(event_id)) {
			return false;
		}
	}

	this->addlog(epollogtype::eDEBUG, "%s(), event id %d Socket:%d connected.", __func__, event_id, (int)ctx->m_socket);
	return true;
}

bool clibpoll::handlereceive(LPPOL_PS_CTX ctx, DWORD dwIoSize)
{
	std::lock_guard<std::recursive_mutex> lk(m);

	LPPOL_PIO_CTX	lpIOContext = (LPPOL_PIO_CTX)&ctx->IOContext[0];
	int eventid = ctx->m_eventid;

	lpIOContext->nSentBytes += dwIoSize;
	if (ctx->recvcb != NULL && !ctx->recvcb(this, eventid, ctx->arg)) { // receive callback
		this->addlog(epollogtype::eERROR, "%s(), event id %d Socket Header error %d", __func__, eventid, WSAGetLastError());
		this->close(eventid, epolstatus::eSOCKERROR);
		return false;
	}

	if (!this->iseventidvalid(eventid)) {
		this->addlog(epollogtype::eWARNING, "%s(), event id %d is invalid.", __func__, eventid);
		return false;
	}

	lpIOContext->nWaitIO = 0;
	return true;
}

bool clibpoll::handlesend(LPPOL_PS_CTX ctx)
{
	std::lock_guard<std::recursive_mutex> lk(m);

	if (ctx->IOContext[1].nTotalBytes > 0) {
		if (send(ctx->m_socket, ctx->IOContext[1].Buffer, ctx->IOContext[1].nTotalBytes, 0) == SOCKET_ERROR) {
			ctx->IOContext[1].nTotalBytes = 0;
			ctx->IOContext[1].nSecondOfs = 0;
			ctx->IOContext[1].nWaitIO = 0;
			this->close((int)ctx->m_eventid, epolstatus::eSOCKERROR);
			return false;
		}
		this->addlog(epollogtype::eDEBUG, "%s, event id %d sent %d bytes (1)", __func__, (int)ctx->m_eventid, ctx->IOContext[1].nTotalBytes);
		ctx->IOContext[1].nTotalBytes = 0;
		ctx->IOContext[1].pReallocCounts = 0;
		ctx->m_pendingsend = false;
	}
	if (ctx->IOContext[1].nSecondOfs > 0) {
		if (ctx->IOContext[1].nSecondOfs <= POL_MAX_IO_BUFFER_SIZE) {
			memcpy(ctx->IOContext[1].Buffer, ctx->IOContext[1].pBuffer, ctx->IOContext[1].nSecondOfs);
			ctx->IOContext[1].nTotalBytes = ctx->IOContext[1].nSecondOfs;
			ctx->IOContext[1].nSecondOfs = 0;
		}
		else {
			memcpy(ctx->IOContext[1].Buffer, ctx->IOContext[1].pBuffer, POL_MAX_IO_BUFFER_SIZE);
			ctx->IOContext[1].nTotalBytes = POL_MAX_IO_BUFFER_SIZE;
			ctx->IOContext[1].nSecondOfs -= POL_MAX_IO_BUFFER_SIZE;
			memcpy(ctx->IOContext[1].pBuffer, &ctx->IOContext[1].pBuffer[POL_MAX_IO_BUFFER_SIZE], ctx->IOContext[1].nSecondOfs);
		}
		this->addlog(epollogtype::eDEBUG, "%s, event id %d copy %d bytes to buffer", __func__, (int)ctx->m_eventid, ctx->IOContext[1].nTotalBytes);
		ctx->m_pendingsend = true;
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

void clibpoll::closesocket(int event_id)
{
	std::lock_guard<std::recursive_mutex> lk(m);
	LPPOL_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(epollogtype::eDEBUG, "%s(), event id %d is invalid.", __func__, event_id);
		return;
	}

	if (ctx->m_socket != INVALID_SOCKET) {
		::closesocket(ctx->m_socket);
		ctx->m_socket = INVALID_SOCKET;

		if (ctx->eventcb != NULL)
			ctx->eventcb(this, event_id, epolstatus::eCLOSED, ctx->arg);
	}
}

void clibpoll::close(int event_id, epolstatus flag)
{
	if (flag == epolstatus::eCLOSED) {
		this->closesocket(event_id);
		return;
	}

	std::lock_guard<std::recursive_mutex> lk(m);

	LPPOL_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(epollogtype::eDEBUG, "%s(), event id %d is invalid.", __func__, event_id);
		return;
	}

	if (ctx->m_socket != INVALID_SOCKET) {
		::closesocket(ctx->m_socket);
		ctx->m_socket = INVALID_SOCKET;
	}

	poleventcb eventcb = ctx->eventcb;
	LPVOID eventcb_arg = ctx->arg;

	this->remove(event_id);

	if (flag != epolstatus::eNOEVENCB && eventcb != NULL)
		eventcb(this, event_id, flag, eventcb_arg);
}

void clibpoll::addlog(epollogtype type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;

	if (((DWORD)type & this->m_logverboseflags) != (DWORD)type)
		return;

	if (this->fnc_loghandler == NULL) {
		sprintf(szBuffer, "[clibpoll] (%d) ", type);
		va_start(pArguments, msg);
		size_t iSize = strlen(szBuffer);
		vsprintf_s(&szBuffer[iSize], 1024 - iSize, msg, pArguments);
		va_end(pArguments);
		OutputDebugStringA(szBuffer);
	}
	else {
		va_start(pArguments, msg);
		vsprintf_s(szBuffer, 1024, msg, pArguments);
		va_end(pArguments);
		this->fnc_loghandler(type, szBuffer);
	}
}

SOCKET clibpoll::getsocket(int event_id) {
	LPPOL_PS_CTX _ctx = this->getctx(event_id); 
	if (_ctx == NULL)
		return -1;
	return _ctx->m_socket;
}

void clibpoll::getipaddr(int event_id, char* ipaddr) {
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL || ipaddr == NULL)
		return;
	strcpy(ipaddr, _ctx->m_ipaddr);
}

void clibpoll::setindex(int event_id, intptr_t index) {
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return;
	_ctx->m_index = index;
}

intptr_t clibpoll::getindex(int event_id) {
	LPPOL_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return -1;
	return _ctx->m_index;
}
