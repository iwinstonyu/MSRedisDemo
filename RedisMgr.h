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
#include "win32fixes.h"
#include <hiredis/hiredis.h>
#include <functional>
#include <sstream>
using namespace std;

namespace lp {

class RedisMgr
{
public:
	typedef function<void(ostringstream& oss)> RedisLogger;

public:
	RedisMgr(string ip, int port, RedisLogger logger);
	~RedisMgr();

	bool Connect();
	bool IsConnecting();


private:
	string ip_;
	int port_;
	RedisLogger logger_;
	redisContext* ctx_;
};

}

#endif