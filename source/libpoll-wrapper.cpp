#include "includes/libpoll-wrapper.h"

polbase* polnewbase(polloghandler loghandler, unsigned int logverboseflags, int connect2ndbuffer) {
	clibpoll* muebase = new clibpoll;
	muebase->init(loghandler, logverboseflags, 0, connect2ndbuffer);
	return (polbase*)muebase;
}

void polenablecustomcontext(polbase* base) {
	clibpoll* poll = (clibpoll*)base;
	poll->enablecustomctx();
}

bool polsetcustomcontext(polbase* base, int event_id, LPPOL_PS_CTX ctx) {
	clibpoll* poll = (clibpoll*)base;
	return poll->setctx(event_id, ctx);
}

void poldelcustomcontext(polbase* base, LPPOL_PS_CTX ctx) {
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> _lk(ctx->m);
	poll->deletectx(ctx);
}

void pollisten(polbase* base, unsigned short int port, polacceptcb acceptcb, void* arg, char* listenip) {
	clibpoll* poll = (clibpoll*)base;
	poll->listen(port, acceptcb, arg, listenip);
}

void polsetacceptcbargument(polbase* base, void* arg) {
	clibpoll* poll = (clibpoll*)base;
	poll->setacceptcbargument(arg);
}

void polsetreadeventcbargument(polbase* base, int event_id, void* arg) {
	clibpoll* poll = (clibpoll*)base;
	poll->setreadeventcbargument(event_id, arg);
}

void poldispatch(polbase* base) {
	clibpoll* poll = (clibpoll*)base;
	poll->dispatch();
}

int polconnect(polbase* base, const char* ipaddr, unsigned short int port, char* initbuf, int initlen, int flag, LPPOL_PS_CTX ctx) {
	clibpoll* poll = (clibpoll*)base;
	int eventid = poll->makeconnect(ipaddr, port, flag, ctx);
	LPPOL_PS_CTX _ctx = poll->getctx(eventid);
	if (_ctx == NULL) {
		poll->addlog(epollogtype::eWARNING, "%s(), makeconnect failed.", __func__);
		return NULL;
	}
	if (initbuf == NULL) {
		poll->addlog(epollogtype::eWARNING, "%s(), initbuf is NULL.", __func__);
		return NULL;
	}
	_ctx->_this = (void*)poll;
	poll->connect(eventid, initbuf, initlen);
	return eventid;
}

int polmakeconnect(polbase* base, const char* ipaddr, unsigned short int port, int flag, LPPOL_PS_CTX ctx) {
	clibpoll* poll = (clibpoll*)base;
	int eventid = poll->makeconnect(ipaddr, port, flag, ctx);
	LPPOL_PS_CTX _ctx = poll->getctx(eventid);
	if (_ctx == NULL) {
		poll->addlog(epollogtype::eWARNING, "%s(), polmakeconnect failed.", __func__);
		return NULL;
	}
	_ctx->_this = (void*)poll;
	return eventid;
}

bool polconnect(polbase* base, int eventid, char* initbuf, int initlen) {
	clibpoll* poll = (clibpoll*)base;
	return poll->connect(eventid, initbuf, initlen);
}

bool polisconnected(polbase* base, int eventid) {
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	LPPOL_PS_CTX ctx = poll->getctx(eventid);
	if (ctx == NULL) {
		return false;
	}
	bool bret = ctx->m_connected;
	return bret;
}

void polsetcb(polbase* base, int event_id, polreadcb readcb, poleventcb eventcb, void* arg) {
	clibpoll* poll = (clibpoll*)base;
	poll->setconnectcb(event_id, readcb, eventcb, arg);
}

bool polwrite(polbase* base, int event_id, unsigned char* lpMsg, unsigned int dwSize) {
	clibpoll* poll = (clibpoll*)base;
	return poll->sendbuffer(event_id, lpMsg, dwSize);
}

size_t polread(polbase* base, int event_id, char* buffer, size_t buffersize) {
	clibpoll* poll = (clibpoll*)base;
	return poll->readbuffer(event_id, buffer, buffersize);
}

void poldispatchbreak(polbase* base) {
	clibpoll* poll = (clibpoll*)base;
	poll->dispatchbreak();
}

void polbasedelete(polbase* base) {
	clibpoll* poll = (clibpoll*)base;
	delete poll;
}

void polclosesocket(polbase* base, int event_id) {
	clibpoll* poll = (clibpoll*)base;
	poll->closefd(event_id);
}

void polgetipaddr(polbase* base, int event_id, char* ipaddr) {
	clibpoll* poll = (clibpoll*)base;
	poll->getipaddr(event_id, ipaddr);
}

sock_t polgetsocket(polbase* base, int event_id) {
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	return poll->getsocket(event_id);
}

void polsetindex(polbase* base, int event_id, intptr_t userindex){
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	poll->setindex(event_id, userindex);
}

intptr_t polgetindex(polbase* base, int event_id) {
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	return poll->getindex(event_id);
}

bool polisvalid(polbase* base, int event_id) {
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	return poll->iseventidvalid(event_id);
}

void poladdlog(polbase* base, epollogtype type, const char* msg, ...) {
	char szBuffer[1024] = { 0 };
	va_list pArguments;
	clibpoll* poll = (clibpoll*)base;
	va_start(pArguments, msg);
	vsprintf(szBuffer, msg, pArguments);
	va_end(pArguments);
	poll->addlog(type, szBuffer);
}

bool polsetcustomarg(polbase* base, int event_id, void* arg)
{
	clibpoll* poll = (clibpoll*)base;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	LPPOL_PS_CTX ctx = poll->getctx(event_id);
	if (ctx == NULL) {
		return false;
	}
	ctx->arg2 = arg;
	return true;
}

void* polgetcustomarg(polbase* base, int event_id)
{
	clibpoll* poll = (clibpoll*)base;
	void* arg = NULL;
	std::lock_guard<std::recursive_mutex> lk(poll->m);
	LPPOL_PS_CTX ctx = poll->getctx(event_id);
	if (ctx == NULL) {
		return arg;
	}
	return ctx->arg2;
}






