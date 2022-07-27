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
polbase* gbase = NULL;

int main()
{
    char sbuf[100] = { 0 };

    SetConsoleCtrlHandler(signalhandler, TRUE);

    polbase* base = polnewbase(logger, (DWORD)epollogtype::eALL);
    gbase = base;

    sprintf_s(sbuf, 100, "Hello World!");

    for (int n = 0; n < 1; n++) {
        int eventid = polconnect(base, "127.0.0.1", 3000, sbuf, strlen(sbuf)+1);
        polsetcb(base, eventid, readcb, eventcb, (LPVOID)n);
    }

    std::cout << "press Ctrl-C to exit.\n";
    poldispatch(base); 

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);

    std::cout << "cleanup done, press any key to exit.\n";
    int ret = _getch();

    return ret;
}

/**read callback*/
static bool readcb(polbase* base, int eventid, LPVOID arg)
{
    char buff[100] = { 0 };
    int index = (int)arg;

    int readsize = polread(base, eventid, buff, sizeof(buff)); /**receive data, read it now...*/
    std::cout << "Server echo to index " << index << ": " << buff << "\n";
    return true;
}

/**event callback*/
static void eventcb(polbase* base, int eventid, epolstatus type, LPVOID arg)
{
    int index = (int)arg;

    switch (type) {
    case epolstatus::eCONNECTED: /**client is connected to host, send the message now...*/
        break;
    case epolstatus::eCLOSED:
        break;
    case epolstatus::eSOCKERROR: /**free part of MUE object's memory resources, we will totally remove it with remove call or upon exit*/
        break;
    }
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

/**assigned log callback*/
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
