#include <iostream>
#include <libpoll-wrapper.h>
#include <csignal>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(polbase* base, int eventid, void* arg);
static bool readcb(polbase* base, int eventid, void* arg);
static void logger(epollogtype type, const char* msg);
static void signal_handler(int signal);
polbase* gbase = NULL;

int main()
{
    char sbuf[100] = { 0 };

    std::signal(SIGINT, signal_handler);

    polbase* base = polnewbase(logger);
    gbase = base;

    for (int n = 0; n < 5; n++) {
		sprintf_s(sbuf, 100, "Hello World!"); /**the initial data to send upon connection*/
		int eventid = polconnect(base, "127.0.0.1", 3000, sbuf, strlen(sbuf)+1); /**set the initial buf size to 0 if there has no initial data to send*/
        polsetcb(base, eventid, readcb, NULL, (void*)n);
    }

    std::cout << "press Ctrl-C to exit.\n";
    poldispatch(base); 

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    polbasedelete(base);
    return 1;
}

/**read callback*/
static bool readcb(polbase* base, int eventid, void* arg)
{
    char buff[100] = { 0 };
    int index = (int)arg;

    int readsize = polread(base, eventid, buff, sizeof(buff)); /**receive data, read it now...*/
    std::cout << "Server echo to index " << index << ": " << buff << "\n";
    return true;
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

static void signal_handler(int signal)
{
    poldispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
}
