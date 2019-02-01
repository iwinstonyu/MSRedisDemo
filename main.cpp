#include <iostream>
#include <string>
#include "RedisMgr.h"
using namespace std;

/*
	»ùÓÚÎ¢Èíredis Win2.6.8
	µØÖ·£ºhttps://github.com/MicrosoftArchive/redis.git
*/

int main()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);

	redisContext* ctx = redisConnect("127.0.0.1", 6379);
	if(!ctx)
		return -1;

	if(ctx->err){
		cout << "Connect error: " << ctx->errstr << endl;
		return -1;
	}

	redisReply* reply = (redisReply*)redisCommand(ctx, "subscribe foo");
	freeReplyObject(reply);
	while(redisGetReply(ctx, (void**)&reply) == REDIS_OK){
		if(reply->type == REDIS_REPLY_STRING ){
			cout << "Reply: " << reply->str << endl;
		}
		else{
			cout << "Reply not string" << endl;
		}
		freeReplyObject(reply);
	}

// 	string input;
// 	while(true){
// 		input = "";
// 		getline(cin, input);
// 
// 		if(input.empty())
// 			break;
// 
// 		redisReply* reply = (redisReply*)redisCommand(ctx, input.c_str());
// 		if(reply){
// 			if(reply->type == REDIS_REPLY_NIL ){
// 				cout << "Reply: nil" << endl;
// 			}
// 			else{
// 				cout << reply->str << endl;
// 			}
// 
// 			freeReplyObject(reply);
// 		}
// 	}

	redisFree(ctx);
	WSACleanup();

	return 0;
}