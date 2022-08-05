/*@file psn-epoll.cpp
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
#include <map>
#include <mutex>
#include "psn-epoll.h"

#define EPOLL_CTL_UPDATE 16

static std::recursive_mutex m1;
static std::recursive_mutex m2;

static std::map<int, HANDLE> mfd2hwnd;
static std::map<int, socket_t> mfd2sock;
static std::map<socket_t, int> msock2fd;

static std::map<int, epoll_event*> mevents;

static int epfdctr = 0;
static int fdctr = 0;

static int closed = 0;

int epoll_sock2fd(socket_t s) {
#ifdef _WIN32
    std::map<socket_t, int>::iterator iter;
    std::lock_guard<std::recursive_mutex> lock1(m1);
    iter = msock2fd.find(s);
    if (iter != msock2fd.end())
        return iter->second;
    ++fdctr;
    msock2fd.insert(std::pair<socket_t, int>(s, fdctr));
    mfd2sock.insert(std::pair<int, socket_t>(fdctr, s));
    return fdctr;
#else
    return s;
#endif
}

socket_t epoll_fd2sock(int fd) {
#ifdef _WIN32
    std::map<int, socket_t>::iterator iter;
    std::lock_guard<std::recursive_mutex> lock1(m1);
    iter = mfd2sock.find(fd);
    if (iter != mfd2sock.end())
        return iter->second;
    return INVALID_SOCKET;
#else
    return fd;
#endif
}

#ifdef _WIN32

static void _delevent(int fd) {
    std::map<int, epoll_event*>::iterator iter;
    iter = mevents.find(fd);
    if (iter != mevents.end()) {
        //printf("debug: _delevent delete fd %d\n", fd);
        free(iter->second);
        mevents.erase(iter);
    }
}

void close(int epfd) {
    std::lock_guard<std::recursive_mutex> lock2(m2);
    if (mfd2hwnd[epfd] == NULL)
        return;
    CloseHandle(mfd2hwnd[epfd]);
    closed = 1;
    mfd2hwnd.clear();
    mfd2sock.clear();
    msock2fd.clear();

    std::map<int, epoll_event*>::iterator iter;
    for (iter = mevents.begin(); iter != mevents.end(); iter++) {
        //printf("debug: close delete fd %d\n", iter->first);
        free(iter->second);
    }
    mevents.clear();
}


int epoll_create(int size) {
	if (!size)
		return -1;

	HANDLE phwnd = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    std::lock_guard<std::recursive_mutex> lock1(m1);
    mfd2hwnd.insert(std::pair<int, HANDLE>(++epfdctr, phwnd));
    return epfdctr;
}

int epoll_create1(int flags) {
	return epoll_create(1);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {

    DWORD errorCode;
    u_long nonBlocking = 1;
    SOCK_NOTIFY_REGISTRATION registration = {};
    SOCKET s = epoll_fd2sock(fd);
    epoll_event* pevent = NULL;

    if (s == INVALID_SOCKET)
        return -1;


    switch (op) {

    case EPOLL_CTL_DEL:

        pevent = mevents[fd];
        pevent->events = 0;
        registration.completionKey = pevent;
        registration.operation = op;
        registration.socket = s;

        errorCode = ProcessSocketNotifications(mfd2hwnd[epfd], 1, &registration, 0, 0, NULL, NULL);

        if (errorCode != ERROR_SUCCESS) {
            printf("epoll_ctl EPOLL_CTL_DEL Error:%u\n", errorCode);
            return -1;
        }

        if (registration.registrationResult != ERROR_SUCCESS) {
            errorCode = registration.registrationResult;
            printf("epoll_ctl EPOLL_CTL_DEL Error:%u\n", errorCode);
            return -1;
        }


        return 0;

    case EPOLL_CTL_MOD:

        pevent = mevents[fd];
        if (pevent == NULL)
            return -1;
        if (event != NULL) {
            ::memcpy(pevent, event, sizeof(epoll_event));
        }

        printf("debug: EPOLL_CTL_MOD, events:%u\n", pevent->events);

        registration.completionKey = pevent;
        registration.operation = EPOLL_CTL_DEL;
        registration.socket = s;

        errorCode = ProcessSocketNotifications(mfd2hwnd[epfd], 1, &registration, 0, 0, NULL, NULL);

        if (errorCode != ERROR_SUCCESS) {
            printf("epoll_ctl EPOLL_CTL_MOD Error:%u\n", errorCode);
            return -1;
        }

        if (registration.registrationResult != ERROR_SUCCESS) {
            errorCode = registration.registrationResult;
            printf("epoll_ctl EPOLL_CTL_MOD Error:%u\n", errorCode);
            return -1;
        }

        return 0;

    case EPOLL_CTL_UPDATE:

        pevent = mevents[fd];

        registration.completionKey = (PVOID)pevent;
        registration.eventFilter = pevent->events | SOCK_NOTIFY_REGISTER_EVENT_HANGUP;
        registration.operation = EPOLL_CTL_ADD;
        registration.triggerFlags = SOCK_NOTIFY_TRIGGER_LEVEL | SOCK_NOTIFY_TRIGGER_PERSISTENT;
        registration.socket = s;

        errorCode = ProcessSocketNotifications(mfd2hwnd[epfd], 1, &registration, 0, 0, NULL, NULL);

        if (errorCode != ERROR_SUCCESS) {
            printf("epoll_ctl-1 EPOLL_CTL_UPDATE Error:%u\n", errorCode);
            return -1;
        }

        if (registration.registrationResult != ERROR_SUCCESS) {
            errorCode = registration.registrationResult;
            printf("epoll_ctl-2 EPOLL_CTL_UPDATE Error:%u\n", errorCode);
            return -1;
        }
        return 0;

    case EPOLL_CTL_ADD:

        pevent = (epoll_event*)calloc(1, sizeof(epoll_event));
        if (pevent == NULL)
            return -1;
        if (event != NULL) {
            ::memcpy(pevent, event, sizeof(epoll_event));
        }
        mevents.insert(std::pair<int, epoll_event*>(fd, pevent));

        printf("debug: EPOLL_CTL_ADD, events:%u\n", pevent->events);

        registration.completionKey = (PVOID)pevent;
        registration.eventFilter = pevent->events | SOCK_NOTIFY_REGISTER_EVENT_HANGUP;
        registration.operation = op;
        registration.triggerFlags = SOCK_NOTIFY_TRIGGER_LEVEL | SOCK_NOTIFY_TRIGGER_PERSISTENT;
        registration.socket = s;

        errorCode = ProcessSocketNotifications(mfd2hwnd[epfd], 1, &registration, 0, 0, NULL, NULL);

        if (errorCode != ERROR_SUCCESS) {
            printf("epoll_ctl-1 EPOLL_CTL_ADD Error:%u\n", errorCode);
            return -1;
        }

        if (registration.registrationResult != ERROR_SUCCESS) {
            errorCode = registration.registrationResult;
            printf("epoll_ctl-2 EPOLL_CTL_ADD Error:%u\n", errorCode);
            return -1;
        }

        return 0;
    }


    return -1;
}

int epoll_wait(int epfd, struct epoll_event* events,
	int maxevents, int timeout) {

    SOCKET s;
    ULONG notificationCount;
    UINT32 psnevents;
    OVERLAPPED_ENTRY* notification = NULL;

    std::lock_guard<std::recursive_mutex> lock2(m2);

    if (closed != 0)
        return 0;

    if (events == NULL)
        return -1;

    notification = (OVERLAPPED_ENTRY*)calloc(maxevents, sizeof(OVERLAPPED_ENTRY));

    if (notification == NULL)
        return -1;

    if (!GetQueuedCompletionStatusEx(mfd2hwnd[epfd],
        notification,
        maxevents,
        &notificationCount,
        timeout,
        FALSE))
    {
        if (GetLastError() != WAIT_TIMEOUT)
            return -1;
    }

    std::lock_guard<std::recursive_mutex> lock1(m1);

    int i = 0;

    for (int n = 0; n < notificationCount; n++) {
        psnevents = SocketNotificationRetrieveEvents(&notification[n]);
        epoll_event* _event = (epoll_event*)notification[n].lpCompletionKey;
        if (_event == NULL)
            continue;
        if (psnevents & SOCK_NOTIFY_EVENT_REMOVE) {
            int fd = _event->data.fd;
            epoll_event* pevent = mevents[fd];
            //printf("epoll_wait debug: fd:%d events %d\n", fd, pevent->events);
            if(pevent->events == 0)
                _delevent(fd);
            else {
                epoll_ctl(epfd, EPOLL_CTL_UPDATE, fd, NULL);
            }
            continue;
        }
        events[i].events = psnevents;
        events[i++].data.fd = _event->data.fd;
    }

    free(notification);
    return i;
}

#endif

