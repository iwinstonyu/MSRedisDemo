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

RedisMgr::RedisMgr(string ip, int port, RedisLogger logger)
	: ip_(ip)
	, port_(port)
	, logger_(logger)
	, ctx_(nullptr)
{
	assert(logger_);
}

RedisMgr::~RedisMgr()
{

}

bool RedisMgr::Connect()
{
	if (ctx_) {
		ostringstream oss;
		oss << "Reset and reconnect";
		redisFree(ctx_);
		ctx_ = nullptr;
	}

	ctx_ = redisConnect(ip_.c_str(), port_);
	if (!ctx_) {
		ostringstream oss;
		oss << "Connect null ctx";
		logger_(oss);
		return false;
	}

	if (ctx_->err) {
		ostringstream oss;
		oss << "Connect error: " << ctx_->err << " " << ctx_->errstr;
		logger_(oss);
		return false;
	}

	return true;
}

bool RedisMgr::IsConnecting()
{
	return ctx_ ? true : false;
}

}