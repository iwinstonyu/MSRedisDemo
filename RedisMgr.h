//
//	<one line to give the program's name and a brief idea of what it does.>
//	Copyright (C) 2019. WenJin Yu. windpenguin@gmail.com.
//
//	Created at 2019/1/31 19:49:52
//	Version 1.0
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _REDIS_MGR_H_
#define _REDIS_MGR_H_

#include <string>
#include <hiredis/hiredis.h>
#include <functional>
#include <sstream>
#include <vector>
extern "C"{
#include "win32fixes.h"
}
#include "SingleQueueStd.h"
using namespace std;

/*
	没有做持久化缓存
	发送失败没有做额外处理
	后续：考虑广播通知服务器在线
*/

namespace lp {

enum ELogLev
{
	eLogLev_Inf = 0,
	eLogLev_Err = 1
};

#define MAX_REDIS_MSG_LENGTH 10240

typedef function<void(ELogLev logLev, ostringstream& oss)> RedisLogger;
typedef shared_ptr<redisReply> RedisReplyRef;

string RedisMakeChannel(int worldId);
void ParseRedisReply(ostringstream& oss, int prefixamount, redisReply* reply);

struct RedisMsg
{
	struct RedisMsgHead
	{
		unsigned short msgSize_;
		unsigned short msgType_;
		unsigned short senderId_;
		unsigned short recverId_;
	};

	RedisMsg();
	unsigned short MsgType() { return head_.msgType_; }
	unsigned short SenderId() { return head_.senderId_; }
	unsigned short RecverId() { return head_.recverId_; }

	const char* ProtoBuf() { return buf_+sizeof(RedisMsgHead); }
	unsigned short ProtoSize() { return head_.msgSize_-sizeof(RedisMsgHead); }
	const char* MsgBuf() { return buf_; }
	unsigned short MsgSize() { return head_.msgSize_; }

	bool Init(unsigned short msgType, unsigned short senderId, unsigned short recverId, const char* protobuf, unsigned short len);
	bool Init(const char* buf, unsigned short len);

	union
	{
		char buf_[MAX_REDIS_MSG_LENGTH];
		RedisMsgHead head_;
	};
};
typedef shared_ptr<RedisMsg> RedisMsgRef;

class RedisConnector
{
public:
	RedisConnector(string name, string ip, int port, RedisLogger logger);
	~RedisConnector();

	bool Connect();
	bool IsConnecting();
	void Disconnect();

	redisReply* ExecCmd(string cmd);
	redisReply* GetReply();

	bool Subscribe(vector<string>& channels);
	bool Publish(RedisMsgRef msgRef);

protected:
	RedisLogger logger_;
	string name_;

private:
	string ip_;
	int port_;
	redisContext* ctx_;
};

DWORD WINAPI RedisSubscriberWorker(LPVOID lpParam);

class RedisSubscriber : public RedisConnector
{
public:
	RedisSubscriber(string ip, int port, RedisLogger logger);
	
	void Start();
	void Worker();
	void AddChannel(string channel);
	RedisMsgRef ReadMsg();

private:
	vector<string> channels_;
	SingleQueueStd<RedisMsg> msgs_;
};

DWORD WINAPI RedisPublisherWorker(LPVOID lpParam);

class RedisPublisher : public RedisConnector
{
public:
	RedisPublisher(string ip, int port, RedisLogger logger);

	void Start();
	void Worker();
	void SendMsg(RedisMsgRef msgRef);

private:
	SingleQueueStd<RedisMsg> msgs_;
	CRITICAL_SECTION cssleep_;
	CONDITION_VARIABLE cvsleep_;
};

class RedisMgr
{
public:
	RedisMgr(string ip, int port, RedisLogger logger, int worldId);
	~RedisMgr();

	void Start();
	void SendMsg(RedisMsgRef msgRef);
	RedisMsgRef ReadMsg();

private:
	string ip_;
	int port_;
	RedisLogger logger_;
	int worldId_;

	RedisSubscriber subscriber_;
	RedisPublisher publisher_;
};

}

#endif