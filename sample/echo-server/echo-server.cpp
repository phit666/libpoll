#include <libpoll-wrapper.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <csignal>
#include <iostream>

static bool acceptcb(polbase* base, int eventid, void* arg);
static bool readcb(polbase* base, int eventid, void* arg);
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg);
static void logger(epollogtype type, const char* msg);
static void signal_handler(int signal);

int indexctr = 0;
polbase* gbase = NULL;

int main()
{
    std::signal(SIGINT, signal_handler);

    polbase* base = polnewbase(logger);
    gbase = base;
    pollisten(base, 3000, acceptcb, NULL);

    std::cout << "press Ctrl-C to exit.\n";
    poldispatch(base);

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);

    return 1;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(polbase* base, int eventid, void* arg)
{
    /**client connection accepted, we should store the pol eventid here to our variable..*/

    /**set read and event callback to newly accepted client*/
    polsetcb(base, eventid, readcb, eventcb, NULL);
    
    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(polbase* base, int eventid, void* arg)
{
    char buff[100] = { 0 };

    int readsize = polread(base, eventid, buff, sizeof(buff));
    
    printf(">>> Client message : %s\n", buff);

    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/

    return true;
}

/**event callback*/
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg)
{
    char ipaddr[16] = { 0 };
    switch (type) {
    case epolstatus::eCONNECTED:
        polgetipaddr(base, eventid, ipaddr);
        poladdlog(base, epollogtype::eINFO, "client connected, ip:%s socket:%d",
            ipaddr, polgetsocket(base, eventid));
        break;
    case epolstatus::eCLOSED:
        polgetipaddr(base, eventid, ipaddr);
        poladdlog(base, epollogtype::eINFO, "client disconnected, ip:%s socket:%d",
            ipaddr, polgetsocket(base, eventid));
        break;
    }
}

/**log callback*/
static void logger(epollogtype type, const char* msg)
{
    switch (type) {
    case epollogtype::eINFO:
        printf("[INFO] %s\n", msg);
        break;
    case epollogtype::eTEST:
        printf("[TEST] %s\n", msg);
        break;
    case epollogtype::eDEBUG:
        printf("[DEBUG] %s\n", msg);
        break;
    case epollogtype::eERROR:
        printf("[ERROR] %s\n", msg);
        break;
    case epollogtype::eWARNING:
        printf("[WARNING] %s\n", msg);
        break;
    }
}

static void signal_handler(int signal)
{
    poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
}
