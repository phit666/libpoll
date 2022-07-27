#include <iostream>
#include <libpoll-wrapper.h>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(polbase* base, int eventid, LPVOID arg);
static bool readcb(polbase* base, int eventid, LPVOID arg);
static void eventcb(polbase* base, int eventid, epolstatus type, LPVOID arg);
static void logger(epollogtype type, const char* msg);

BOOL WINAPI signalhandler(DWORD signum);
int indexctr = 0;
polbase* gbase = NULL;

int main()
{
    SetConsoleCtrlHandler(signalhandler, TRUE);

    polbase* base = polnewbase(logger);
    gbase = base;
    pollisten(base, 3000, acceptcb, NULL);

    std::cout << "press Ctrl-C to exit.\n";
    poldispatch(base);

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);

    std::cout << "cleanup done, press any key to exit.\n";
    int ret = _getch();
    return ret;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(polbase* base, int eventid, LPVOID arg)
{
    /**client connection accepted, we should store the pol eventid here to our variable..*/

    /**set read and event callback to newly accepted client*/
    polsetcb(base, eventid, readcb, eventcb, NULL);
    
    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(polbase* base, int eventid, LPVOID arg)
{
    char buff[100] = { 0 };

    int readsize = polread(base, eventid, buff, sizeof(buff));
    
    std::cout << "Client message : " << buff << "\n";

    polwrite(base, eventid, (LPBYTE)buff, readsize); /**echo the received data from client*/

    return true;
}

/**event callback*/
static void eventcb(polbase* base, int eventid, epolstatus type, LPVOID arg)
{
    char ipaddr[16] = { 0 };
    switch (type) {
    case epolstatus::eCONNECTED:
        polgetipaddr(base, eventid, ipaddr);
        poladdlog(base, epollogtype::eINFO, "client connected, ip:%s socket:%d",
            ipaddr, polgetsocket(base, eventid));
        break;
    case epolstatus::eCLOSED:
        break;
    case epolstatus::eSOCKERROR:
        break;
    }
}

/**log callback*/
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


BOOL WINAPI signalhandler(DWORD signum)
{
    switch (signum)
    {
    case CTRL_C_EVENT:
        poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
        break;
    }
    return TRUE;
}
