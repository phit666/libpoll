/*@file proxy-server.cpp
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
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>


static void logger(epollogtype type, const char* msg);
static bool acceptcb(int eventid, void* arg);
static bool readcb(int eventid, void* arg);
static bool remote_readcb(int eventid, void* arg);
static void signal_handler(int signal);

static int portProxy;
static int portServer;
static char proxyip[50] = { 0 };
static char svrip[50] = { 0 };
clibpoll* libpoll = NULL;

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

    libpoll = new clibpoll;
    libpoll->init(logger);
    libpoll->listen(portProxy, acceptcb, NULL, proxyip);

    libpoll->dispatch();
    delete libpoll;
    return 1;
}

static bool acceptcb(int eventid, void* arg)
{
    int event_id = libpoll->makeconnect(svrip, portServer); // create the event id of remote connection

    intptr_t _event_id = static_cast<intptr_t>(event_id);
    libpoll->setconnectcb(eventid, readcb, NULL, NULL, (void*)_event_id); // pass remote event id to client callback

    intptr_t _eventid = static_cast<intptr_t>(eventid);
    libpoll->setconnectcb(event_id, remote_readcb, NULL, NULL, (void*)_eventid); // pass proxy event id to remote callback

    return true;
}

static bool readcb(int eventid, void* arg)
{
    intptr_t ptr = (intptr_t)arg;
    int remote_eventid = static_cast<int>(ptr);
    char buf[POL_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = libpoll->readbuffer(eventid, buf, POL_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    if (!libpoll->isconnected(remote_eventid)) {
        return libpoll->connect(remote_eventid, buf, size); // connect to remote and send initial buffer
    }
    else {
        return libpoll->sendbuffer(remote_eventid, (unsigned char*)buf, size);
    }
}

static bool remote_readcb(int eventid, void* arg)
{
    intptr_t ptr = (intptr_t)arg;
    int local_eventid = static_cast<int>(ptr);
    char buf[POL_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = libpoll->readbuffer(eventid, buf, POL_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    return libpoll->sendbuffer(local_eventid, (unsigned char*)buf, size);
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
    libpoll->dispatchbreak(); /**we will return dispatch upon Ctrl-C*/
}
