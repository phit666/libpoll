#include <iostream>
#include <libpoll-wrapper.h>
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>


static void logger(epollogtype type, const char* msg);
static bool acceptcb(polbase* base, int eventid, void* arg);
static bool readcb(polbase* base, int eventid, void* arg);
static bool remote_readcb(polbase* base, int eventid, void* arg);
static void signal_handler(int signal);

static int portProxy;
static int portServer;
static char proxyip[50] = { 0 };
static char svrip[50] = { 0 };
polbase* gbase = NULL;

int main(int argc, char* argv[])
{
    std::cout << "libpoll Proxy-Server" << std::endl;

    if (argc < 4) {
        std::cout << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "proxy-server.exe <listen ip> <listen port> <remote ip> <remote port>" << std::endl;
        std::cout << std::endl;
        system("pause");
        return -1;
    }

    std::signal(SIGINT, signal_handler);

    std::stringstream s;
    s << argv[2]; s >> portProxy; s.clear();
    s << argv[4]; s >> portServer;
    memcpy(&proxyip, argv[1], sizeof(proxyip));
    memcpy(&svrip, argv[3], sizeof(svrip));

    polbase* base = polnewbase(logger, (DWORD)epollogtype::eALL);
    gbase = base;
    pollisten(base, portProxy, acceptcb, NULL, proxyip);
    poldispatch(base);
    polbasedelete(base);
    return 1;
}

static bool acceptcb(polbase* base, int eventid, void* arg)
{
    int event_id = polmakeconnect(base, svrip, portServer); // create the event id of remote connection

    intptr_t _event_id = static_cast<intptr_t>(event_id);
    polsetcb(base, eventid, readcb, NULL, (void*)_event_id); // pass remote event id to client callback

    intptr_t _eventid = static_cast<intptr_t>(eventid);
    polsetcb(base, event_id, remote_readcb, NULL, (void*)_eventid); // pass proxy event id to remote callback

    return true;
}

static bool readcb(polbase* base, int eventid, void* arg)
{
    intptr_t ptr = (intptr_t)arg;
    int remote_eventid = static_cast<int>(ptr);
    char buf[POL_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = polread(base, eventid, buf, POL_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    if (!polisconnected(base, remote_eventid)) {
        return polconnect(base, remote_eventid, buf, size); // connect to remote and send initial buffer
    }
    else {
        return polwrite(base, remote_eventid, (unsigned char*)buf, size);
    }
}

static bool remote_readcb(polbase* base, int eventid, void* arg)
{
    intptr_t ptr = (intptr_t)arg;
    int local_eventid = static_cast<int>(ptr);
    char buf[POL_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = polread(base, eventid, buf, POL_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    return polwrite(base, local_eventid, (unsigned char*)buf, size);
}

static void logger(epollogtype type, const char* msg)
{
    switch (type) {
    case epollogtype::eINFO:
        std::cout << "[INFO] " << msg << "\n";
        break;
    case epollogtype::eDEBUG:
        std::cout << "[DEBUG] " << msg << "\n";
        break;
    case epollogtype::eERROR:
        std::cout << "[ERROR] " << msg << "\n";
        break;
    case epollogtype::eWARNING:
        std::cout << "[WARNING] " << msg << "\n";
        break;
    }
}

static void signal_handler(int signal)
{
    poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
}