//
//	<one line to give the program's name and a brief idea of what it does.>
//	Copyright (C) 2019. WenJin Yu. windpenguin@gmail.com.
//
//	Created at 2019/1/31 19:50:04
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

#include "RedisMgr.h"
#include <cassert>

namespace lp {

#define REDIS_RECONNECT_INTERVAL 5000

string RedisMakeChannel(int worldId)
{
	ostringstream oss;
	oss << "world-" << worldId;
	return oss.str();
}

void ParseRedisReply(ostringstream& oss, int prefixamount, redisReply* reply)
{
	if (!reply)
		return;

	string prefix;
	for (int i = 0; i < prefixamount; ++i)
		prefix += "	";

	switch (reply->type)
	{
	case REDIS_REPLY_STRING:
	{
		oss << prefix << "str: " << string(reply->str, reply->len) << endl;
	}
	break;
	case REDIS_REPLY_ARRAY:
	{
		for (size_t i = 0; i < reply->elements; ++i) {
			ParseRedisReply(oss, prefixamount + 1, reply->element[i]);
		}
	}
	break;
	case REDIS_REPLY_INTEGER:
	{
		oss << prefix << "int: " << reply->integer << endl;
	}
	break;
	case REDIS_REPLY_NIL:
	{
		oss << prefix << "nil" << endl;
	}
	break;
	case REDIS_REPLY_STATUS:
	{
		oss << prefix << "status: " << string(reply->str, reply->len) << endl;
	}
	break;
	case REDIS_REPLY_ERROR:
	{
		oss << prefix << "error: " << string(reply->str, reply->len) << endl;
	}
	break;
	default:
		break;
	}
}

RedisMsg::RedisMsg()
{
	memset(buf_, 0, sizeof(buf_));
}

bool RedisMsg::Init(unsigned short msgType, unsigned short senderId, unsigned short recverId, const char* protobuf, unsigned short len)
{
	head_.msgType_ = msgType;
	head_.msgSize_ = sizeof(RedisMsgHead) + len;
	head_.senderId_ = senderId;
	head_.recverId_ = recverId;

	assert(head_.msgSize_ < sizeof(buf_));
	memcpy(buf_ + sizeof(RedisMsgHead), protobuf, len);

	return true;
}

bool RedisMsg::Init(const char* buf, unsigned short len)
{
	if (len < sizeof(RedisMsgHead) || len >= MAX_REDIS_MSG_LENGTH)
		return false;

	memcpy(buf_, buf, len);

	if (!head_.msgType_)
		return false;

	if (head_.msgSize_ != len)
		return false;

	return true;
}

RedisConnector::RedisConnector(string name, string ip, int port, RedisLogger logger)
	: name_(name)
	, ip_(ip)
	, port_(port)
	, logger_(logger)
	, ctx_(nullptr)
{
	assert(logger_);
}

RedisConnector::~RedisConnector()
{
	if (IsConnecting())
		Disconnect();
}

bool RedisConnector::Connect()
{
	if (IsConnecting())
		Disconnect();

	ctx_ = redisConnect(ip_.c_str(), port_);
	if (!ctx_) {
		ostringstream oss;
		oss << name_ << " Connect null ctx";
		logger_(eLogLev_Err, oss);

		return false;
	}

	if (ctx_->err) {
		ostringstream osslog;
		osslog << name_ << " Connect error: " << ctx_->err << " " << ctx_->errstr;
		logger_(eLogLev_Err, osslog);

		redisFree(ctx_);
		ctx_ = nullptr;

		return false;
	}

	ostringstream osslog;
	osslog << name_ << " Connect to server succ: " << ip_ << ":" << port_;
	logger_(eLogLev_Inf, osslog);
	return true;
}

bool RedisConnector::IsConnecting()
{
	return ctx_ ? true : false;
}

void RedisConnector::Disconnect()
{
	if (ctx_) {
		ostringstream osslog;
		osslog << name_ << " Disconnect to server";
		logger_(eLogLev_Err, osslog);

		redisFree(ctx_);
		ctx_ = nullptr;
	}
	else {
		ostringstream osslog;
		osslog << name_ << " Disconnect but not connecting";
		logger_(eLogLev_Err, osslog);
	}
}

redisReply* RedisConnector::ExecCmd(string cmd)
{
	if (!IsConnecting()) {
		ostringstream osslog;
		osslog << "ExecCmd not connecting: " << cmd;
		logger_(eLogLev_Err, osslog);

		return nullptr;
	}

	redisReply* reply = (redisReply*)redisCommand(ctx_, cmd.c_str());
	if (!reply) {
		ostringstream osslog;
		osslog << name_ << " ExecCmd no reply: " << cmd;
		logger_(eLogLev_Err, osslog);

		Disconnect();
	}
	return reply;
}

redisReply* RedisConnector::GetReply()
{
	redisReply* reply = nullptr;
	int res = redisGetReply(ctx_, (void**)&reply);
	if (res != REDIS_OK) {
		ostringstream osslog;
		osslog << name_ << " fail get reply: " << res;
		logger_(eLogLev_Err, osslog);

		Disconnect();
	}
	return reply;
}

bool RedisConnector::Subscribe(vector<string>& channels)
{
	for (auto it = channels.begin(); it != channels.end(); ++it) {
		ostringstream oss;
		oss << "SUBSCRIBE " << *it;

		redisReply* reply = ExecCmd(oss.str());
		if (!reply) {
			Disconnect();
			return false;
		}

		ostringstream osslog;
		osslog << "Subscribe reply: " << endl;
		ParseRedisReply(osslog, 0, reply);
		logger_(eLogLev_Inf, osslog);

		freeReplyObject(reply);
	}
	return true;
}

bool RedisConnector::Publish(RedisMsgRef msgRef)
{
	if (!IsConnecting())
		return false;

	string recvChannel = RedisMakeChannel(msgRef->RecverId());
	redisReply* reply = (redisReply*)redisCommand(ctx_, "PUBLISH %b %b", recvChannel.c_str(), recvChannel.length(), msgRef->MsgBuf(), msgRef->MsgSize());
	if (!reply) {
		ostringstream osslog;
		osslog << "Publish no reply: " << msgRef->MsgType();
		logger_(eLogLev_Err, osslog);

		Disconnect();
		return false;
	}

	if (reply->type == REDIS_REPLY_INTEGER) {
		if (reply->integer == 0) {
			ostringstream osslog;
			osslog << "Publish to zero world: " << msgRef->MsgType() << ", maybe receiver not online" << endl;
			ParseRedisReply(osslog, 0, reply);
			logger_(eLogLev_Err, osslog);

			freeReplyObject(reply);
			return false;
		}
		else if (reply->integer > 1 && msgRef->RecverId() ) {
			ostringstream osslog;
			osslog << "Publish to more than one world: " << msgRef->MsgType() << ", more than one server in same receiver id" << endl;
			ParseRedisReply(osslog, 0, reply);
			logger_(eLogLev_Err, osslog);

			freeReplyObject(reply);
			return false;
		}
	}
	else {
		ostringstream osslog;
		osslog << "Publish fail: " << msgRef->MsgType() << endl;
		ParseRedisReply(osslog, 0, reply);
		logger_(eLogLev_Err, osslog);

		freeReplyObject(reply);
		Disconnect();
		return false;
	}

	freeReplyObject(reply);
	return true;
}

// RedisSubscriber

DWORD WINAPI RedisSubscriberWorker(LPVOID lpParam)
{
	RedisSubscriber* subscriber = reinterpret_cast<RedisSubscriber*>(lpParam);
	assert(subscriber);
	subscriber->Worker();
	return 0;
}

RedisSubscriber::RedisSubscriber(string ip, int port, RedisLogger logger)
	: RedisConnector("RedisSubscriber", ip, port, logger)
{

}

void RedisSubscriber::Start()
{
	DWORD dwThreadID = 0;
	HANDLE hThread = CreateThread(NULL, 0, RedisSubscriberWorker, this, 0, &dwThreadID);
}

void RedisSubscriber::Worker()
{
	while (true) {
		if (!IsConnecting()) {
			Connect();

			if (!IsConnecting()) {
				Sleep(REDIS_RECONNECT_INTERVAL);
				continue;
			}

			if (!Subscribe(channels_)) {
				Sleep(REDIS_RECONNECT_INTERVAL);
				continue;
			}
		}

		redisReply* reply = GetReply();
		if (!reply) {
			Sleep(REDIS_RECONNECT_INTERVAL);
			continue;
		}

		bool ignore = false;

		if (reply->type != REDIS_REPLY_ARRAY && reply->elements != 3)
			ignore = true;

		if (!ignore) {
			redisReply* elementpb = reply->element[2];

			if (elementpb->type != REDIS_REPLY_STRING)
				ignore = true;

			if (!ignore) {
				RedisMsgRef msgRef(new RedisMsg);
				if (!msgRef->Init(elementpb->str, elementpb->len))
					ignore = true;

				if(!ignore)
					msgs_.Write(msgRef);
			}
		}

		if (ignore) {
			ostringstream osslog;
			osslog << "Ignore subscribe msg: " << endl;
			ParseRedisReply(osslog, 0, reply);
			logger_(eLogLev_Err, osslog);
		}

		freeReplyObject(reply);
	}
}

void RedisSubscriber::AddChannel(string channel)
{
	channels_.push_back(channel);
}

RedisMsgRef RedisSubscriber::ReadMsg()
{
	return msgs_.Read();
}

// RedisPublisher

DWORD WINAPI RedisPublisherWorker(LPVOID lpParam)
{
	RedisPublisher* publisher = reinterpret_cast<RedisPublisher*>(lpParam);
	assert(publisher);
	publisher->Worker();
	return 0;
}

RedisPublisher::RedisPublisher(string ip, int port, RedisLogger logger)
	: RedisConnector("RedisPublisher", ip, port, logger)
{
	InitializeCriticalSection(&cssleep_);
	InitializeConditionVariable(&cvsleep_);
}

void RedisPublisher::Start()
{
	DWORD dwThreadID = 0;
	HANDLE hThread = CreateThread(NULL, 0, RedisPublisherWorker, this, 0, &dwThreadID);
}

void RedisPublisher::Worker()
{
	while (true) {
		if (!IsConnecting()) {
			Connect();

			if (!IsConnecting()) {
				Sleep(REDIS_RECONNECT_INTERVAL);
				continue;
			}
		}

		RedisMsgRef msgRef = msgs_.Read();
		if (msgRef) {
			Publish(msgRef);
		}
		else {
			EnterCriticalSection(&cssleep_);
			SleepConditionVariableCS(&cvsleep_, &cssleep_, 10000);
			LeaveCriticalSection(&cssleep_);
		}
	}
}

void RedisPublisher::SendMsg(RedisMsgRef msgRef)
{
	msgs_.Write(msgRef);
	WakeConditionVariable(&cvsleep_);
}

RedisMgr::RedisMgr(string ip, int port, RedisLogger logger, int worldId)
	: ip_(ip)
	, port_(port)
	, logger_(logger)
	, subscriber_(ip, port, logger)
	, publisher_(ip, port, logger)
	, worldId_(worldId)
{
	assert(!ip.empty());
	assert(port > 0);
	assert(logger_);
	assert(worldId_ > 0);

	ostringstream osslog;
	osslog << "Create redis mgr: " << ip_ << ":" << port_ << " " << worldId_;
	logger_(eLogLev_Inf, osslog);

	subscriber_.AddChannel(RedisMakeChannel(0));
	subscriber_.AddChannel(RedisMakeChannel(worldId_));
}

RedisMgr::~RedisMgr()
{
}

void RedisMgr::Start()
{
	subscriber_.Start();
	publisher_.Start();
}

void RedisMgr::SendMsg(RedisMsgRef msgRef)
{
	publisher_.SendMsg(msgRef);
}

RedisMsgRef RedisMgr::ReadMsg()
{
	return subscriber_.ReadMsg();
}

}