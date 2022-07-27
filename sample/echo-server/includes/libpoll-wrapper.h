/*
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
#pragma once
#include "libpoll.h"

/**
	@file pol.h

	pol is a wrapper to mueiocp class in procedural style, designed to simplify
	the implementation of AcceptEx, ConnectEx and GetQueuedCompletionStatusEx windows API.

	Interface Callbacks:
		polacceptcb - Accept/Listen callback to process new connection
		polreadcb - Read callback to process received data
		poleventcb - Event callback to process connection status
*/

/**
	stpolConnectInfo struct store the newly connected IP address and socket, this is only use when polenablecustomcontext is called 
	because when the custom context is provided the IP address and socket info will not available in the custom context inside the 
	accept callback. LpstpolProxyInfo pointer should be passed as accept callback's argument to pollisten call then inside the accept
	callback cast the argument to LpstProxyInfo to fetch the IP address and socket info.

	@param s				Socket info of the newly connected client.
	@param ipaddr			IP address of the newly connected client.
*/
typedef struct _stpolConnectInfo {
	sock_t s;
	char ipaddr[16];
} stpolConnectInfo, * LPstpolConnectInfo;


/**
		Create a new pol base.

		@param cpucorenum		Number of CPU, it's multiplied by 2 so 2 will spawn
								4 thread workers, set it to 0 to automatically get
								the machine's CPU count.
		@param loghandler		The log callback function, it will default to
								OutputDebugString when set to NULL.
		@param logverboseflags	Bit flags of enum class emuelogtype for filtering
								type of log to be shown.
		@param connect2ndbuffer	This is a dynamic buffer storage that will expand as required by the data allocation size needed to be sent. 
		@return					pol base.
*/
polbase * polnewbase(polloghandler loghandler=0, unsigned int logverboseflags=(unsigned int)-1, int connect2ndbuffer=0);

/**
	Start accepting connections.
	@param base					pol base from polnewbase call.
	@param port					Listen port of this server.
	@param acceptcb				Pointer to Accept callback function of type polacceptcb for
								processing client newly connected.
	@param arg					Variable  you want to pass to Accept callback.
*/
void pollisten(polbase* base, unsigned short int port, polacceptcb acceptcb, void* arg=0, char* listenip=NULL);

/**
	Connect this server to another server.

	@param base					pol base from polnewbase call.
	@param ipaddr				Server IP address to connect to.
	@param port					Server port to connect to.
	@param initbuf				Optional initial data to send to server, this can't be NULL and user is responsible in allocating memory for this variable.
	@param initlen				Size of optional data to send to server, set to 0 if initbuf has no data.
	@param ctx					User provided pointer to POL_PS_CTX struct, this is required if polenablecustomcontext has been called.
	@return						-1 on failure and pol event id on success.
*/
int polconnect(polbase* base, const char* ipaddr, unsigned short int port, char *initbuf, int initlen, LPPOL_PS_CTX ctx=NULL);

/**
	Make the connection setup only but don't connect.
	@param base					pol base from polnewbase call.
	@param ipaddr				Server IP address to connect to.
	@param port					Server port to connect to.
	@param ctx					User provided pointer to POL_PS_CTX struct, this is required if polenablecustomcontext has been called.
	@return						-1 on failure and pol event id on success.
*/
int polmakeconnect(polbase* base, const char* ipaddr, unsigned short int port, LPPOL_PS_CTX ctx=NULL);

/**
	Connect to another server based on the parameters of polmakeconnect call.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id from polmakeconnect call.
	@param initbuf				Optional initial data to send to server, this can't be NULL and user is responsible in allocating memory for this variable.
	@param initlen				Size of optional data to send to server, set to 0 if initbuf has no data.
	@return						true on success and false on failure.
*/
bool polconnect(polbase* base, int eventid, char* initbuf, int initlen);

/**
	Check if the call to polconnect really made, call to polconnect will return true immediately if there has no socket error but the IO is still
	pending, calling polisconnected will get the real connection status. 
	@param base					pol base from polnewbase call.
	@param event_id				pol event id from polmakeconnect call.
	@return						true if connected and false if not.
*/
bool polisconnected(polbase* base, int eventid);

