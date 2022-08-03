#include <libpoll-wrapper.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <csignal>
#include <iostream>
#include <map>

#define BUFFER_SIZE 8192

struct context {
    context()
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(ip, 0, sizeof(ip));
        bufferlen = 0;
        s = INVALID_SOCKET;
    }
    char buffer[BUFFER_SIZE];
    int bufferlen;
    char ip[16];
    SOCKET s;
};

/*this will dynamically store our client context*/
static std::map<int, context> mcontext;

static bool acceptcb(polbase* base, int eventid, void* arg);
static bool readcb(polbase* base, int eventid, void* arg);
static bool writecb(polbase* base, int eventid, void* arg);
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg);
static void logger(epollogtype type, const char* msg);
static void signal_handler(int signal);

int indexctr = 0;
polbase* gbase = NULL;

int main()
{
    std::thread t[4];

    polbase* base = polnewbase(logger);
    gbase = base;
    pollisten(base, 3000, acceptcb, NULL);

    std::signal(SIGINT, signal_handler);
    std::cout << "press Ctrl-C to exit.\n";

    poldispatch(base, 1000);

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);
    mcontext.clear();
    return 1;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(polbase* base, int eventid, void* arg)
{
    /**store variables*/
    context ctx;
    polgetipaddr(base, eventid, ctx.ip);
    ctx.s = polgetsocket(base, eventid);
    mcontext.insert(std::pair<int, context>(eventid, ctx));

    /**set read, write and event callback to newly accepted client*/
    polsetcb(base, eventid, readcb, writecb, eventcb, NULL);
    
    /*set both read and write raw*/
    polsetraw(base, eventid, true, true);

    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(polbase* base, int eventid, void* arg)
{
    /*this is a none blocking receive so no need to loop*/
    int len = recv(mcontext[eventid].s, mcontext[eventid].buffer, BUFFER_SIZE, 0);

    if (!len) {
        return false;
    }

    mcontext[eventid].bufferlen += len;

    printf(">>> Client message : %s\n", mcontext[eventid].buffer);

    /*request write will trigger the write callback when write is set to raw*/
    polreqwrite(base, eventid);

    return true;
}

/**write callback, returing false in this callback will close the client*/
static bool writecb(polbase* base, int eventid, void* arg)
{
    if (mcontext[eventid].bufferlen &&
        send(mcontext[eventid].s, mcontext[eventid].buffer, mcontext[eventid].bufferlen, 0) == SOCKET_ERROR)
        return false;

    /*this is a none blocking send so sent bytes is equal to bufferlen*/
    mcontext[eventid].bufferlen = 0;

    return true;
}

/**event callback*/
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg)
{
    char ipaddr[16] = { 0 };
    switch (type) {
    case epolstatus::eCONNECTED:
        poladdlog(base, epollogtype::eINFO, "client connected, ip:%s socket:%d",
            mcontext[eventid].ip, mcontext[eventid].s);
        break;
    case epolstatus::eCLOSED:
        poladdlog(base, epollogtype::eINFO, "client disconnected, ip:%s socket:%d",
            mcontext[eventid].ip, mcontext[eventid].s);

        /*delete our client context when closed*/
        std::map <int, context>::iterator iter;
        iter = mcontext.find(eventid);
        if (iter != mcontext.end())
            mcontext.erase(iter);

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
