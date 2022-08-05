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
#include <libpoll-wrapper.h>
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(polbase* base, int eventid, void* arg);
static bool readcb(polbase* base, int eventid, void* arg);
static bool writecb(polbase* base, int eventid, void* arg);
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg);

static void logger(epollogtype type, const char* msg);
static void signal_handler(int signal);
polbase* gbase = NULL;

int main()
{
    polbase* base = polnewbase(logger, (unsigned int)epollogtype::eALL);
    gbase = base;

    for (int n = 0; n < 1; n++) {
		int eventid = polconnect(base, "127.0.0.1", 3000, NULL, 0); /**set the initial buf size to 0 if there has no initial data to send*/
        polsetcb(base, eventid, readcb, writecb, eventcb, (void*)n);
    }

    std::signal(SIGINT, signal_handler);
    std::cout << "press Ctrl-C to exit.\n";

    /*single threaded dispatching of events*/
    poldispatch(base, 1000); 

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);
    return 1;
}

/**event callback*/
static void eventcb(polbase* base, int eventid, epolstatus type, void* arg)
{
    printf("eventcb, type: %d\n", (int)type);

    char sbuf[100] = { 0 };
    switch (type) {
    case epolstatus::eCONNECTED:
        sprintf_s(sbuf, 100, "Hello World!"); 
        polwrite(base, eventid, (unsigned char*)sbuf, sizeof(sbuf)+1);
        break;
    case epolstatus::eCLOSED:
        break;
    }
}

/**read callback*/
int ctr = 10000;
static bool readcb(polbase* base, int eventid, void* arg)
{
    char buff[100] = { 0 };
    int index = (int)arg;

    int readsize = polread(base, eventid, buff, sizeof(buff)); /**receive data, read it now...*/
    printf("<<< Server echo to index %d : %s\n", index, buff);

    if (ctr) {
        //polwrite(base, eventid, (unsigned char*)buff, readsize);
        ctr--;
    }

    return true;
}

/**write callback, we can only get the size of data sent in this callback*/
static bool writecb(polbase* base, int eventid, void* arg)
{
    int index = (int)arg;
    printf(">>> index %d Sent %llu bytes\n", index, polgetsentbytes(base, eventid));
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

static void signal_handler(int signal)
{
    poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
}
