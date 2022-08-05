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
#pragma comment(lib, "Ws2_32")

static std::recursive_mutex m1;
static std::recursive_mutex m2;

static std::map<int, HANDLE> mfd2hwnd;

static std::map<int, SOCKET> mfd2sock;
static std::map<SOCKET, int> msock2fd;

static int epfdctr = 0;
static int fdctr = 0;

int epoll_sock2fd(SOCKET s) {
    std::map<SOCKET, int>::iterator iter;
    std::lock_guard<std::recursive_mutex> lock1(m1);
    iter = msock2fd.find(s);
    if (iter != msock2fd.end())
        return iter->second;
    ++fdctr;
    msock2fd.insert(std::pair<SOCKET, int>(s, fdctr));
    mfd2sock.insert(std::pair<int, SOCKET>(fdctr, s));
    return fdctr;
}

SOCKET epoll_fd2sock(int fd) {
    std::map<int, SOCKET>::iterator iter;
    std::lock_guard<std::recursive_mutex> lock1(m1);
    iter = mfd2sock.find(fd);
    if (iter != mfd2sock.end())
        return iter->second;
    return INVALID_SOCKET;
}

int epoll_create(int size) {
	if (!size)
		return -1;

	HANDLE phwnd = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    std::lock_guard<std::recursive_mutex> lock1(m1);
    mfd2hwnd.insert(std::pair<int, HANDLE>(++epfdctr, phwnd));
}

int epoll_create1(int flags) {
	epoll_create(1);
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {

    DWORD errorCode;
    u_long nonBlocking = 1;
    SOCK_NOTIFY_REGISTRATION registration = {};
    SOCKET s = epoll_fd2sock(fd);

    if (s == INVALID_SOCKET)
        return -1;

    if (ioctlsocket(s, FIONBIO, &nonBlocking) != 0) {
        errorCode = GetLastError();
        return -1;
    }

    registration.completionKey = (PVOID)event;
    registration.eventFilter = event->events;
    registration.operation = op;
    registration.triggerFlags = SOCK_NOTIFY_TRIGGER_LEVEL | SOCK_NOTIFY_TRIGGER_PERSISTENT;
    registration.socket = s;

    errorCode = ProcessSocketNotifications(mfd2hwnd[epfd],
        1,
        &registration,
        0,
        0,
        NULL,
        NULL);

    if (errorCode != ERROR_SUCCESS) {
        return -1;
    }

    if (registration.registrationResult != ERROR_SUCCESS) {
        errorCode = registration.registrationResult;
        return -1;
    }

    return 0;
}

int epoll_wait(int epfd, struct epoll_event* events,
	int maxevents, int timeout) {

    SOCKET s;
    ULONG notificationCount;
    UINT32 psnevents;
    OVERLAPPED_ENTRY* notification = (OVERLAPPED_ENTRY*)calloc(maxevents, sizeof(OVERLAPPED_ENTRY));

    if (events == NULL)
        return -1;

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

    for (int n = 0; n < notificationCount; n++) {
        epoll_event* _event = (epoll_event*)notification[n].lpCompletionKey;
        psnevents = SocketNotificationRetrieveEvents(&notification[n]);
        events[n].events = psnevents;
        events[n].data.fd = _event->data.fd;
    }

    free(notification);
    return notificationCount;
}

