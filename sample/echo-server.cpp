/*@file echo-server.cpp
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
    std::thread t[1];

    polbase* base = polnewbase(logger);
    gbase = base;
    pollisten(base, 3000, acceptcb, NULL);

    /*multi-threaded dispatching of events. 4 thread workers are set to poll for events.*/
    for (int n = 0; n < 1; n++) {
        t[n] = std::thread(poldispatch, base, 1000, 10, NULL);
    }

    std::signal(SIGINT, signal_handler);
    std::cout << "press Ctrl-C to exit.\n";

    for (int n = 0; n < 1; n++) {
        t[n].join(); /*lets block here*/
    }

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);

    return 1;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(polbase* base, int eventid, void* arg)
{
    /**client connection accepted, we should store the pol eventid here to our variable..*/

    /**set read and event callback to newly accepted client*/
    polsetcb(base, eventid, readcb, NULL, eventcb, NULL);
    
    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(polbase* base, int eventid, void* arg)
{
    char buff[100] = { 0 };

    size_t readsize = polread(base, eventid, buff, sizeof(buff));
    
    printf(">>> Client message : %s\n", buff);

    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
    polwrite(base, eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/
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
