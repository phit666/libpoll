/*@file echo-client.cpp
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
#include <libpoll.h>
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(int eventid, void* arg);
static bool readcb(int eventid, void* arg);
static bool writecb(int eventid, void* arg);
static void logger(epollogtype type, const char* msg);
clibpoll* libpoll = NULL;

int main()
{
    char sbuf[100] = { 0 };
    libpoll = new clibpoll;
    libpoll->init(logger);

    for (int n = 0; n < 1; n++) {
		sprintf_s(sbuf, 100, "Hello World!\0"); /**the initial data to send upon connection*/
        int eventid = libpoll->makeconnect("127.0.0.1", 3000);
        libpoll->connect(eventid, sbuf, strlen(sbuf)+1); /**set the initial buf size to 0 if there has no initial data to send*/
        libpoll->setconnectcb(eventid, readcb, NULL, NULL, (void*)n);
    }

    std::cout << "press any key to exit.\n";

    libpoll->dispatch_threads(4, 10, 1);

    int ret = getchar();

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    libpoll->dispatchbreak();
    delete libpoll;
    return 1;
}

/**read callback*/
static bool readcb(int eventid, void* arg)
{
    char buff[100] = { 0 };
    int index = (int)arg;

    int readsize = libpoll->readbuffer(eventid, buff, sizeof(buff)); /**receive data, read it now...*/
    printf("<<< Server echo to index %d : %d %s\n", index, readsize, buff);
    return true;
}

/**write callback, we can only get the size of data sent in this callback*/
static bool writecb(int eventid, void* arg)
{
    int index = (int)arg;
    printf(">>> index %d Sent %u bytes\n", index, (unsigned int)libpoll->getsentbytes(eventid));
    return true;
}

/**assigned log callback*/
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
