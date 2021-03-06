/*
	SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
	http://www.audiosynth.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _SC_ComPort_
#define _SC_ComPort_

#include <sys/types.h>
#include <sys/socket.h>
#include "SC_Msg.h"
#include "SC_Sem.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////

class SC_CmdPort
{
protected:
	pthread_t mThread;

	void Start();
	virtual ReplyFunc GetReplyFunc()=0;
public:
	SC_CmdPort();

	virtual void* Run()=0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////

class SC_ComPort : public SC_CmdPort
{
protected:
	int mPortNum;
	int mSocket;
	struct sockaddr_in mBindSockAddr;

public:
	SC_ComPort(int inPortNum);
	virtual ~SC_ComPort();
	
	int Socket() { return mSocket; }
        
	int PortNum() const { return mPortNum; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////

class SC_UdpInPort : public SC_ComPort
{
protected:
	struct sockaddr_in mReplySockAddr;
	virtual ReplyFunc GetReplyFunc();
	
public:
	SC_UdpInPort(int inPortNum);
	~SC_UdpInPort();

	int PortNum() const { return mPortNum; }

	void* Run();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////

class SC_TcpInPort : public SC_ComPort
{
        SC_Semaphore mConnectionAvailable;
        int mBacklog;

protected:
	virtual ReplyFunc GetReplyFunc();
	
public:
	SC_TcpInPort(int inPortNum, int inMaxConnections, int inBacklog);

        virtual void* Run();
        
        void ConnectionTerminated();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////

class SC_TcpConnectionPort : public SC_ComPort
{
        SC_TcpInPort *mParent;

protected:
	virtual ReplyFunc GetReplyFunc();

public:
	SC_TcpConnectionPort(SC_TcpInPort *inParent, int inSocket);
        virtual ~SC_TcpConnectionPort();
        
        virtual void* Run();
};

const int kTextBufSize = 8192;

//////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif

