#include <iostream>
#include <libpoll-wrapper.h>
#include "third_party/socketpair.h"
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <cmath>

static void startbench();
static bool readcb(polbase* base, int eventid, void* arg);
static void logger(epollogtype type, const char* msg);
static polbase* base = NULL;
std::map<int, SOCKET*> ms;
static size_t con = 0;
static size_t writes = 0;
static size_t twrites = 0;
static size_t reads = 0;
static size_t dispatchcounts = 0;
static int ncount = 0;
static int wcount = 0;
std::chrono::time_point<std::chrono::system_clock> startick;
std::chrono::time_point<std::chrono::system_clock> endtick;
unsigned int difftick = 0;

static char buf[2] = { 0x00 };

int main()
{
    con = 2;
    writes = 1000;

    base = polnewbase(logger, NULL);

    polenablecustomcontext(base);

    for (int n = 0; n < con; n++) {
        SOCKET s[2];
        ms.insert(std::pair<int, SOCKET*>(n, s));
        dumb_socketpair(s, 0);
        LPPOL_PS_CTX ctx = new POL_PS_CTX;
        ctx->m_connected = true;
        ctx->m_rawread = true;
        ctx->m_socket = s[0];
        ctx->m_eventid = static_cast<int>(s[0]);
        ctx->m_type = (unsigned char)epoliotype::eRECV_IO;
        polsetcustomcontext(base, ctx->m_eventid, ctx);
        polsetcustomarg(base, ctx->m_eventid, (void*)ctx);
        polsetcb(base, ctx->m_eventid, readcb, NULL, NULL, NULL);
    }
    
    startbench();

    endtick = std::chrono::system_clock::now();
    unsigned int dur = std::chrono::duration_cast<std::chrono::nanoseconds>(endtick - startick).count() - difftick;
    printf("Writes/Dispatch:%d/%d (%d) Result: %u ms. (%u)\n", twrites, reads, ncount + 1, dur / 1000, difftick);
    
    std::map <int, SOCKET*>::iterator iter;
    for (iter = ms.begin(); iter != ms.end(); iter++) {
        int eventid = (int)iter->second[0];
        LPPOL_PS_CTX ctx = (LPPOL_PS_CTX)polgetcustomarg(base, eventid);
        poldelcustomcontext(base, ctx);
    }
    polbasedelete(base);
    ms.clear();
}

static void startbench() {

    if (send(ms[ncount][1], buf, 1, 0) > 0) {
        wcount++;
        twrites++;
    }

    startick = std::chrono::system_clock::now();

    while (true) 
    {
        poldispatch(base, 0, DISPATCH_DONT_BLOCK);
        dispatchcounts++;

        if (reads == (writes * con))
            break;

        if (wcount < writes) {
            auto s = std::chrono::system_clock::now();
            if (send(ms[ncount][1], buf, 1, 0) > 0) {
                wcount++;
                twrites++;
            }
            auto e = std::chrono::system_clock::now();
            difftick += std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
        }
        else {
            ncount += 1;
            if (ncount < con) {
                wcount = 0;
                auto s = std::chrono::system_clock::now();
                if (send(ms[ncount][1], buf, 1, 0) > 0) {
                    wcount++;
                    twrites++;
                }
                auto e = std::chrono::system_clock::now();
                difftick += std::chrono::duration_cast<std::chrono::nanoseconds>(e - s).count();
            }
        }
    }
}

static bool readcb(polbase* base, int eventid, void* arg) {
    char rbuf[2];
    int len = recv(polgetsocket(base, eventid), rbuf, 1, 0);
    if (len > 0) {
        reads++;
    }
    return true;
}


static void logger(epollogtype type, const char* msg) {
    //printf("LOG: %s\n", msg);
}

