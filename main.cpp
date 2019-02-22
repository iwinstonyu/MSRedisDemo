#include <iostream>
#include <string>
#include "RedisMgr.h"
#include <ctime>
#include <fstream>
#include "Bank.pb.h"
#include "MsWrapper.h"
using namespace std;
using namespace lp;

/*
	基于微软redis Win2.6.8 vs2010
	地址：https://github.com/MicrosoftArchive/redis.git
*/

CRITICAL_SECTION cslog;		///< log加锁
string glogFileName = "";

void GlobalLogger(ELogLev logLev, ostringstream& oss)
{
	string prefix = "INF: ";
	switch (logLev)
	{
	case eLogLev_Err:
		prefix = "ERR: ";
		break;
	default:
		break;
	}

	EnterCriticalSection(&cslog);
	cout << prefix << oss.str() << endl;
	
	ofstream ofs(glogFileName, ios::app);
	if (ofs) {
		ofs << prefix << oss.str() << endl;
		ofs.close();
	}
	LeaveCriticalSection(&cslog);
}

void TestRedisMgr(int worldId, int worldAmount, clock_t sendMsgInterval, string ip, int port)
{
	lp::RedisMgr redisMgr(ip, port, GlobalLogger, worldId);

	redisMgr.Start();

	lpb::MsgType sendMsgType = lpb::MsgType_None;
	clock_t sendMsgTime = 0;
	int recvWorldId = 0;

	while (true) {
		while (!sendMsgTime) {
			recvWorldId = rand() % worldAmount;
			if (recvWorldId == worldId) {
				recvWorldId = 0;
				continue;
			}

			sendMsgType = static_cast<lpb::MsgType>(lpb::MsgType_MIN + 1 + rand() % lpb::MsgType_MAX);
			sendMsgTime = clock() + rand() % sendMsgInterval;
		}

		if (sendMsgTime && sendMsgTime < clock()) {
			sendMsgTime = 0;
			::google::protobuf::Message* pbmsg = nullptr;
			switch (sendMsgType) 
			{
			case lpb::MsgType_Borrow:
			{
				lpb::BorrowMoney* detailmsg = new lpb::BorrowMoney;
				if (detailmsg) {
					detailmsg->set_amount(rand()%100);
					detailmsg->set_info("borrow money");
				}
				pbmsg = detailmsg;
			}
			break;
			case lpb::MsgType_Return:
			{
				lpb::ReturnMoney* detailmsg = new lpb::ReturnMoney;
				if (detailmsg) {
					detailmsg->set_amount(rand() % 100);
					detailmsg->set_info("return money");
				}
				pbmsg = detailmsg;
			}
			break;
			default:
				break;
			}

			if (pbmsg) {
				string strmsg;
				if (pbmsg->SerializeToString(&strmsg)) {
					RedisMsgRef msgRef(new RedisMsg);
					msgRef->Init(sendMsgType, worldId, recvWorldId, strmsg.c_str(), strmsg.length());
					redisMgr.SendMsg(msgRef);

					ostringstream osslog;
					osslog << "Send msg: " << pbmsg->GetTypeName() << " " << sendMsgType << " to " << recvWorldId << endl;
					osslog << pbmsg->DebugString() << endl;
					GlobalLogger(eLogLev_Inf, osslog);
				}

				delete pbmsg;
			}
		}

		RedisMsgRef msgRef;
		while ((msgRef = redisMgr.ReadMsg())) {
			if (msgRef->SenderId() == worldId)
				continue;

			::google::protobuf::Message* pbmsg = nullptr;
			switch (msgRef->MsgType()) 
			{
			case lpb::MsgType_Borrow:
			{
				pbmsg = new lpb::BorrowMoney;
			}
			break;
			case lpb::MsgType_Return:
			{
				pbmsg = new lpb::ReturnMoney;
			}
			break;
			default:
				ostringstream osslog;
				osslog << "Ignore subscribe msg: " << endl;
				GlobalLogger(eLogLev_Err, osslog);
				break;
			}

			if (pbmsg) {
				if (pbmsg->ParseFromArray(msgRef->ProtoBuf(), msgRef->ProtoSize())) {
					ostringstream osslog;
					osslog << "Recv msg: " << msgRef->MsgType() << " from " << msgRef->SenderId() << endl;
					osslog << pbmsg->DebugString() << endl;
					GlobalLogger(eLogLev_Inf, osslog);
				}

				delete pbmsg;
			}
		}

		Sleep(200);
	}
}

void SimpleTest1()
{
	redisContext* ctx = redisConnect("127.0.0.1", 6379);
	if (!ctx)
		return;

	if (ctx->err) {
		cout << "Connect error: " << ctx->errstr << endl;
		return;
	}

	redisReply* reply = (redisReply*)redisCommand(ctx, "subscribe foo");
	freeReplyObject(reply);
	while (redisGetReply(ctx, (void**)&reply) == REDIS_OK) {
		if (reply->type == REDIS_REPLY_STRING) {
			cout << "Reply: " << reply->str << endl;
		}
		else {
			cout << "Reply not string" << endl;
		}
		freeReplyObject(reply);
	}

	redisFree(ctx);
}

void SimpleTest2()
{
	redisContext* ctx = redisConnect("127.0.0.1", 6379);
	if (!ctx)
		return;

	if (ctx->err) {
		cout << "Connect error: " << ctx->errstr << endl;
		return;
	}

	string input;
	while (true) {
		input = "";
		getline(cin, input);

		if (input.empty())
			break;

		redisReply* reply = (redisReply*)redisCommand(ctx, input.c_str());
		if (reply) {
			if (reply->type == REDIS_REPLY_NIL) {
				cout << "Reply: nil" << endl;
			}
			else {
				cout << reply->str << endl;
			}

			freeReplyObject(reply);
		}
	}

	redisFree(ctx);
}

int main(int argc, char* argv[])
{
	if (argc != 6)
		return -1;

	int worldId = atoi(argv[1]);
	if (worldId < 0)
		return -1;

	int worldAmount = atoi(argv[2]);
	if (worldAmount < 0 || worldAmount < worldId)
		return -1;

	clock_t sendMsgInterval = atoi(argv[3]);
	if (sendMsgInterval < 0)
		return -1;

	string ip = argv[4];
	int port = atoi(argv[5]);
	if (port < 0)
		return -1;

	ostringstream osslogfile;
	osslogfile << "log_" << worldId << "_" << time(nullptr) << ".txt";
	glogFileName = osslogfile.str();

	InitializeCriticalSection(&cslog);

	srand(clock());

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);

	TestRedisMgr(worldId, worldAmount, sendMsgInterval, ip, port);

	WSACleanup();
	
	system("pause");
	return 0;
}