// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_H
#define BITCOIN_RPC_H

#include "json/json_spirit.h"

#include <map>
#include <list>
#include <set>
#include <string>
#include <vector>

void ThreadRPCServer(void* parg);
json_spirit::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);
void RPCConvertValues(const std::string &strMethod, json_spirit::Array &params);
int CommandLineRPC(int argc, char *argv[]);
json_spirit::Value ExecuteRPC(const std::string &strMethod, const std::vector<std::string> &vParams);
json_spirit::Value ExecuteRPC(const std::string &strMethod, const json_spirit::Array &params);

/*
  Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
  the right number of arguments are passed, just that any passed are the correct type.
  Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
*/
void RPCTypeCheck(const json_spirit::Array& params,
                  const std::list<json_spirit::Value_type>& typesExpected, bool fAllowNull=false);
/*
  Check for expected keys/value types in an Object.
  Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
*/
void RPCTypeCheck(const json_spirit::Object& o,
                  const std::map<std::string, json_spirit::Value_type>& typesExpected, bool fAllowNull=false);

extern std::string HelpRequiringPassphrase();
extern void EnsureWalletIsUnlocked();

typedef json_spirit::Value(*rpcfn_type)(const json_spirit::Array& params, bool fHelp);
extern std::map<std::string, rpcfn_type> mapCallTable;
extern std::set<std::string> setCallAsync;

// Bitcoin RPC error codes
enum RPCErrorCode
{
    // Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST  = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

    // General application defined errors
    RPC_MISC_ERROR                  = -1,  // std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  // Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR                  = -3,  // Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY      = -5,  // Invalid address or key
    RPC_OUT_OF_MEMORY               = -7,  // Ran out of memory during operation
    RPC_INVALID_PARAMETER           = -8,  // Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR              = -20, // Database error
    RPC_DESERIALIZATION_ERROR       = -22, // Error parsing or validating structure in raw format

    // P2P client errors
    RPC_CLIENT_NOT_CONNECTED        = -9,  // Bitcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10, // Still downloading initial blocks

    // Wallet errors
    RPC_WALLET_ERROR                = -4,  // Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  // Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11, // Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12, // Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13, // Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, // The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE      = -15, // Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED    = -16, // Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED     = -17, // Wallet is already unlocked

    // Async method call interrupted.
    RPC_ASYNC_INTERRUPT             = -100,
    // Daemon in warm-up phase.
    RPC_IN_WARMUP                   = -101,
};

/* Keep track of current "warmup status".  This is set to a descriptive
   string while initialising everything, and NULL if the RPC process
   can function as normal.  */
extern const char* rpcWarmupStatus;

#endif
