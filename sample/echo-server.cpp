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
#include <libpoll.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <csignal>
#include <iostream>

static bool acceptcb(int eventid, void* arg);
static bool readcb(int eventid, void* arg);
static void eventcb(int eventid, epolstatus type, void* arg);
static void logger(epollogtype type, const char* msg);

int indexctr = 0;
clibpoll* libpoll = NULL;

int main()
{
    std::thread t[1];

    libpoll = new clibpoll;
    libpoll->init(logger);
    libpoll->listen(3000, acceptcb, NULL);
    libpoll->dispatch_threads(4, INFINITE, 1);

    std::cout << "press any key to exit.\n";

    int ret = getchar();

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    libpoll->dispatchbreak();
    delete libpoll;

    return 1;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(int eventid, void* arg)
{
    /**client connection accepted, we should store the pol eventid here to our variable..*/

    /**set read and event callback to newly accepted client*/
    libpoll->setconnectcb(eventid, readcb, NULL, eventcb, NULL);
    
    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(int eventid, void* arg)
{
    char buff[100] = { 0 };

    size_t readsize = libpoll->readbuffer(eventid, buff, sizeof(buff));
    
    printf(">>> Client message : %s\n", buff);

    libpoll->sendbuffer(eventid, (unsigned char*)buff, readsize); /**echo the received data from client*/

    return true;
}

/**event callback*/
static void eventcb(int eventid, epolstatus type, void* arg)
{
    char ipaddr[16] = { 0 };
    switch (type) {
    case epolstatus::eCONNECTED:
        libpoll->getipaddr(eventid, ipaddr);
        libpoll->addlog(epollogtype::eINFO, "client connected, ip:%s socket:%d",
            ipaddr, libpoll->getsocket(eventid));
        break;
    case epolstatus::eCLOSED:
        libpoll->getipaddr(eventid, ipaddr);
        libpoll->addlog(epollogtype::eINFO, "client disconnected, ip:%s socket:%d",
            ipaddr, libpoll->getsocket(eventid));
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