/**
	Start mueevent, this is a blocking call as it will loop continuesly to poll for socket events.
	@param base					pol base from polnewbase call.
*/
void poldispatch(polbase* base);

/**
	Set the read and event callback of pol object.

	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param readcb				Read callback function of type polreadcb for
								processing received data.
	@param eventcb				Event callback of type poleventcb for processing
								connection status.
	@param arg					Variable you want to pass to read and event callback.
*/
void polsetcb(polbase* base, int event_id, polreadcb readcb, poleventcb eventcb, void* arg = NULL);

/**
	Set the read and event callback of pol object.
	@param base					pol base from polnewbase call.
	@param arg					Variable you want to pass to accept callback.
*/
void polsetacceptcbargument(polbase* base, void* arg);

/**
	Set the read and event callback argument.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param arg					Variable you want to pass to accept callback.
*/
void polsetreadeventcbargument(polbase* base, int event_id, void* arg);


/**
	Attempt and immediate sending of data, if not possible unsent data will be moved to dynamic buffer
	for later sending.

	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param lpMsg				Data to send.
	@param dwSize				Size of data to be sent
	@return						false when failed otherwise true if the attempt is successfull
*/
bool polwrite(polbase* base, int event_id, unsigned char* lpMsg, unsigned int dwSize);

/**
	Read received data, call this inside the read callback.

	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param buffer				Store the data to be read
	@param dwSize				Size of data to be read
	@return						Size of data read otherwise 0 when there is error.
*/
size_t polread(polbase* base, int event_id, char* buffer, size_t buffersize);

/**
	Return poldispatch blocking call.
	@param base					pol base from polnewbase call.
*/
void poldispatchbreak(polbase* base);

/**
	Log a message and categorize it with enum class emuelogtype.
	@param mue					pol object.
	@param type					Log category from enum class emuelogtype
	@param msg					Log message.
*/
void poladdlog(polbase* base, epollogtype type, const char* msg, ...);


/**
	Stop pol and clean all the memory used.
	@param base					pol base from polnewbase call.
*/
void polbasedelete(polbase* base);


/**
	Close pol object socket connection.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
*/
void polclosesocket(polbase* base, int event_id);

/**
	Return pol object's IP address.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param ipaddr				user allocated buffer storage for IP address, size 16
*/
void polgetipaddr(polbase* base, int event_id, char* ipaddr);

/**
	Return pol object's socket.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@return						socket on success and -1 on failure.
*/
sock_t polgetsocket(polbase* base, int event_id);

/**
	Set a user index to pol context.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param userindex			User index to set in pol's context.
*/
void polsetindex(polbase* base, int event_id, intptr_t userindex);

/**
	Get the user index from pol context.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@return						-1 if not set or the user index when set.
*/
intptr_t polgetindex(polbase* base, int event_id);

/**
	Check if pol event id is valid.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@return						true if event id is valid otherwise false.
*/
bool polisvalid(polbase* base, int event_id);

/**
	Set the user custom argument.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param arg					Custom argument to pass on event id.
*/
bool polsetcustomarg(polbase* base, int event_id, void* arg);

/**
	Get the user custom argument.
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@return						Pointer to argument on success or 0 on failure.
*/
void* polgetcustomarg(polbase* base, int event_id);

/**
	Enable the user to provide a pointer to POL_PS_CTX struct.
	@param base					pol base from polnewbase call.
*/
void polenablecustomcontext(polbase* base);

/**
	Set the user provided POL_PS_CTX struct pointer with the event id, this can only be called inside accept callback. 
	@param base					pol base from polnewbase call.
	@param event_id				pol event id.
	@param ctx					User created pointer to POL_PS_CTX struct.
	@return						true on success or false on failure.
*/
bool polsetcustomcontext(polbase* base, int event_id, LPPOL_PS_CTX ctx);

/**
	Delete the user provided POL_PS_CTX struct pointer, this must be called by the user upon exit. 
	@param base					pol base from polnewbase call.
	@param ctx					User created pointer to POL_PS_CTX struct.
*/
void poldelcustomcontext(polbase* base, LPPOL_PS_CTX ctx);