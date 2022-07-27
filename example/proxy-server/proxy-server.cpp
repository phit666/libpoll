#include <iostream>
#include <libpoll-wrapper.h>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>


static void logger(epollogtype type, const char* msg);
static bool acceptcb(polbase* base, int eventid, LPVOID arg);
static bool readcb(polbase* base, int eventid, LPVOID arg);
static bool remote_readcb(polbase* base, int eventid, LPVOID arg);

static BOOL WINAPI signalhandler(DWORD signum);
static int portProxy;
static int portServer;
static char proxyip[50] = { 0 };
static char svrip[50] = { 0 };
polbase* gbase = NULL;

int main(int argc, char* argv[])
{
    printf("libpoll Proxy-Server Version 1.00.00\n");

    if (argc < 4) {
        std::cout << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "proxy-server.exe <listen ip> <listen port> <remote ip> <remote port>" << std::endl;
        std::cout << std::endl;
        system("pause");
        return -1;
    }

    SetConsoleCtrlHandler(signalhandler, TRUE);

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
    return _getch();
}

static bool acceptcb(polbase* base, int eventid, LPVOID arg)
{
    int event_id = polmakeconnect(base, svrip, portServer); // create the event id of remote connection

    intptr_t _event_id = static_cast<intptr_t>(event_id);
    polsetcb(base, eventid, readcb, NULL, (LPVOID)_event_id); // pass remote event id to client callback

    intptr_t _eventid = static_cast<intptr_t>(eventid);
    polsetcb(base, event_id, remote_readcb, NULL, (LPVOID)_eventid); // pass proxy event id to remote callback

    return true;
}

static bool readcb(polbase* base, int eventid, LPVOID arg)
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
        return polwrite(base, remote_eventid, (LPBYTE)buf, size);
    }
}

static bool remote_readcb(polbase* base, int eventid, LPVOID arg)
{
    intptr_t ptr = (intptr_t)arg;
    int local_eventid = static_cast<int>(ptr);
    char buf[POL_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = polread(base, eventid, buf, POL_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    return polwrite(base, local_eventid, (LPBYTE)buf, size);
}

static BOOL WINAPI signalhandler(DWORD signum)
{
    switch (signum)
    {
    case CTRL_C_EVENT:
        poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
        break;
    }
    return TRUE;
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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
