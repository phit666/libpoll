/*@file bench.cpp
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
#include <iostream>
#include <libpoll-wrapper.h>
#include "third_party/socketpair.h"
#include "third_party/select.h"
#include "third_party/psn.h"
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <cmath>


static void runbench();
static bool readcb(polbase* base, int eventid, void* arg);
static void logger(epollogtype type, const char* msg);
static polbase* base = NULL;
static size_t con = 0;
static size_t writes = 0;
static size_t twrites = 0;
static size_t treads = 0;
static size_t dispatchcounts = 0;
static int ncount = 0;
static int errcount = 0;
std::chrono::time_point<std::chrono::high_resolution_clock> startick;
std::chrono::time_point<std::chrono::high_resolution_clock> endtick;
static intptr_t difftick = 0;
static char method[10] = { 0 };
static int m = 0;
struct socketpair
{
    SOCKET s1;
    SOCKET s2;
};
std::map<int, socketpair> ms;

static struct timeval ts, te;

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cout << std::endl;
        std::cout << "Usage:" << std::endl;
        std::cout << "bench.exe <connections> <writes> <methods: libpoll, select and psn>" << std::endl;
        std::cout << std::endl;
        system("pause");
        return -1;
    }

    sprintf_s(method, 10, "%s", argv[3]);
    con = atoi(argv[1]);
    writes = atoi(argv[2]);

    if (strcmp(method, "select") != 0 && strcmp(method, "libpoll") != 0 && strcmp(method, "psn") != 0) {
        std::cout << "Invalid " << method << " entered, available methods are libpoll, select and psn." << std::endl;
        system("pause");
        return -1;
    }

    if (!con || !writes) {
        std::cout << "connections and writes should be positive values." << std::endl;
        system("pause");
        return -1;
    }

    if (strcmp(method, "select") == 0)
        m = 1;
    else if (strcmp(method, "psn") == 0)
        m = 2;

    std::cout << "<<<" << method << " method benchmark >>>" << std::endl;

#ifdef _WIN32
    WSADATA WSAData;
    WSAStartup(0x0202, &WSAData);
#endif


    if (m == 0) {
        base = polnewbase(logger, NULL);
        polenablecustomcontext(base);
    }
    else if (m == 2) {
        if (!psninit()) {
            printf("psninit failed, err:%d.\n", GetLastError());
        }
    }
    else {
        initselect();
    }

    for (int n = 0; n < con; n++) {

        SOCKET s[2];
        if (int err = dumb_socketpair(s, 0) != 0) {
            printf("socketpair failed, connections:%d err:%d %d.\n", n + 1, err, WSAGetLastError());
            break;
        }
        socketpair spair;
        spair.s1 = s[0];
        spair.s2 = s[1];

        ms.insert(std::pair<int, socketpair>(n, spair));

        if (m == 0) {
            LPPOL_PS_CTX ctx = new POL_PS_CTX;
            ctx->clear();
            ctx->m_connected = true;
            ctx->m_rawread = true;
            ctx->m_socket = s[0];
            ctx->m_eventid = static_cast<int>(s[0]);
            ctx->m_type = (unsigned char)epoliotype::eRECV_IO;
            polsetcustomcontext(base, ctx->m_eventid, ctx);
            polsetcustomarg(base, ctx->m_eventid, (void*)ctx);
            polsetcb(base, ctx->m_eventid, readcb, NULL, NULL, NULL);
        }
        else if (m == 2) {
            if (psnadd(s[0]) == 0){
                printf("psnadd failed, connections:%d err:%d.\n", n + 1,  WSAGetLastError());
                break;
            }
        }
        else {
            addfd(s[0]);
        }
    }

    size_t average = 0;

    for (int n = 0; n < 10; n++) {
        runbench();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(endtick - startick).count();
        average += dur;
        printf("Writes/Read:%lld/%lld Dispatch:%lld Error:%d Result:%lld usec.\n", twrites, treads, dispatchcounts, errcount, dur);
    }

    printf("Average Result:%lld usec.\n", average / 10);

    std::map <int, socketpair>::iterator iter;
    for (iter = ms.begin(); iter != ms.end(); iter++) {
        if (m == 0) {
            int eventid = (int)iter->second.s1;
            LPPOL_PS_CTX ctx = (LPPOL_PS_CTX)polgetcustomarg(base, eventid);
            poldelcustomcontext(base, ctx);
        }
        closesocket(iter->second.s1);
        closesocket(iter->second.s2);
    }

    if(m == 0)
        polbasedelete(base);
    if (m == 2) {
        free(serverContext);
    }
    ms.clear();
#ifdef _WIN32
    WSACleanup();
#endif
}

static void runbench() {

    twrites = treads = dispatchcounts = errcount = ncount = 0;

    if (m == 0)
        poldispatch(base, 0, 10, DISPATCH_DONT_BLOCK);
    else if (m == 2) {
        psndispatch();
    }
    else
        selectdispatch();

    if (send(ms[ncount].s2, ".", 1, 0) > 0) {
        twrites += 1;
    }
    else
        return;

    startick = std::chrono::high_resolution_clock::now();

    for(; treads != twrites; ++dispatchcounts){
        if(m == 0)
            poldispatch(base, 0, 10, DISPATCH_DONT_BLOCK);
        else if (m == 2) {
            if (!psndispatch()) {
                printf("psndispatch failed, error:%d\n", GetLastError());
            }
        }
        else {
            if (!selectdispatch())
                break;
        }
    }

    endtick = std::chrono::high_resolution_clock::now();
}

static bool readcb(polbase* base, int eventid, void* arg) {

    char rbuf[1];

    int len = recv((SOCKET)eventid, rbuf, 1, 0);

    if (!len)
        return false;

    treads += len;

    if (twrites >= writes)
        return true;

    ncount++;

    if (ncount >= con) {
        ncount = 0;
    }

    if (send(ms[ncount].s2, ".", 1, 0) == SOCKET_ERROR) {
        errcount++;
    }

    twrites += 1;

    return true;
}

void selectread(SOCKET s)
{
    char rbuf[1];

    int len = recv(s, rbuf, 1, 0);

    if (!len)
        return;

    treads += len;

    if (twrites >= writes)
        return;

    ncount++;

    if (ncount >= con) {
        ncount = 0;
    }

    if (send(ms[ncount].s2, ".", 1, 0) == SOCKET_ERROR) {
        errcount++;
    }

    twrites += 1;
}


static void logger(epollogtype type, const char* msg) {
    //printf("LOG: %s\n", msg);
}


