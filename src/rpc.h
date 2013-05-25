// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_H
#define BITCOIN_RPC_H

#include "json/json_spirit.h"

void ThreadRPCServer(void* parg);
json_spirit::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);
void RPCConvertValues(const std::string &strMethod, json_spirit::Array &params);
int CommandLineRPC(int argc, char *argv[]);
json_spirit::Value ExecuteRPC(const std::string &strMethod, const std::vector<std::string> &vParams);
json_spirit::Value ExecuteRPC(const std::string &strMethod, const json_spirit::Array &params);

#endif
