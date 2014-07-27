// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "cryptopp/sha.h"
#include "db.h"
#include "net.h"
#include "init.h"
#include "main.h"
#include "auxpow.h"

#undef printf

#include <boost/asio.hpp>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/thread/thread.hpp>

#include <memory>

#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> SSLStream;
#endif

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include "namecoin.h"

#define printf OutputDebugStringF

// MinGW 3.4.5 gets "fatal error: had to relocate PCH" if the json headers are
// precompiled in headers.h.  The problem might be when the pch file goes over
// a certain size around 145MB.  If we need access to json_spirit outside this
// file, we could use the compiled json_spirit option.

#include "bitcoinrpc.h"

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace json_spirit;

const char* rpcWarmupStatus = "uninitialised";

void ThreadRPCServer2(void* parg);
Value sendtoaddress(const Array& params, bool fHelp);

int64 nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

void ThreadCleanWalletPassphrase(void* parg);

static inline unsigned short GetDefaultRPCPort()
{
    return GetBoolArg("-testnet", false) ? 18336 : 8336;
}

Object JSONRPCError(int code, const string& message)
{
    Object error;
    error.push_back(Pair("code", code));
    error.push_back(Pair("message", message));
    return error;
}


void PrintConsole(const char* format, ...)
{
    char buffer[50000];
    int limit = sizeof(buffer);
    va_list arg_ptr;
    va_start(arg_ptr, format);
    int ret = _vsnprintf(buffer, limit, format, arg_ptr);
    va_end(arg_ptr);
    if (ret < 0 || ret >= limit)
    {
        ret = limit - 1;
        buffer[limit-1] = 0;
    }
    printf("%s", buffer);
#if defined(__WXMSW__) && defined(GUI)
    MyMessageBox(buffer, "Namecoin", wxOK | wxICON_EXCLAMATION);
#else
    fprintf(stdout, "%s", buffer);
#endif
}

std::string HelpRequiringPassphrase()
{
    return pwalletMain->IsCrypted()
        ? "\nrequires wallet passphrase to be set with walletpassphrase first"
        : "";
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void RPCTypeCheck(const Array& params,
                  const list<Value_type>& typesExpected,
                  bool fAllowNull)
{
    unsigned int i = 0;
    BOOST_FOREACH(Value_type t, typesExpected)
    {
        if (params.size() <= i)
            break;

        const Value& v = params[i];
        if (!((v.type() == t) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s, got %s",
                                   Value_type_name[t], Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
        i++;
    }
}

void RPCTypeCheck(const Object& o,
                  const map<string, Value_type>& typesExpected,
                  bool fAllowNull)
{
    BOOST_FOREACH(const PAIRTYPE(string, Value_type)& t, typesExpected)
    {
        const Value& v = find_value(o, t.first);
        if (!fAllowNull && v.type() == null_type)
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", t.first.c_str()));

        if (!((v.type() == t.second) || (fAllowNull && (v.type() == null_type))))
        {
            string err = strprintf("Expected type %s for %s, got %s",
                                   Value_type_name[t.second], t.first.c_str(), Value_type_name[v.type()]);
            throw JSONRPCError(RPC_TYPE_ERROR, err);
        }
    }
}

int64 AmountFromValue(const Value& value)
{
    double dAmount = value.get_real();
    if (dAmount <= 0.0 || dAmount > 21000000.0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    int64 nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

Value ValueFromAmount(int64 amount)
{
    return (double)amount / (double)COIN;
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    entry.push_back(Pair("confirmations", wtx.GetDepthInMainChain()));
    entry.push_back(Pair("txid", wtx.GetHash().GetHex()));
    entry.push_back(Pair("time", (boost::int64_t)wtx.GetTxTime()));
    BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}



///
/// Note: This interface may still be subject to change.
///


Value help(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "help [command]\n"
            "List commands, or get help for a command.");

    string strCommand;
    if (params.size() > 0)
        strCommand = params[0].get_str();

    string strRet;
    set<rpcfn_type> setDone;
    for (map<string, rpcfn_type>::iterator mi = mapCallTable.begin(); mi != mapCallTable.end(); ++mi)
    {
        string strMethod = (*mi).first;
        // We already filter duplicates, but these deprecated screw up the sort order
        if (strMethod == "getamountreceived" ||
            strMethod == "getallreceived" ||
            (strMethod.find("label") != string::npos))
            continue;
        if (strCommand != "" && strMethod != strCommand)
            continue;
        try
        {
            Array params;
            rpcfn_type pfn = (*mi).second;
            if (setDone.insert(pfn).second)
                (*pfn)(params, true);
        }
        catch (std::exception& e)
        {
            // Help text is returned in an exception
            string strHelp = string(e.what());
            if (strCommand == "")
                if (strHelp.find('\n') != -1)
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "")
        strRet = strprintf("help: unknown command: %s\n", strCommand.c_str());
    strRet = strRet.substr(0,strRet.size()-1);
    return strRet;
}


Value stop(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "stop\n"
            "Stop namecoin server.");

    // Shutdown will take long enough that the response should get back
    StartShutdown();
    return "namecoin server stopping";
}


Value getblockcount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblockcount\n"
            "Returns the number of blocks in the longest block chain.");

    return nBestHeight;
}

Value getblockhash(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockhash <index>\n"
            "Returns hash of block in best-block-chain at <index>.");

    int nHeight = params[0].get_int();
    if (nHeight < 0 || nHeight > nBestHeight)
        throw runtime_error("Block number out of range.");

    CBlockIndex* pblockindex = FindBlockByHeight(nHeight);
    return pblockindex->phashBlock->GetHex();
}

Value getblocknumber(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getblocknumber\n"
            "Returns the block number of the latest block in the longest block chain.");

    return nBestHeight;
}

static double
GetDifficulty (unsigned int nBits)
{
    // Floating point number that is a multiple of the minimum difficulty,
    // minimum difficulty = 1.0.

    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

static double
GetDifficulty ()
{
  if (pindexBest == NULL)
    return 1.0;

  return GetDifficulty (pindexBest->nBits);
}

Value BlockToValue(const CBlock &block, const CBlockIndex* blockindex)
{
    Object obj;
    CMerkleTx txGen(block.vtx[0]);
    txGen.SetMerkleBranch(&block);
    obj.push_back(Pair("hash", block.GetHash().ToString().c_str()));
    obj.push_back(Pair("confirmations", (int)txGen.GetDepthInMainChain()));
    obj.push_back(Pair("size", (int)::GetSerializeSize(block, SER_NETWORK)));
    obj.push_back(Pair("height", blockindex->nHeight));
    obj.push_back(Pair("version", block.nVersion));
    obj.push_back(Pair("merkleroot", block.hashMerkleRoot.ToString().c_str()));

    Array tx;
    for (int i = 0; i < block.vtx.size(); i++) {
        tx.push_back(block.vtx[i].GetHash().ToString().c_str());
    }

    obj.push_back(Pair("tx", tx));
    obj.push_back(Pair("n_tx", (int)block.vtx.size()));
    obj.push_back(Pair("time", (uint64_t)block.nTime));
    obj.push_back(Pair("nonce", (uint64_t)block.nNonce));
    obj.push_back(Pair("bits", (uint64_t)block.nBits));
    obj.push_back(Pair("difficulty", GetDifficulty (block.nBits)));
    obj.push_back(Pair("chainwork", blockindex->bnChainWork.GetHex()));

    if (blockindex->pprev)
        obj.push_back(Pair("previousblockhash", block.hashPrevBlock.ToString().c_str()));
    const CBlockIndex *pnext = blockindex->pnext;
    if (pnext)
        obj.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));

    Array mrkl;
    for (int i = 0; i < block.vMerkleTree.size(); i++)
    	mrkl.push_back(block.vMerkleTree[i].ToString().c_str());

    obj.push_back(Pair("mrkl_tree", mrkl));

    return obj;
}

Value getblockbycount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblockbycount height\n"
            "Dumps the block existing at specified height");

    int64 height = params[0].get_int64();
    if (height > nBestHeight)
        throw runtime_error(
            "getblockbycount height\n"
            "Dumps the block existing at specified height");

    string blkname = strprintf("blk%d", height);

    CBlockIndex* pindex;
    bool found = false;

    for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin();
         mi != mapBlockIndex.end(); ++mi)
    {
    	pindex = (*mi).second;
	if ((pindex->nHeight == height) && (pindex->IsInMainChain())) {
		found = true;
		break;
	}
    }

    if (!found)
        throw runtime_error(
            "getblockbycount height\n"
            "Dumps the block existing at specified height");

    CBlock block;
    block.ReadFromDisk(pindex);
    block.BuildMerkleTree();

    return BlockToValue(block, pindex);
}


Value getblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getblock hash\n"
            "Dumps the block with specified hash");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi == mapBlockIndex.end())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "hash not found");

    CBlockIndex* pindex = (*mi).second;

    CBlock block;
    block.ReadFromDisk(pindex);
    block.BuildMerkleTree();

    return BlockToValue(block, pindex);
}

/* Comparison function for sorting the getchains heads.  */
static bool
compareBlocksByHeight (const uint256& a, const uint256& b)
{
  std::map<uint256, CBlockIndex*>::const_iterator ia, ib;

  ia = mapBlockIndex.find (a);
  ib = mapBlockIndex.find (b);

  assert (ia != mapBlockIndex.end () && ib != mapBlockIndex.end ());

  return (ia->second->nHeight > ib->second->nHeight);
}

/* Return the state of all known chains.  */
static Value
getchains (const Array& params, bool fHelp)
{
  if (fHelp || params.size () != 0)
    throw runtime_error (
      "getchains\n"
      "Return status of all known chains.");

  /* Lock everything.  Not sure if this is needed for the whole duration
     of the call, but better be safe than sorry.  */
  CCriticalBlock lock(cs_main);

  /* For each block known, keep track if there are follow-ups (which have
     the block as pprev) so that we find the chain heads.  */

  std::map<uint256, bool> blockIsHead;
  std::map<uint256, CBlockIndex*>::const_iterator i;

  for (i = mapBlockIndex.begin (); i != mapBlockIndex.end (); ++i)
    blockIsHead.insert (std::make_pair (i->first, true));

  for (i = mapBlockIndex.begin (); i != mapBlockIndex.end (); ++i)
    {
      const CBlockIndex* pprev = i->second->pprev;
      if (!pprev)
        continue;

      const uint256 prevHash = *pprev->phashBlock;
      const std::map<uint256, bool>::iterator j = blockIsHead.find (prevHash);
      assert (j != blockIsHead.end ());
      j->second = false;
    }

  /* Get chain heads and sort them by height.  */

  std::vector<uint256> heads;
  for (std::map<uint256, bool>::const_iterator j = blockIsHead.begin ();
       j != blockIsHead.end (); ++j)
    if (j->second)
      heads.push_back (j->first);

  std::sort (heads.begin (), heads.end (), &compareBlocksByHeight);

  /* Construct the output array.  */

  Array res;
  for (std::vector<uint256>::const_iterator j = heads.begin ();
       j != heads.end (); ++j)
    {
      i = mapBlockIndex.find (*j);
      assert (i != mapBlockIndex.end ());

      const CBlockIndex& block = *i->second;
      assert (*j == *block.phashBlock);

      Object obj;
      obj.push_back (Pair ("height", block.nHeight));
      obj.push_back (Pair ("hash", block.phashBlock->GetHex ()));

      const bool isMain = (&block == pindexBest);
      obj.push_back (Pair ("is_best", isMain));

      /* If the block is not the main head, construct the branch that
         connects it to the main chain.  */
      if (!isMain)
        {
          Array branch;
          int len = 0;

          const CBlockIndex* pcur = &block;
          while (true)
            {
              assert (pcur->pprev);
              pcur = pcur->pprev;

              branch.push_back (pcur->phashBlock->GetHex ());
              ++len;

              /* We are on the main chain if there's a next pointer.  */
              if (pcur->pnext)
                break;
            }

          obj.push_back (Pair ("branch_len", len));
          obj.push_back (Pair ("branch", branch));
        }
      else
        obj.push_back (Pair ("branch_len", 0));

      res.push_back (obj);
    }

  return res;
}

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getconnectioncount\n"
            "Returns the number of connections to other nodes.");

    return (int)vNodes.size();
}

Value getdifficulty(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getdifficulty\n"
            "Returns the proof-of-work difficulty as a multiple of the minimum difficulty.");

    return GetDifficulty();
}


Value getgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "Returns true or false.");

    return (bool)fGenerateBitcoins;
}


Value setgenerate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate <generate> [genproclimit]\n"
            "<generate> is true or false to turn generation on or off.\n"
            "Generation is limited to [genproclimit] processors, -1 is unlimited.");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    if (params.size() > 1)
    {
        int nGenProcLimit = params[1].get_int();
        fLimitProcessors = (nGenProcLimit != -1);
        WriteSetting("fLimitProcessors", fLimitProcessors);
        if (nGenProcLimit != -1)
            WriteSetting("nLimitProcessors", nLimitProcessors = nGenProcLimit);
        if (nGenProcLimit == 0)
            fGenerate = false;
    }

    GenerateBitcoins(fGenerate, pwalletMain);
    return Value::null;
}


Value gethashespersec(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gethashespersec\n"
            "Returns a recent hashes per second performance measurement while generating.");

    if (GetTimeMillis() - nHPSTimerStart > 8000)
        return (boost::int64_t)0;
    return (boost::int64_t)dHashesPerSec;
}


Value getinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getinfo\n"
            "Returns an object containing various state info.");

    Object obj;
    obj.push_back(Pair("version",       (int)VERSION));
    obj.push_back(Pair("balance",       ValueFromAmount(pwalletMain->GetBalance())));
    obj.push_back(Pair("blocks",        (int)nBestHeight));
    obj.push_back(Pair("timeoffset",    (boost::int64_t)GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("proxy",         (fUseProxy ? addrProxy.ToStringIPPort() : string())));
    obj.push_back(Pair("generate",      (bool)fGenerateBitcoins));
    obj.push_back(Pair("genproclimit",  (int)(fLimitProcessors ? nLimitProcessors : -1)));
    obj.push_back(Pair("difficulty",    (double)GetDifficulty()));
    obj.push_back(Pair("hashespersec",  gethashespersec(params, false)));
    obj.push_back(Pair("testnet",       fTestNet));
    obj.push_back(Pair("keypoololdest", (boost::int64_t)pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize",   pwalletMain->GetKeyPoolSize()));
    obj.push_back(Pair("paytxfee",      ValueFromAmount(nTransactionFee)));
    obj.push_back(Pair("mininput",      ValueFromAmount(nMinimumInputValue)));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", (boost::int64_t)nWalletUnlockTime / 1000));
    obj.push_back(Pair("errors",        GetWarnings("statusbar")));
    return obj;
}


Value getnewaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getnewaddress [account]\n"
            "Returns a new Namecoin address for receiving payments.  "
            "If [account] is specified (recommended), it is added to the address book "
            "so payments received with the address will be credited to [account]."
            + std::string(pwalletMain->IsCrypted() ? "\nmay require wallet passphrase to be set with walletpassphrase, if the key pool is empty" : ""));

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);

    // Generate a new key that is added to wallet
    string strAddress = PubKeyToAddress(pwalletMain->GetKeyFromKeyPool());

    // This could be done in the same main CS as GetKeyFromKeyPool.
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
       pwalletMain->SetAddressBookName(strAddress, strAccount);

    return strAddress;
}


// requires cs_main, cs_mapWallet, cs_mapAddressBook locks
string GetAccountAddress(string strAccount, bool bForceNew=false)
{
    string strAddress;

    CWalletDB walletdb(pwalletMain->strWalletFile);
    walletdb.TxnBegin();

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    // Check if the current key has been used
    if (!account.vchPubKey.empty())
    {
        CScript scriptPubKey;
        scriptPubKey.SetBitcoinAddress(account.vchPubKey);
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && !account.vchPubKey.empty();
             ++it)
        {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    account.vchPubKey.clear();
        }
    }

    // Generate a new key
    if (account.vchPubKey.empty() || bForceNew)
    {
        account.vchPubKey = pwalletMain->GetKeyFromKeyPool();
        string strAddress = PubKeyToAddress(account.vchPubKey);
        pwalletMain->SetAddressBookName(strAddress, strAccount);
        walletdb.WriteAccount(strAccount, account);
    }

    walletdb.TxnCommit();
    strAddress = PubKeyToAddress(account.vchPubKey);

    return strAddress;
}

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress <account>\n"
            "Returns the current Namecoin address for receiving payments to this account.");

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        ret = GetAccountAddress(strAccount);
    }

    return ret;
}



Value setaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setaccount <namecoinaddress> <account>\n"
            "Sets the account associated with the given address.");

    string strAddress = params[0].get_str();
    uint160 hash160;
    bool isValid = AddressToHash160(strAddress, hash160);
    if (!isValid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Namecoin address");


    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Detect when changing the account of an address that is the 'unused current key' of another account:
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        if (pwalletMain->mapAddressBook.count(strAddress))
        {
            string strOldAccount = pwalletMain->mapAddressBook[strAddress];
            if (strAddress == GetAccountAddress(strOldAccount))
                GetAccountAddress(strOldAccount, true);
        }

        pwalletMain->SetAddressBookName(strAddress, strAccount);
    }

    return Value::null;
}


Value getaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccount <namecoinaddress>\n"
            "Returns the account associated with the given address.");

    string strAddress = params[0].get_str();

    string strAccount;
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        map<string, string>::iterator mi = pwalletMain->mapAddressBook.find(strAddress);
        if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.empty())
            strAccount = (*mi).second;
    }
    return strAccount;
}


Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbyaccount <account>\n"
            "Returns the list of addresses for the given account.");

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    Array ret;
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        BOOST_FOREACH(const PAIRTYPE(string, string)& item, pwalletMain->mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strName = item.second;
            if (strName == strAccount)
            {
                // We're only adding valid bitcoin addresses and not ip addresses
                CScript scriptPubKey;
                if (scriptPubKey.SetBitcoinAddress(strAddress))
                    ret.push_back(strAddress);
            }
        }
    }
    return ret;
}

Value settxfee(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "settxfee <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.00000001");

    // Amount
    int64 nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts

    nTransactionFee = nAmount;
    return true;
}

Value setmininput(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "setmininput <amount>\n"
            "<amount> is a real and is rounded to the nearest 0.00000001");

    // Amount
    int64 nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts

    nMinimumInputValue = nAmount;
    return true;
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoaddress <namecoinaddress> <amount> [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.01"
            + HelpRequiringPassphrase());

    string strAddress = params[0].get_str();

    // Amount
    int64 nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["comment"] = params[2].get_str();
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["to"]      = params[3].get_str();

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        string strError = pwalletMain->SendMoneyToBitcoinAddress(strAddress, nAmount, wtx);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value listaddressgroupings(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listaddressgroupings\n"
            "Lists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions");

    Array jsonGroupings;
    map<string, int64> balances = pwalletMain->GetAddressBalances();
    BOOST_FOREACH(set<string> grouping, pwalletMain->GetAddressGroupings())
    {
        Array jsonGrouping;
        BOOST_FOREACH(string address, grouping)
        {
            Array addressInfo;
            addressInfo.push_back(address);
            addressInfo.push_back(ValueFromAmount(balances[address]));
            CRITICAL_BLOCK(pwalletMain->cs_wallet)
            {
                map<string, string>::iterator mi = pwalletMain->mapAddressBook.find(address);
                if (mi != pwalletMain->mapAddressBook.end())
                    addressInfo.push_back(mi->second);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaddress <namecoinaddress> [minconf=1]\n"
            "Returns the total amount received by <namecoinaddress> in transactions with at least [minconf] confirmations.");

    // Bitcoin address
    string strAddress = params[0].get_str();
    CScript scriptPubKey;
    if (!scriptPubKey.SetBitcoinAddress(strAddress))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Namecoin address");
    if (!IsMine(*pwalletMain,scriptPubKey))
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    int64 nAmount = 0;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    if (wtx.GetDepthInMainChain() >= nMinDepth)
                        nAmount += txout.nValue;
        }
    }

    return  ValueFromAmount(nAmount);
}


void GetAccountPubKeys(string strAccount, set<CScript>& setPubKey)
{
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        BOOST_FOREACH(const PAIRTYPE(string, string)& item, pwalletMain->mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strName = item.second;
            if (strName == strAccount)
            {
                // We're only counting our own valid bitcoin addresses and not ip addresses
                CScript scriptPubKey;
                if (scriptPubKey.SetBitcoinAddress(strAddress))
                    if (IsMine(*pwalletMain,scriptPubKey))
                        setPubKey.insert(scriptPubKey);
            }
        }
    }
}


Value getreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaccount <account> [minconf=1]\n"
            "Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.");

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys that have the label
    string strAccount = AccountFromValue(params[0]);
    set<CScript> setPubKey;
    GetAccountPubKeys(strAccount, setPubKey);

    // Tally
    int64 nAmount = 0;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (setPubKey.count(txout.scriptPubKey))
                    if (wtx.GetDepthInMainChain() >= nMinDepth)
                        nAmount += txout.nValue;
        }
    }

    return (double)nAmount / (double)COIN;
}


int64 GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth)
{
    int64 nBalance = 0;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        // Tally wallet transactions
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsFinal())
                continue;

            int64 nGenerated, nReceived, nSent, nFee;
            wtx.GetAccountAmounts(strAccount, nGenerated, nReceived, nSent, nFee);

            if (nReceived != 0 && wtx.GetDepthInMainChain() >= nMinDepth)
                nBalance += nReceived;
            nBalance += nGenerated - nSent - nFee;
        }

        // Tally internal accounting entries
        nBalance += walletdb.GetAccountCreditDebit(strAccount);
    }

    return nBalance;
}

int64 GetAccountBalance(const string& strAccount, int nMinDepth)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth);
}


Value getbalance(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 0 || params.size() > 2)
        throw runtime_error(
            "getbalance [account] [minconf=1]\n"
            "If [account] is not specified, returns the server's total available balance.\n"
            "If [account] is specified, returns the balance in the account.");

    if (params.size() == 0)
        return ValueFromAmount(pwalletMain->GetBalance());

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and getbalance '*' 0 should return the same number
        int64 nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (!wtx.IsFinal())
                continue;

            int64 allGeneratedImmature, allGeneratedMature, allFee;
            allGeneratedImmature = allGeneratedMature = allFee = 0;
            string strSentAccount;
            list<pair<string, int64> > listReceived;
            list<pair<string, int64> > listSent;
            bool fNameTx;
            wtx.GetAmounts(allGeneratedImmature, allGeneratedMature, listReceived, listSent, allFee, strSentAccount, fNameTx);
            if (wtx.GetDepthInMainChain() >= nMinDepth)
                BOOST_FOREACH(const PAIRTYPE(string,int64)& r, listReceived)
                    nBalance += r.second;
            BOOST_FOREACH(const PAIRTYPE(string,int64)& r, listSent)
                nBalance -= r.second;
            nBalance -= allFee;
            nBalance += allGeneratedMature;
        }
        return ValueFromAmount(nBalance);
    }

    string strAccount = AccountFromValue(params[0]);

    int64 nBalance = GetAccountBalance(strAccount, nMinDepth);

    return ValueFromAmount(nBalance);
}


Value movecmd(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "move <fromaccount> <toaccount> <amount> [minconf=1] [comment]\n"
            "Move from one account in your wallet to another.");

    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    int64 nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();
    string strComment;
    if (params.size() > 4)
        strComment = params[4].get_str();

    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        CWalletDB walletdb(pwalletMain->strWalletFile);
        walletdb.TxnBegin();

        int64 nNow = GetAdjustedTime();

        // Debit
        CAccountingEntry debit;
        debit.strAccount = strFrom;
        debit.nCreditDebit = -nAmount;
        debit.nTime = nNow;
        debit.strOtherAccount = strTo;
        debit.strComment = strComment;
        walletdb.WriteAccountingEntry(debit);

        // Credit
        CAccountingEntry credit;
        credit.strAccount = strTo;
        credit.nCreditDebit = nAmount;
        credit.nTime = nNow;
        credit.strOtherAccount = strFrom;
        credit.strComment = strComment;
        walletdb.WriteAccountingEntry(credit);

        walletdb.TxnCommit();
    }
    return true;
}


Value sendfrom(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendfrom <fromaccount> <tonamecoinaddress> <amount> [minconf=1] [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.01"
            + HelpRequiringPassphrase());

    string strAccount = AccountFromValue(params[0]);
    string strAddress = params[1].get_str();
    int64 nAmount = AmountFromValue(params[2]);
    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();


    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        EnsureWalletIsUnlocked();

        // Check funds
        int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
        if (nAmount > nBalance)
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

        // Send
        string strError = pwalletMain->SendMoneyToBitcoinAddress(strAddress, nAmount, wtx);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    return wtx.GetHash().GetHex();
}

Value sendmany(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendmany <fromaccount> {address:amount,...} [minconf=1] [comment]\n"
            "amounts are double-precision floating point numbers"
            + HelpRequiringPassphrase());

    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();
    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();

    set<string> setAddress;
    vector<pair<CScript, int64> > vecSend;

    int64 totalAmount = 0;
    BOOST_FOREACH(const Pair& s, sendTo)
    {
        uint160 hash160;
        string strAddress = s.name_;

        if (setAddress.count(strAddress))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+strAddress);
        setAddress.insert(strAddress);

        CScript scriptPubKey;
        if (!scriptPubKey.SetBitcoinAddress(strAddress))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Namecoin address:")+strAddress);
        int64 nAmount = AmountFromValue(s.value_);
        totalAmount += nAmount;

        vecSend.push_back(make_pair(scriptPubKey, nAmount));
    }

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        EnsureWalletIsUnlocked();

        // Check funds
        int64 nBalance = GetAccountBalance(strAccount, nMinDepth);
        if (totalAmount > nBalance)
            throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

        // Send
        CReserveKey keyChange(pwalletMain);
        int64 nFeeRequired = 0;
        bool fCreated = pwalletMain->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);
        if (!fCreated)
        {
            if (totalAmount + nFeeRequired > pwalletMain->GetBalance())
                throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");
            throw JSONRPCError(RPC_WALLET_ERROR, "Transaction creation failed");
        }
        if (!pwalletMain->CommitTransaction(wtx, keyChange))
            throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");
    }

    return wtx.GetHash().GetHex();
}


struct tallyitem
{
    int64 nAmount;
    int nConf;
    tallyitem()
    {
        nAmount = 0;
        nConf = INT_MAX;
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    // Tally
    map<uint160, tallyitem> mapTally;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    CRITICAL_BLOCK(pwalletMain->cs_mapKeys)
    {
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            if (wtx.IsCoinBase() || !wtx.IsFinal())
                continue;

            int nDepth = wtx.GetDepthInMainChain();
            if (nDepth < nMinDepth)
                continue;

            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                // -------------- Only counting our own bitcoin addresses and not ip addresses
                // Now counting all addresses, because name tx can send change to pubkey, rather than hash160
                uint160 hash160 = txout.scriptPubKey.GetBitcoinAddressHash160();
                if (hash160 == 0 || !pwalletMain->mapPubKeys.count(hash160)) // IsMine
                    continue;

                tallyitem& item = mapTally[hash160];
                item.nAmount += txout.nValue;
                item.nConf = min(item.nConf, nDepth);
            }
        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    {
        BOOST_FOREACH(const PAIRTYPE(string, string)& item, pwalletMain->mapAddressBook)
        {
            const string& strAddress = item.first;
            const string& strAccount = item.second;
            uint160 hash160;
            if (!AddressToHash160(strAddress, hash160))
                continue;
            map<uint160, tallyitem>::iterator it = mapTally.find(hash160);
            if (it == mapTally.end() && !fIncludeEmpty)
                continue;

            int64 nAmount = 0;
            int nConf = INT_MAX;
            if (it != mapTally.end())
            {
                nAmount = (*it).second.nAmount;
                nConf = (*it).second.nConf;
            }

            if (fByAccounts)
            {
                tallyitem& item = mapAccountTally[strAccount];
                item.nAmount += nAmount;
                item.nConf = min(item.nConf, nConf);
            }
            else
            {
                Object obj;
                obj.push_back(Pair("address",       strAddress));
                obj.push_back(Pair("account",       strAccount));
                obj.push_back(Pair("label",         strAccount)); // deprecated
                obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
                obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
                ret.push_back(obj);
            }
        }
    }

    if (fByAccounts)
    {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it)
        {
            int64 nAmount = (*it).second.nAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("label",         (*it).first)); // deprecated
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == INT_MAX ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaddress [minconf=1] [includeempty=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include addresses that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"address\" : receiving address\n"
            "  \"account\" : the account of the receiving address\n"
            "  \"amount\" : total amount received by the address\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    return ListReceived(params, false);
}

Value listreceivedbyaccount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listreceivedbyaccount [minconf=1] [includeempty=false]\n"
            "[minconf] is the minimum number of confirmations before payments are included.\n"
            "[includeempty] whether to include accounts that haven't received any payments.\n"
            "Returns an array of objects containing:\n"
            "  \"account\" : the account of the receiving addresses\n"
            "  \"amount\" : total amount received by addresses with this account\n"
            "  \"confirmations\" : number of confirmations of the most recent transaction included");

    return ListReceived(params, true);
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret)
{
    int64 nGeneratedImmature, nGeneratedMature, nFee;
    string strSentAccount;
    list<pair<string, int64> > listReceived;
    list<pair<string, int64> > listSent;
    bool fNameTx;
    wtx.GetAmounts(nGeneratedImmature, nGeneratedMature, listReceived, listSent, nFee, strSentAccount, fNameTx);

    bool fAllAccounts = (strAccount == string("*"));

    // Generated blocks assigned to account ""
    if ((nGeneratedMature+nGeneratedImmature) != 0 && (fAllAccounts || strAccount == ""))
    {
        Object entry;
        entry.push_back(Pair("account", string("")));
        if (nGeneratedImmature)
        {
            entry.push_back(Pair("category", wtx.GetDepthInMainChain() ? "immature" : "orphan"));
            entry.push_back(Pair("amount", ValueFromAmount(nGeneratedImmature)));
        }
        else
        {
            entry.push_back(Pair("category", "generate"));
            entry.push_back(Pair("amount", ValueFromAmount(nGeneratedMature)));
        }
        if (fLong)
            WalletTxToJSON(wtx, entry);
        ret.push_back(entry);
    }

    // Sent
    if ((!listSent.empty() || nFee != 0 || fNameTx) && (fAllAccounts || strAccount == strSentAccount))
    {
        if (listSent.empty() || fNameTx)
        {
            // Name transaction, or some non-standard transaction with non-zero fee
            Object entry;
            entry.push_back(Pair("account", strSentAccount));
            string strAddress;
            if (fNameTx)
            {
                int nTxOut = IndexOfNameOutput(wtx);
                hooks->ExtractAddress(wtx.vout[nTxOut].scriptPubKey, strAddress);
            }
            entry.push_back(Pair("address", strAddress));
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(0)));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
        else
        {
            BOOST_FOREACH(const PAIRTYPE(string, int64)& s, listSent)
            {
                Object entry;
                entry.push_back(Pair("account", strSentAccount));
                entry.push_back(Pair("address", s.first));
                entry.push_back(Pair("category", "send"));
                entry.push_back(Pair("amount", ValueFromAmount(-s.second)));
                entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
        CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
        {
            BOOST_FOREACH(const PAIRTYPE(string, int64)& r, listReceived)
            {
                string account;
                if (pwalletMain->mapAddressBook.count(r.first))
                    account = pwalletMain->mapAddressBook[r.first];
                if (fAllAccounts || (account == strAccount))
                {
                    Object entry;
                    entry.push_back(Pair("account", account));
                    entry.push_back(Pair("address", r.first));
                    entry.push_back(Pair("category", "receive"));
                    entry.push_back(Pair("amount", ValueFromAmount(r.second)));
                    if (fLong)
                        WalletTxToJSON(wtx, entry);
                    ret.push_back(entry);
                }
            }
        }

}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", (boost::int64_t)acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

Value listtransactions(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listtransactions [account] [count=10] [from=0]\n"
            "Returns up to [count] most recent transactions skipping the first [from] transactions for account [account].");

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();

    Array ret;
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        // Firs: get all CWalletTx and CAccountingEntry into a sorted-by-time multimap:
        typedef pair<CWalletTx*, CAccountingEntry*> TxPair;
        typedef multimap<int64, TxPair > TxItems;
        TxItems txByTime;

        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            CWalletTx* wtx = &((*it).second);
            txByTime.insert(make_pair(wtx->GetTxTime(), TxPair(wtx, (CAccountingEntry*)0)));
        }
        list<CAccountingEntry> acentries;
        walletdb.ListAccountCreditDebit(strAccount, acentries);
        BOOST_FOREACH(CAccountingEntry& entry, acentries)
        {
            txByTime.insert(make_pair(entry.nTime, TxPair((CWalletTx*)0, &entry)));
        }

        // Now: iterate backwards until we have nCount items to return:
        TxItems::reverse_iterator it = txByTime.rbegin();
        for (std::advance(it, nFrom); it != txByTime.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != 0)
                ListTransactions(*pwtx, strAccount, 0, true, ret);
            CAccountingEntry *const pacentry = (*it).second.second;
            if (pacentry != 0)
                AcentryToJSON(*pacentry, strAccount, ret);

            if (ret.size() >= nCount) break;
        }
        // ret is now newest to oldest
    }

    // Make sure we return only last nCount items (sends-to-self might give us an extra):
    if (ret.size() > nCount)
    {
        Array::iterator last = ret.begin();
        std::advance(last, nCount);
        ret.erase(last, ret.end());
    }
    std::reverse(ret.begin(), ret.end()); // oldest to newest

    return ret;
}

Value listaccounts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "listaccounts [minconf=1]\n"
            "Returns Object that has account names as keys, account balances as values.");

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    map<string, int64> mapAccountBalances;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
    CRITICAL_BLOCK(pwalletMain->cs_mapKeys)
    {
        BOOST_FOREACH(const PAIRTYPE(string, string)& entry, pwalletMain->mapAddressBook) {
            uint160 hash160;
            if(AddressToHash160(entry.first, hash160) && pwalletMain->mapPubKeys.count(hash160)) // This address belongs to me
                mapAccountBalances[entry.second] = 0;
        }

        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it)
        {
            const CWalletTx& wtx = (*it).second;
            int64 nGeneratedImmature, nGeneratedMature, nFee;
            string strSentAccount;
            list<pair<string, int64> > listReceived;
            list<pair<string, int64> > listSent;
            bool fNameTx;
            wtx.GetAmounts(nGeneratedImmature, nGeneratedMature, listReceived, listSent, nFee, strSentAccount, fNameTx);
            mapAccountBalances[strSentAccount] -= nFee;
            BOOST_FOREACH(const PAIRTYPE(string, int64)& s, listSent)
                mapAccountBalances[strSentAccount] -= s.second;
            if (wtx.GetDepthInMainChain() >= nMinDepth)
            {
                mapAccountBalances[""] += nGeneratedMature;
                BOOST_FOREACH(const PAIRTYPE(string, int64)& r, listReceived)
                    if (pwalletMain->mapAddressBook.count(r.first))
                        mapAccountBalances[pwalletMain->mapAddressBook[r.first]] += r.second;
                    else
                        mapAccountBalances[""] += r.second;
            }
        }
    }

    list<CAccountingEntry> acentries;
    CWalletDB(pwalletMain->strWalletFile).ListAccountCreditDebit("*", acentries);
    BOOST_FOREACH(const CAccountingEntry& entry, acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    Object ret;
    BOOST_FOREACH(const PAIRTYPE(string, int64)& accountBalance, mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listsinceblock [blockhash] [target-confirmations]\n"
            "Get all transactions in blocks since block [blockhash], or all transactions if omitted");

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;

    if (params.size() > 0)
    {
        uint256 blockId = 0;

        blockId.SetHex(params[0].get_str());
        pindex = CBlockLocator(blockId).GetBlockIndex();
    }

    if (params.size() > 1)
    {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    int depth = pindex ? (1 + nBestHeight - pindex->nHeight) : -1;

    Array transactions;

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++)
    {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth)
            ListTransactions(tx, "*", 0, true, transactions);
    }

    uint256 lastblock;

    if (target_confirms == 1)
    {
        lastblock = hashBestChain;
    }
    else
    {
        int target_height = pindexBest->nHeight + 1 - target_confirms;

        CBlockIndex *block;
        for (block = pindexBest;
             block && block->nHeight > target_height;
             block = block->pprev)  { }

        lastblock = block ? block->GetBlockHash() : 0;
    }

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "gettransaction <txid>\n"
            "Get detailed information about <txid>");

    uint256 hash;
    hash.SetHex(params[0].get_str());

    Object entry;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        if (!pwalletMain->mapWallet.count(hash))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
        const CWalletTx& wtx = pwalletMain->mapWallet[hash];

        int64 nCredit = wtx.GetCredit();
        int64 nDebit = wtx.GetDebit();
        int64 nNet = nCredit - nDebit;
        int64 nFee = (wtx.IsFromMe() ? wtx.GetValueOut() - nDebit : 0);

        entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
        if (wtx.IsFromMe())
            entry.push_back(Pair("fee", ValueFromAmount(nFee)));

        WalletTxToJSON(pwalletMain->mapWallet[hash], entry);

        Array details;
        ListTransactions(pwalletMain->mapWallet[hash], "*", 0, false, details);
        entry.push_back(Pair("details", details));
    }

    return entry;
}


Value backupwallet(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "backupwallet <destination>\n"
            "Safely copies wallet.dat to destination, which can be a directory or a path with filename.");

    string strDest = params[0].get_str();
    BackupWallet(*pwalletMain, strDest);

    return Value::null;
}

void ThreadCleanWalletPassphrase(void* parg)
{
    int64 nMyWakeTime = GetTimeMillis() + *((int64*)parg) * 1000;

    cs_nWalletUnlockTime.Enter();

    if (nWalletUnlockTime == 0)
    {
        nWalletUnlockTime = nMyWakeTime;

        do
        {
            if (nWalletUnlockTime==0)
                break;
            int64 nToSleep = nWalletUnlockTime - GetTimeMillis();
            if (nToSleep <= 0)
                break;

            cs_nWalletUnlockTime.Leave();
            MilliSleep(nToSleep);
            cs_nWalletUnlockTime.Enter();

        } while(1);

        if (nWalletUnlockTime)
        {
            nWalletUnlockTime = 0;
            pwalletMain->Lock();
        }
    }
    else
    {
        if (nWalletUnlockTime < nMyWakeTime)
            nWalletUnlockTime = nMyWakeTime;
    }

    cs_nWalletUnlockTime.Leave();

    delete (int64*)parg;
}

Value walletpassphrase(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    if (!pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_ALREADY_UNLOCKED, "Error: Wallet is already unlocked.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }
    else
        throw runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    //CreateThread(ThreadTopUpKeyPool, NULL);
    int64* pnSleepTime = new int64(params[1].get_int64());
    CreateThread(ThreadCleanWalletPassphrase, pnSleepTime);

    return Value::null;
}


Value walletpassphrasechange(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return Value::null;
}


Value walletlock(const Array& params, bool fHelp)
{
    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            "walletlock\n"
            "Removes the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.");
    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    CRITICAL_BLOCK(cs_nWalletUnlockTime)
    {
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return Value::null;
}


Value encryptwallet(const Array& params, bool fHelp)
{
    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");
    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Namecoin server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

Value validateaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "validateaddress <namecoinaddress>\n"
            "Return information about <namecoinaddress>.");

    string strAddress = params[0].get_str();
    uint160 hash160;
    bool isValid = AddressToHash160(strAddress, hash160);

    Object ret;
    ret.push_back(Pair("isvalid", isValid));
    if (isValid)
    {
        // Call Hash160ToAddress() so we always return current ADDRESSVERSION
        // version of the address:
        string currentAddress = Hash160ToAddress(hash160);
        ret.push_back(Pair("address", currentAddress));
        CRITICAL_BLOCK(pwalletMain->cs_mapKeys)
            ret.push_back(Pair("ismine", (pwalletMain->mapPubKeys.count(hash160) > 0)));
        CRITICAL_BLOCK(pwalletMain->cs_mapAddressBook)
        {
            if (pwalletMain->mapAddressBook.count(currentAddress))
                ret.push_back(Pair("account", pwalletMain->mapAddressBook[currentAddress]));
        }
    }
    return ret;
}


Value getwork(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getwork [data]\n"
            "If [data] is not specified, returns formatted hash data to work on:\n"
            "  \"midstate\" : precomputed hash state after hashing the first half of the data\n"
            "  \"data\" : block data\n"
            "  \"hash1\" : formatted hash buffer for second hash\n"
            "  \"target\" : little endian hash target\n"
            "If [data] is specified, tries to solve the block and returns true if it was successful.");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "namecoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "namecoin is downloading blocks...");

    static map<uint256, pair<CBlock*, unsigned int> > mapNewBlock;
    static vector<CBlock*> vNewBlock;
    static CReserveKey reservekey(pwalletMain);

    if (params.size() == 0)
    {
        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64 nStart;
        static CBlock* pblock;
        if (pindexPrev != pindexBest ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60))
        {
            if (pindexPrev != pindexBest)
            {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                BOOST_FOREACH(CBlock* pblock, vNewBlock)
                    delete pblock;
                vNewBlock.clear();
            }
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev = pindexBest;
            nStart = GetTime();

            // Create new block
            pblock = CreateNewBlock(reservekey);
            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
            vNewBlock.push_back(pblock);
        }

        // Update nTime
        pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        static int64 nPrevTime = 0;
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce, nPrevTime);

        // Save
        mapNewBlock[pblock->hashMerkleRoot] = make_pair(pblock, nExtraNonce);

        // Prebuild hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        FormatHashBuffers(pblock, pmidstate, pdata, phash1);

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate))));
        result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1))));
        result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
        return result;
    }
    else
    {
        // Parse parameters
        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        if (vchData.size() != 128)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
        CBlock* pdata = (CBlock*)&vchData[0];

        // Byte reverse
        for (int i = 0; i < 128/4; i++)
            ((unsigned int*)pdata)[i] = CryptoPP::ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        if (!mapNewBlock.count(pdata->hashMerkleRoot))
            return false;
        CBlock* pblock = mapNewBlock[pdata->hashMerkleRoot].first;
        unsigned int nExtraNonce = mapNewBlock[pdata->hashMerkleRoot].second;

        pblock->nTime = pdata->nTime;
        pblock->nNonce = pdata->nNonce;
        pblock->vtx[0].vin[0].scriptSig = CScript() << pblock->nBits << CBigNum(nExtraNonce);
        pblock->hashMerkleRoot = pblock->BuildMerkleTree();

        return CheckWork(pblock, *pwalletMain, reservekey);
    }
}

Value getworkaux(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "getworkaux <aux>\n"
            "getworkaux '' <data>\n"
            "getworkaux 'submit' <data>\n"
            "getworkaux '' <data> <chain-index> <branch>*\n"
            " get work with auxiliary data in coinbase, for multichain mining\n"
            "<aux> is the merkle root of the auxiliary chain block hashes, concatenated with the aux chain merkle tree size and a nonce\n"
            "<chain-index> is the aux chain index in the aux chain merkle tree\n"
            "<branch> is the optional merkle branch of the aux chain\n"
            "If <data> is not specified, returns formatted hash data to work on:\n"
            "  \"midstate\" : precomputed hash state after hashing the first half of the data\n"
            "  \"data\" : block data\n"
            "  \"hash1\" : formatted hash buffer for second hash\n"
            "  \"target\" : little endian hash target\n"
            "If <data> is specified and 'submit', tries to solve the block for this (parent) chain and returns true if it was successful."
            "If <data> is specified and empty first argument, returns the aux merkle root, with size and nonce."
            "If <data> and <chain-index> are specified, creates an auxiliary proof of work for the chain specified and returns:\n"
            "  \"aux\" : merkle root of auxiliary chain block hashes\n"
            "  \"auxpow\" : aux proof of work to submit to aux chain\n"
            );

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Namecoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Namecoin is downloading blocks...");

    static map<uint256, pair<CBlock*, unsigned int> > mapNewBlock;
    static vector<CBlock*> vNewBlock;
    static CReserveKey reservekey(pwalletMain);

    if (params.size() == 1)
    {
        static vector<unsigned char> vchAuxPrev;
        vector<unsigned char> vchAux = ParseHex(params[0].get_str());

        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64 nStart;
        static CBlock* pblock;
        if (pindexPrev != pindexBest ||
            vchAux != vchAuxPrev ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60))
        {
            if (pindexPrev != pindexBest)
            {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                BOOST_FOREACH(CBlock* pblock, vNewBlock)
                    delete pblock;
                vNewBlock.clear();
            }
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev = pindexBest;
            vchAuxPrev = vchAux;
            nStart = GetTime();

            // Create new block
            pblock = CreateNewBlock(reservekey);
            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
            vNewBlock.push_back(pblock);
        }

        // Update nTime
        pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        static int64 nPrevTime = 0;
        IncrementExtraNonceWithAux(pblock, pindexPrev, nExtraNonce, nPrevTime, vchAux);

        // Save
        mapNewBlock[pblock->hashMerkleRoot] = make_pair(pblock, nExtraNonce);

        // Prebuild hash buffers
        char pmidstate[32];
        char pdata[128];
        char phash1[64];
        FormatHashBuffers(pblock, pmidstate, pdata, phash1);

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        Object result;
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate))));
        result.push_back(Pair("data",     HexStr(BEGIN(pdata), END(pdata))));
        result.push_back(Pair("hash1",    HexStr(BEGIN(phash1), END(phash1))));
        result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
        return result;
    }
    else
    {
        if (params[0].get_str() != "submit" && params[0].get_str() != "")
            throw JSONRPCError(RPC_INVALID_PARAMETER, "<aux> must be the empty string or 'submit' if work is being submitted");
        // Parse parameters
        vector<unsigned char> vchData = ParseHex(params[1].get_str());
        if (vchData.size() != 128)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
        CBlock* pdata = (CBlock*)&vchData[0];

        // Byte reverse
        for (int i = 0; i < 128/4; i++)
            ((unsigned int*)pdata)[i] = CryptoPP::ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        if (!mapNewBlock.count(pdata->hashMerkleRoot))
            return false;
        CBlock* pblock = mapNewBlock[pdata->hashMerkleRoot].first;
        unsigned int nExtraNonce = mapNewBlock[pdata->hashMerkleRoot].second;

        pblock->nTime = pdata->nTime;
        pblock->nNonce = pdata->nNonce;

        // Get the aux merkle root from the coinbase
        CScript script = pblock->vtx[0].vin[0].scriptSig;
        opcodetype opcode;
        CScript::const_iterator pc = script.begin();
        script.GetOp(pc, opcode);
        script.GetOp(pc, opcode);
        script.GetOp(pc, opcode);
        if (opcode != OP_2)
            throw runtime_error("invalid aux pow script");
        vector<unsigned char> vchAux;
        script.GetOp(pc, opcode, vchAux);

        RemoveMergedMiningHeader(vchAux);

        pblock->vtx[0].vin[0].scriptSig = MakeCoinbaseWithAux(pblock->nBits, nExtraNonce, vchAux);
        pblock->hashMerkleRoot = pblock->BuildMerkleTree();

        if (params.size() > 2)
        {
            // Requested aux proof of work
            int nChainIndex = params[2].get_int();

            CAuxPow pow(pblock->vtx[0]);

            for (int i = 3 ; i < params.size() ; i++)
            {
                uint256 nHash;
                nHash.SetHex(params[i].get_str());
                pow.vChainMerkleBranch.push_back(nHash);
            }

            pow.SetMerkleBranch(pblock);
            pow.nChainIndex = nChainIndex;
            pow.parentBlock = *pblock;
            CDataStream ss(SER_GETHASH|SER_BLOCKHEADERONLY);
            ss << pow;
            Object result;
            result.push_back(Pair("auxpow", HexStr(ss.begin(), ss.end())));
            return result;
        }
        else
        {
            if (params[0].get_str() == "submit")
            {
                return CheckWork(pblock, *pwalletMain, reservekey);
            }
            else
            {
                Object result;
                result.push_back(Pair("aux", HexStr(vchAux.begin(), vchAux.end())));
                result.push_back(Pair("hash", pblock->GetHash().GetHex()));
                return result;
            }
        }
    }
}

Value getmemorypool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getmemorypool [data]\n"
            "If [data] is not specified, returns data needed to construct a block to work on:\n"
            "  \"version\" : block version\n"
            "  \"previousblockhash\" : hash of current highest block\n"
            "  \"transactions\" : contents of non-coinbase transactions that should be included in the next block\n"
            "  \"coinbasevalue\" : maximum allowable input to coinbase transaction, including the generation award and transaction fees\n"
            "  \"time\" : timestamp appropriate for next block\n"
            "  \"bits\" : compressed target of next block\n"
            "If [data] is specified, tries to solve the block and returns true if it was successful.");

    if (params.size() == 0)
    {
        if (vNodes.empty())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Namecoin is not connected!");

        if (IsInitialBlockDownload())
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Namecoin is downloading blocks...");

        static CReserveKey reservekey(pwalletMain);

        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64 nStart;
        static CBlock* pblock;
        if (pindexPrev != pindexBest ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 5))
        {
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev = pindexBest;
            nStart = GetTime();

            // Create new block
            if(pblock)
                delete pblock;
            pblock = CreateNewBlock(reservekey);
            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
        }

        // Update nTime
        pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
        pblock->nNonce = 0;

        Array transactions;
        BOOST_FOREACH(CTransaction tx, pblock->vtx) {
            if(tx.IsCoinBase())
                continue;

            CDataStream ssTx;
            ssTx << tx;

            transactions.push_back(HexStr(ssTx.begin(), ssTx.end()));
        }

        Object result;
        result.push_back(Pair("version", pblock->nVersion));
        result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
        result.push_back(Pair("transactions", transactions));
        result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].vout[0].nValue));
        result.push_back(Pair("time", (int64_t)pblock->nTime));

        union {
            int32_t nBits;
            char cBits[4];
        } uBits;
        uBits.nBits = htonl((int32_t)pblock->nBits);
        result.push_back(Pair("bits", HexStr(BEGIN(uBits.cBits), END(uBits.cBits))));

        return result;
    }
    else
    {
        // Parse parameters
        CDataStream ssBlock(ParseHex(params[0].get_str()));
        CBlock pblock;
        ssBlock >> pblock;

        return ProcessBlock(NULL, &pblock);
    }
}


Value getauxblock(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 0 && params.size() != 2))
        throw runtime_error(
            "getauxblock [<hash> <auxpow>]\n"
            " create a new block"
            "If <hash>, <auxpow> is not specified, returns a new block hash.\n"
            "If <hash>, <auxpow> is specified, tries to solve the block based on "
            "the aux proof of work and returns true if it was successful.");

    if (vNodes.empty())
        throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Namecoin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Namecoin is downloading blocks...");

    static map<uint256, CBlock*> mapNewBlock;
    static vector<CBlock*> vNewBlock;
    static CReserveKey reservekey(pwalletMain);

    if (params.size() == 0)
    {
        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64 nStart;
        static CBlock* pblock;
        if (pindexPrev != pindexBest ||
            (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60))
        {
            if (pindexPrev != pindexBest)
            {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                BOOST_FOREACH(CBlock* pblock, vNewBlock)
                    delete pblock;
                vNewBlock.clear();
            }
            nTransactionsUpdatedLast = nTransactionsUpdated;
            pindexPrev = pindexBest;
            nStart = GetTime();

            // Create new block with nonce = 0 and extraNonce = 1
            pblock = CreateNewBlock(reservekey);

            // Update nTime
            pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
            pblock->nNonce = 0;

            // Push OP_2 just in case we want versioning later
            pblock->vtx[0].vin[0].scriptSig = CScript() << pblock->nBits << CBigNum(1) << OP_2;
            pblock->hashMerkleRoot = pblock->BuildMerkleTree();

            // Sets the version
            pblock->SetAuxPow(new CAuxPow());

            // Save
            mapNewBlock[pblock->GetHash()] = pblock;

            if (!pblock)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");
            vNewBlock.push_back(pblock);
        }

        uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

        Object result;
        result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
        result.push_back(Pair("hash", pblock->GetHash().GetHex()));
        result.push_back(Pair("chainid", pblock->GetChainID()));
        return result;
    }
    else
    {
        uint256 hash;
        hash.SetHex(params[0].get_str());
        vector<unsigned char> vchAuxPow = ParseHex(params[1].get_str());
        CDataStream ss(vchAuxPow, SER_GETHASH|SER_BLOCKHEADERONLY);
        CAuxPow* pow = new CAuxPow();
        ss >> *pow;
        if (!mapNewBlock.count(hash))
            return ::error("getauxblock() : block not found");

        CBlock* pblock = mapNewBlock[hash];
        pblock->SetAuxPow(pow);

        if (!CheckWork(pblock, *pwalletMain, reservekey))
        {
            return false;
        }
        else
        {
            return true;
        }
    }
}

Value buildmerkletree(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
                "buildmerkletree <obj>...\n"
                " build a merkle tree with the given hex-encoded objects\n"
                );
    vector<uint256> vTree;
    BOOST_FOREACH(const Value& obj, params)
    {
        uint256 nHash;
        nHash.SetHex(obj.get_str());
        vTree.push_back(nHash);
    }

    int j = 0;
    for (int nSize = params.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            vTree.push_back(Hash(BEGIN(vTree[j+i]),  END(vTree[j+i]),
                        BEGIN(vTree[j+i2]), END(vTree[j+i2])));
        }
        j += nSize;
    }

    Array result;
    BOOST_FOREACH(uint256& nNode, vTree)
    {
        result.push_back(nNode.GetHex());
    }

    return result;
}

Value importprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importprivkey <namecoinprivkey> [label] [rescan=true]\n"
            "Adds a private key (as returned by dumpprivkey) to your wallet."
            + HelpRequiringPassphrase());

    string strSecret = params[0].get_str();
    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");

    CKey key;
    bool fCompressed;
    CSecret32 secret = vchSecret.GetSecret(fCompressed);
    key.SetSecret(secret, fCompressed);
    string strAddress = PubKeyToAddress(key.GetPubKey());
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_wallet)
    {
        EnsureWalletIsUnlocked();

        if (!pwalletMain->AddKey(key))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookName(strAddress, strLabel);

        if (fRescan) {
            pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
            pwalletMain->ReacceptWalletTransactions();
        }
    }

    return Value::null;
}

// Based on Codeshark's pull reqeust: https://github.com/bitcoin/bitcoin/pull/2121/files
Value importaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importaddress <namecoinaddress> [label] [rescan=true]\n"
            "Adds an address that can be watched as if it were in your wallet but cannot be used to spend.");

    string strAddress = params[0].get_str();
    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Namecoin address");

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_wallet)
    {
        if (!pwalletMain->AddAddress(hash160))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBookName(strAddress, strLabel);

        if (fRescan)
        {
            pwalletMain->ScanForWalletTransactions(pindexGenesisBlock, true);
            pwalletMain->ReacceptWalletTransactions();
        }
    }

    return Value::null;
}

Value dumpprivkey(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey <namecoinaddress>\n"
            "Reveals the private key corresponding to <namecoinaddress>."
            + HelpRequiringPassphrase());

    string strAddress = params[0].get_str();

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Namecoin address");

    CPrivKey privKey;
    bool found = false;
    CRITICAL_BLOCK(pwalletMain->cs_mapKeys)
    {
        EnsureWalletIsUnlocked();

        std::map<uint160, std::vector<unsigned char> >::const_iterator mi = pwalletMain->mapPubKeys.find(hash160);
        if (mi != pwalletMain->mapPubKeys.end() && pwalletMain->GetPrivKey(mi->second, privKey))
            found = true;
    }

    if (!found)
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    CKey key;
    if (!key.SetPrivKey(privKey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is invalid");
    bool fCompressed;
    CSecret32 secret = key.GetSecret(fCompressed);
    return CBitcoinSecret(secret, fCompressed).ToString();
}

Value signmessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "signmessage <namecoinaddress> <message>\n"
            "Sign a message with the private key of an address"
            + HelpRequiringPassphrase());

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CPrivKey privKey;
    CRITICAL_BLOCK(pwalletMain->cs_mapKeys)
    {
        std::map<uint160, std::vector<unsigned char> >::const_iterator mi = pwalletMain->mapPubKeys.find(hash160);
        if (mi == pwalletMain->mapPubKeys.end())
            throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

        if (!pwalletMain->GetPrivKey(mi->second, privKey))
            throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    CKey key;
    key.SetPrivKey(privKey);
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

Value verifymessage(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "verifymessage <namecoinaddress> <signature> <message>\n"
            "Verify a signed message");

    string strAddress  = params[0].get_str();
    string strSign     = params[1].get_str();
    string strMessage  = params[2].get_str();

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CKey key;
    if (!key.SetCompactSignature(Hash(ss.begin(), ss.end()), vchSig))
        return false;

    return Hash160(key.GetPubKey()) == hash160;
}

//
// Utilities: convert hex-encoded Values
// (throws error if not hex).
//
uint256 ParseHashV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex)) // Note: IsHex("") is false
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    uint256 result;
    result.SetHex(strHex);
    return result;
}
uint256 ParseHashO(const Object& o, string strKey)
{
    return ParseHashV(find_value(o, strKey), strKey);
}
vector<unsigned char> ParseHexV(const Value& v, string strName)
{
    string strHex;
    if (v.type() == str_type)
        strHex = v.get_str();
    if (!IsHex(strHex))
        throw JSONRPCError(RPC_INVALID_PARAMETER, strName+" must be hexadecimal string (not '"+strHex+"')");
    return ParseHex(strHex);
}
vector<unsigned char> ParseHexO(const Object& o, string strKey)
{
    return ParseHexV(find_value(o, strKey), strKey);
}

void ScriptPubKeyToJSON(const CScript& scriptPubKey, Object& out)
{
    //txnouttype type;
    //vector<CTxDestination> addresses;
    string address;
    int nRequired = 1;

    /* If this is a name transaction, try to strip off the initial
       script and decode the rest for better results.  */
    std::string nameAsmPrefix = "";
    CScript script(scriptPubKey);
    int op;
    vector<vector<unsigned char> > vvch;
    CScript::const_iterator pc = script.begin();
    if (DecodeNameScript(script, op, vvch, pc))
    {
        Object nameOp;

        switch (op)
        {
        case OP_NAME_NEW:
        {
            assert(vvch.size() == 1);
            const std::string rand = HexStr(vvch[0].begin(), vvch[0].end());
            nameOp.push_back(Pair("op", "name_new"));
            nameOp.push_back(Pair("rand", rand));
            break;
        }

        case OP_NAME_FIRSTUPDATE:
        {
            assert(vvch.size() == 3);
            const std::string name(vvch[0].begin(), vvch[0].end());
            const std::string rand = HexStr(vvch[1].begin(), vvch[1].end());
            const std::string val(vvch[2].begin(), vvch[2].end());
            nameOp.push_back(Pair("op", "name_firstupdate"));
            nameOp.push_back(Pair("name", name));
            nameOp.push_back(Pair("rand", rand));
            nameOp.push_back(Pair("value", val));
            break;
        }

        case OP_NAME_UPDATE:
        {
            assert(vvch.size() == 2);
            const std::string name(vvch[0].begin(), vvch[0].end());
            const std::string val(vvch[1].begin(), vvch[1].end());
            nameOp.push_back(Pair("op", "name_update"));
            nameOp.push_back(Pair("name", name));
            nameOp.push_back(Pair("value", val));
            break;
        }

        default:
            nameOp.push_back(Pair("op", "unknown"));
            break;
        }

        out.push_back(Pair("nameOp", nameOp));
        nameAsmPrefix = "NAME_OPERATION ";
        script = CScript(pc, script.end());
    }

    /* Write out the results.  */
    out.push_back(Pair("asm", nameAsmPrefix + script.ToString()));
    out.push_back(Pair("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

    //if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired))
    if (!ExtractDestination(script, address))
    {
        out.push_back(Pair("type", "nonstandard" /*GetTxnOutputType(TX_NONSTANDARD)*/ ));
        return;
    }

    /* Note:  ExtractDestination handles both pubkey hash and pubkey,
       but the code below prints pubkeyhash as type in both cases.  That
       is not so big a deal, presumably.  */

    out.push_back(Pair("reqSigs", nRequired));
    out.push_back(Pair("type", "pubkeyhash" /*GetTxnOutputType(type)*/ ));

    Array a;
    //BOOST_FOREACH(const CTxDestination& addr, addresses)
    //    a.push_back(CBitcoinAddress(addr).ToString());
    a.push_back(address);

    out.push_back(Pair("addresses", a));
}

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, Object& entry)
{
    entry.push_back(Pair("txid", tx.GetHash().GetHex()));
    entry.push_back(Pair("version", tx.nVersion));
    entry.push_back(Pair("locktime", (boost::int64_t)tx.nLockTime));

    const CBlockIndex* pindex = NULL;
    if (hashBlock != 0)
    {
        entry.push_back(Pair("blockhash", hashBlock.GetHex()));
        map<uint256, CBlockIndex*>::const_iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
        {
            pindex = (*mi).second;
            if (pindex->IsInMainChain())
            {
                entry.push_back(Pair("confirmations", 1 + nBestHeight - pindex->nHeight));
                entry.push_back(Pair("time", (boost::int64_t)pindex->nTime));
                entry.push_back(Pair("blocktime", (boost::int64_t)pindex->nTime));
            }
            else
                entry.push_back(Pair("confirmations", 0));
        }
    }

    Array vin;
    int64 nValueIn = 0;
    bool fullValueIn = true;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        Object in;
        if (tx.IsCoinBase())
        {
            in.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));

            if (pindex)
            {
                const int64 val = GetBlockValue(pindex->nHeight, 0);
                nValueIn += val;
                in.push_back(Pair("value", ValueFromAmount(val)));
            }
            else
                fullValueIn = false;
        }
        else
        {
            in.push_back(Pair("txid", txin.prevout.hash.GetHex()));
            in.push_back(Pair("vout", (boost::int64_t)txin.prevout.n));
            Object o;
            o.push_back(Pair("asm", txin.scriptSig.ToString()));
            o.push_back(Pair("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
            in.push_back(Pair("scriptSig", o));

            /* Try to retrieve previous transaction output to find
               its value in order to calculate transaction fees.  */
            CTransaction prevTx;
            uint256 prevHashBlock = 0;
            if (GetTransaction(txin.prevout.hash, prevTx, prevHashBlock))
            {
                if (prevTx.vout.size () > txin.prevout.n)
                {
                    const int64 val = prevTx.vout[txin.prevout.n].nValue;
                    nValueIn += val;
                    in.push_back(Pair("value", ValueFromAmount(val)));
                }
                else
                    fullValueIn = false;
            }
            else
                fullValueIn = false;
        }
        in.push_back(Pair("sequence", (boost::int64_t)txin.nSequence));
        vin.push_back(in);
    }
    entry.push_back(Pair("vin", vin));

    Array vout;
    int64 nValueOut = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        nValueOut += txout.nValue;
        Object out;
        out.push_back(Pair("value", ValueFromAmount(txout.nValue)));
        out.push_back(Pair("n", (boost::int64_t)i));
        Object o;
        ScriptPubKeyToJSON(txout.scriptPubKey, o);
        out.push_back(Pair("scriptPubKey", o));
        vout.push_back(out);
    }
    entry.push_back(Pair("vout", vout));

    if (fullValueIn)
        entry.push_back(Pair("fees", ValueFromAmount(nValueIn - nValueOut)));
}

Value getrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getrawtransaction <txid> [verbose=0]\n"
            "If verbose=0, returns a string that is\n"
            "serialized, hex-encoded data for <txid>.\n"
            "If verbose is non-zero, returns an Object\n"
            "with information about <txid>.");

    uint256 hash = ParseHashV(params[0], "parameter 1");

    bool fVerbose = false;
    if (params.size() > 1)
        fVerbose = (params[1].get_int() != 0);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock /*, true*/))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");

    CDataStream ssTx(SER_NETWORK, VERSION);
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());

    if (!fVerbose)
        return strHex;

    Object result;
    result.push_back(Pair("hex", strHex));
    TxToJSON(tx, hashBlock, result);
    return result;
}

Value listunspent(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listunspent [minconf=1] [maxconf=9999999]  [\"address\",...]\n"
            "Returns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filtered to only include txouts paid to specified addresses.\n"
            "Results are an array of Objects, each of which has:\n"
            "{txid, vout, scriptPubKey, amount, confirmations}");

    RPCTypeCheck(params, boost::assign::list_of(int_type)(int_type)(array_type));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    set<string> setAddress;
    if (params.size() > 2)
    {
        Array inputs = params[2].get_array();
        BOOST_FOREACH(Value& input, inputs)
        {
            string address = input.get_str();
            if (!IsValidBitcoinAddress(address))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Namecoin address: ")+input.get_str());
            if (setAddress.count(address))
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+input.get_str());
           setAddress.insert(address);
        }
    }

    Array results;
    vector<COutput> vecOutputs;
    pwalletMain->AvailableCoins(vecOutputs, false);
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        if (setAddress.size())
        {
            string address;
            if (!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address))
                continue;

            if (!setAddress.count(address))
                continue;
        }

        int64 nValue = out.tx->vout[out.i].nValue;
        const CScript& pk = out.tx->vout[out.i].scriptPubKey;
        Object entry;
        entry.push_back(Pair("txid", out.tx->GetHash().GetHex()));
        entry.push_back(Pair("vout", out.i));
        entry.push_back(Pair("scriptPubKey", HexStr(pk.begin(), pk.end())));
        /*if (pk.IsPayToScriptHash())
        {
            string address;
            if (ExtractDestination(pk, address))
            {
                const CScriptID& hash = boost::get<const CScriptID&>(address);
                CScript redeemScript;
                if (pwalletMain->GetCScript(hash, redeemScript))
                    entry.push_back(Pair("redeemScript", HexStr(redeemScript.begin(), redeemScript.end())));
            }
        }*/
        entry.push_back(Pair("amount",ValueFromAmount(nValue)));
        entry.push_back(Pair("confirmations",out.nDepth));
        results.push_back(entry);
    }

    return results;
}

Value createrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || (params.size() != 2 && params.size() != 3))
        throw runtime_error(
            "createrawtransaction [{\"txid\":txid,\"vout\":n},...] {address:amount,...}\n"
            "optional third argument:\n"
            "  {\"op\":\"name_update\", \"name\":name, \"value\":value, \"address\":address}\n\n"
            "Create a transaction spending given inputs\n"
            "(array of objects containing transaction id and output number),\n"
            "sending to given address(es).\n"
            "Optionally, a name_update operation can be performed.\n\n"
            "Returns hex-encoded raw transaction.\n\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.");

    if (params.size() == 2)
        RPCTypeCheck(params, boost::assign::list_of(array_type)(obj_type));
    else
        RPCTypeCheck(params, boost::assign::list_of(array_type)(obj_type)(obj_type));

    Array inputs = params[0].get_array();
    Object sendTo = params[1].get_obj();

    CTransaction rawTx;

    BOOST_FOREACH(const Value& input, inputs)
    {
        const Object& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const Value& vout_v = find_value(o, "vout");
        if (vout_v.type() != int_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(txid, nOutput));
        rawTx.vin.push_back(in);
    }

    set<string> setAddress;
    BOOST_FOREACH(const Pair& s, sendTo)
    {
        string address = s.name_;
        if (!IsValidBitcoinAddress(address))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Namecoin address: ")+s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+s.name_);
        setAddress.insert(address);

        CScript scriptPubKey;
        scriptPubKey.SetDestination(address);
        int64 nAmount = AmountFromValue(s.value_);

        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
    }

    if (params.size() == 3)
    {
        Object nameOp = params[2].get_obj();
        AddRawTxNameOperation(rawTx, nameOp);
    }

    CDataStream ss(SER_NETWORK, VERSION);
    ss << rawTx;
    return HexStr(ss.begin(), ss.end());
}

Value decoderawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decoderawtransaction <hex string>\n"
            "Return a JSON object representing the serialized, hex-encoded transaction.");

    vector<unsigned char> txData(ParseHexV(params[0], "argument"));
    CDataStream ssData(txData, SER_NETWORK, VERSION);
    CTransaction tx;
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    Object result;
    TxToJSON(tx, 0, result);

    return result;
}

Value signrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "signrawtransaction <hex string> [{\"txid\":txid,\"vout\":n,\"scriptPubKey\":hex},...] [<privatekey1>,...] [sighashtype=\"ALL\"]\n"
            "Sign inputs for raw transaction (serialized, hex-encoded).\n"
            "Second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "Third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
            "Fourth optional argument is a string that is one of six values; ALL, NONE, SINGLE or\n"
            "ALL|ANYONECANPAY, NONE|ANYONECANPAY, SINGLE|ANYONECANPAY.\n"
            "Returns json object with keys:\n"
            "  hex : raw transaction with signature(s) (hex-encoded string)\n"
            "  complete : 1 if transaction has a complete set of signature (0 if not)"
            + HelpRequiringPassphrase());

    RPCTypeCheck(params, boost::assign::list_of(str_type)(array_type)(array_type)(str_type), true);

    vector<unsigned char> txData(ParseHexV(params[0], "argument 1"));
    CDataStream ssData(txData, SER_NETWORK, VERSION);
    vector<CTransaction> txVariants;
    while (!ssData.empty())
    {
        try {
            CTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        }
        catch (std::exception &e) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
    }

    if (txVariants.empty())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CTransaction mergedTx(txVariants[0]);
    bool fComplete = true;

    // Fetch previous transactions (inputs):
    map<COutPoint, CScript> mapPrevOut;
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTransaction tempTx;
        MapPrevTx mapPrevTx;
        CTxDB txdb("r");
        map<uint256, CTxIndex> unused;
        bool fInvalid;

        // FetchInputs aborts on failure, so we go one at a time.
        tempTx.vin.push_back(mergedTx.vin[i]);
        tempTx.FetchInputs(txdb, unused, false, false, mapPrevTx, fInvalid);

        // Copy results into mapPrevOut:
        BOOST_FOREACH(const CTxIn& txin, tempTx.vin)
        {
            const uint256& prevHash = txin.prevout.hash;
            if (mapPrevTx.count(prevHash) && mapPrevTx[prevHash].second.vout.size()>txin.prevout.n)
                mapPrevOut[txin.prevout] = mapPrevTx[prevHash].second.vout[txin.prevout.n].scriptPubKey;
        }
    }

    bool fGivenKeys = false;
    CKeyStore tempKeystore;
    if (params.size() > 2 && params[2].type() != null_type)
    {
        fGivenKeys = true;
        Array keys = params[2].get_array();
        BOOST_FOREACH(Value k, keys)
        {
            CBitcoinSecret vchSecret;
            bool fGood = vchSecret.SetString(k.get_str());
            if (!fGood)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
            CKey key;
            bool fCompressed;
            CSecret32 secret = vchSecret.GetSecret(fCompressed);
            key.SetSecret(secret, fCompressed);
            tempKeystore.AddKey(key);
        }
    }
    else
        EnsureWalletIsUnlocked();

    // Add previous txouts given in the RPC call:
    if (params.size() > 1 && params[1].type() != null_type)
    {
        Array prevTxs = params[1].get_array();
        BOOST_FOREACH(Value& p, prevTxs)
        {
            if (p.type() != obj_type)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            Object prevOut = p.get_obj();

            RPCTypeCheck(prevOut, boost::assign::map_list_of("txid", str_type)("vout", int_type)("scriptPubKey", str_type));

            uint256 txid = ParseHashO(prevOut, "txid");

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            vector<unsigned char> pkData(ParseHexO(prevOut, "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            COutPoint outpoint(txid, nOut);
            if (mapPrevOut.count(outpoint))
            {
                // Complain if scriptPubKey doesn't match
                if (mapPrevOut[outpoint] != scriptPubKey)
                {
                    string err("Previous output scriptPubKey mismatch:\n");
                    err = err + mapPrevOut[outpoint].ToString() + "\nvs:\n"+
                        scriptPubKey.ToString();
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
                // what todo if txid is known, but the actual output isn't?
            }
            else
                mapPrevOut[outpoint] = scriptPubKey;
        }
    }


    const CKeyStore& keystore = (fGivenKeys ? tempKeystore : *pwalletMain);

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && params[3].type() != null_type)
    {
        static map<string, int> mapSigHashValues =
            boost::assign::map_list_of
            (string("ALL"), int(SIGHASH_ALL))
            (string("ALL|ANYONECANPAY"), int(SIGHASH_ALL|SIGHASH_ANYONECANPAY))
            (string("NONE"), int(SIGHASH_NONE))
            (string("NONE|ANYONECANPAY"), int(SIGHASH_NONE|SIGHASH_ANYONECANPAY))
            (string("SINGLE"), int(SIGHASH_SINGLE))
            (string("SINGLE|ANYONECANPAY"), int(SIGHASH_SINGLE|SIGHASH_ANYONECANPAY))
            ;
        string strHashType = params[3].get_str();
        if (mapSigHashValues.count(strHashType))
            nHashType = mapSigHashValues[strHashType];
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++)
    {
        CTxIn& txin = mergedTx.vin[i];
        if (mapPrevOut.count(txin.prevout) == 0)
        {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = mapPrevOut[txin.prevout];

        txin.scriptSig.clear();
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            SignSignature(keystore, prevPubKey, mergedTx, i, nHashType);

        // ... and merge in other signatures:
        BOOST_FOREACH(const CTransaction& txv, txVariants)
        {
            txin.scriptSig = CombineSignatures(prevPubKey, mergedTx, i, txin.scriptSig, txv.vin[i].scriptSig);
        }
        if (!VerifyScript(txin.scriptSig, prevPubKey, mergedTx, i /*, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC*/ , 0))
            fComplete = false;
    }

    Object result;
    CDataStream ssTx(SER_NETWORK, VERSION);
    ssTx << mergedTx;
    result.push_back(Pair("hex", HexStr(ssTx.begin(), ssTx.end())));
    result.push_back(Pair("complete", fComplete));

    return result;
}

Value sendrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "sendrawtransaction <hex string>\n"
            "Submits raw transaction (serialized, hex-encoded) to local node and network.");

    // parse hex string from parameter
    vector<unsigned char> txData(ParseHexV(params[0], "parameter"));
    CDataStream ssData(txData, SER_NETWORK, VERSION);
    CTransaction tx;

    // deserialize binary data stream
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }
    uint256 hashTx = tx.GetHash();

    // See if the transaction is already in a block
    // or in the memory pool:
    CTransaction existingTx;
    uint256 hashBlock = 0;
    if (GetTransaction(hashTx, existingTx, hashBlock))
    {
        if (hashBlock != 0)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("transaction already in block ")+hashBlock.GetHex());
        // Not in block, but already in the memory pool; will drop
        // through to re-relay it.
    }
    else
    {
        // push to local node
        DatabaseSet dbset("r");
        if (!tx.AcceptToMemoryPool (dbset, true, false))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX rejected");

        SyncWithWallets(tx, NULL, true);
    }
    RelayMessage(CInv(MSG_TX, hashTx), tx);

    return hashTx.GetHex();
}

extern CCriticalSection cs_mapTransactions;

Value getrawmempool(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getrawmempool\n"
            "Returns all transaction ids in memory pool.");

    Array a;

    CRITICAL_BLOCK(cs_mapTransactions)
    {
        a.reserve(mapTransactions.size());
        BOOST_FOREACH(const PAIRTYPE(uint256, CTransaction) &mi, mapTransactions)
            a.push_back(mi.first.ToString());
    }

    return a;
}

/* Block until a new block is found and return only then.  */
static Value
waitforblock (const Array& params, bool fHelp)
{
  if (fHelp || params.size () > 1)
    throw runtime_error (
      "waitforblock [blockHash]\n"
      "Wait for a change in the best chain (a new block being found)"
      " and return the new block's hash when it arrives.  If blockHash"
      " is given, wait until a block with different hash is found.\n");

  if (IsInitialBlockDownload ())
    throw JSONRPCError (RPC_CLIENT_IN_INITIAL_DOWNLOAD,
                        "huntercoin is downloading blocks...");

  uint256 lastHash;
  if (params.size () > 0)
    lastHash = ParseHashV (params[0], "blockHash");
  else
    lastHash = hashBestChain;

  boost::unique_lock<boost::mutex> lock(mut_newBlock);
  while (true)
    {
      /* Atomically check whether we have found a new best block and return
         it if that's the case.  We use a lock on cs_main in order to
         prevent race conditions.  */
      CRITICAL_BLOCK(cs_main)
        {
          if (lastHash != hashBestChain)
            return hashBestChain.GetHex ();
        }

      /* Wait on the condition variable.  */
      cv_newBlock.wait (lock);
    }
}


//
// Call Table
//

pair<string, rpcfn_type> pCallTable[] =
{
    make_pair("help",                  &help),
    make_pair("stop",                  &stop),
    make_pair("getblockbycount",       &getblockbycount),
    make_pair("getblock",              &getblock),
    make_pair("getblockcount",         &getblockcount),
    make_pair("getblockhash",          &getblockhash),
    make_pair("getblocknumber",        &getblocknumber),
    make_pair("getchains",             &getchains),
    make_pair("getconnectioncount",    &getconnectioncount),
    make_pair("getdifficulty",         &getdifficulty),
    make_pair("getgenerate",           &getgenerate),
    make_pair("setgenerate",           &setgenerate),
    make_pair("gethashespersec",       &gethashespersec),
    make_pair("getinfo",               &getinfo),
    make_pair("getnewaddress",         &getnewaddress),
    make_pair("getaccountaddress",     &getaccountaddress),
    make_pair("setaccount",            &setaccount),
    make_pair("setlabel",              &setaccount), // deprecated
    make_pair("getaccount",            &getaccount),
    make_pair("getlabel",              &getaccount), // deprecated
    make_pair("getaddressesbyaccount", &getaddressesbyaccount),
    make_pair("getaddressesbylabel",   &getaddressesbyaccount), // deprecated
    make_pair("sendtoaddress",         &sendtoaddress),
    make_pair("getamountreceived",     &getreceivedbyaddress), // deprecated, renamed to getreceivedbyaddress
    make_pair("getallreceived",        &listreceivedbyaddress), // deprecated, renamed to listreceivedbyaddress
    make_pair("getreceivedbyaddress",  &getreceivedbyaddress),
    make_pair("getreceivedbyaccount",  &getreceivedbyaccount),
    make_pair("getreceivedbylabel",    &getreceivedbyaccount), // deprecated
    make_pair("listreceivedbyaddress", &listreceivedbyaddress),
    make_pair("listreceivedbyaccount", &listreceivedbyaccount),
    make_pair("listreceivedbylabel",   &listreceivedbyaccount), // deprecated
    make_pair("backupwallet",          &backupwallet),
    make_pair("walletpassphrase",      &walletpassphrase),
    make_pair("walletpassphrasechange", &walletpassphrasechange),
    make_pair("walletlock",            &walletlock),
    make_pair("encryptwallet",         &encryptwallet),
    make_pair("validateaddress",       &validateaddress),
    make_pair("getbalance",            &getbalance),
    make_pair("move",                  &movecmd),
    make_pair("sendfrom",              &sendfrom),
    make_pair("sendmany",              &sendmany),
    make_pair("gettransaction",        &gettransaction),
    make_pair("listtransactions",      &listtransactions),
    make_pair("getwork",               &getwork),
    make_pair("getworkaux",            &getworkaux),
    make_pair("getauxblock",           &getauxblock),
    make_pair("buildmerkletree",       &buildmerkletree),
    make_pair("listaccounts",          &listaccounts),
    make_pair("settxfee",              &settxfee),
    make_pair("getmemorypool",         &getmemorypool),
    make_pair("setmininput",           &setmininput),
    make_pair("dumpprivkey",           &dumpprivkey),
    make_pair("importprivkey",         &importprivkey),
    make_pair("importaddress",         &importaddress),
    make_pair("signmessage",           &signmessage),
    make_pair("verifymessage",         &verifymessage),
    make_pair("listunspent",           &listunspent),
    make_pair("listaddressgroupings",  &listaddressgroupings),
    make_pair("listsinceblock",        &listsinceblock),
    make_pair("getrawtransaction",     &getrawtransaction),
    make_pair("createrawtransaction",  &createrawtransaction),
    make_pair("decoderawtransaction",  &decoderawtransaction),
    make_pair("signrawtransaction",    &signrawtransaction),
    make_pair("sendrawtransaction",    &sendrawtransaction),
    make_pair("getrawmempool",         &getrawmempool),
    make_pair("waitforblock",          &waitforblock),
};
map<string, rpcfn_type> mapCallTable(pCallTable, pCallTable + sizeof(pCallTable)/sizeof(pCallTable[0]));

string pAllowInSafeMode[] =
{
    "help",
    "stop",
    "getblockcount",
    "getblocknumber",
    "getconnectioncount",
    "getdifficulty",
    "getgenerate",
    "setgenerate",
    "gethashespersec",
    "getinfo",
    "getnewaddress",
    "getaccountaddress",
    "setlabel",
    "getaccount",
    "getlabel", // deprecated
    "getaddressesbyaccount",
    "getaddressesbylabel", // deprecated
    "backupwallet",
    "walletpassphrase",
    "walletlock",
    "validateaddress",
    "getwork",
    "getworkaux",
    "getauxblock",
    "getmemorypool",
    "dumpprivkey",
    "getrawmempool",
};
set<string> setAllowInSafeMode(pAllowInSafeMode, pAllowInSafeMode + sizeof(pAllowInSafeMode)/sizeof(pAllowInSafeMode[0]));

/* Methods that will be called in a new thread and can block waiting for
   some condition without hurting the RPC server performance.  */
string pCallAsync[] =
{
    "waitforblock",
};
set<string> setCallAsync(pCallAsync, pCallAsync + sizeof(pCallAsync)/sizeof(pCallAsync[0]));




//
// HTTP protocol
//
// This ain't Apache.  We're just using HTTP header for the length field
// and to be compatible with other JSON-RPC implementations.
//

string HTTPPost(const string& strMsg, const map<string,string>& mapRequestHeaders)
{
    ostringstream s;
    s << "POST / HTTP/1.1\r\n"
      << "User-Agent: namecoin-json-rpc/" << FormatFullVersion() << "\r\n"
      << "Host: 127.0.0.1\r\n"
      << "Content-Type: application/json\r\n"
      << "Content-Length: " << strMsg.size() << "\r\n"
      << "Accept: application/json\r\n";
    BOOST_FOREACH(const PAIRTYPE(string, string)& item, mapRequestHeaders)
        s << item.first << ": " << item.second << "\r\n";
    s << "\r\n" << strMsg;

    return s.str();
}

string rfc1123Time()
{
    char buffer[64];
    time_t now;
    time(&now);
    struct tm* now_gmt = gmtime(&now);
    string locale(setlocale(LC_TIME, NULL));
    setlocale(LC_TIME, "C"); // we want posix (aka "C") weekday/month strings
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S +0000", now_gmt);
    setlocale(LC_TIME, locale.c_str());
    return string(buffer);
}

static string HTTPReply(int nStatus, const string& strMsg)
{
    if (nStatus == 401)
        return strprintf("HTTP/1.0 401 Authorization Required\r\n"
            "Date: %s\r\n"
            "Server: namecoin-json-rpc/%s\r\n"
            "WWW-Authenticate: Basic realm=\"jsonrpc\"\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 296\r\n"
            "\r\n"
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\r\n"
            "\"http://www.w3.org/TR/1999/REC-html401-19991224/loose.dtd\">\r\n"
            "<HTML>\r\n"
            "<HEAD>\r\n"
            "<TITLE>Error</TITLE>\r\n"
            "<META HTTP-EQUIV='Content-Type' CONTENT='text/html; charset=ISO-8859-1'>\r\n"
            "</HEAD>\r\n"
            "<BODY><H1>401 Unauthorized.</H1></BODY>\r\n"
            "</HTML>\r\n", rfc1123Time().c_str(), FormatFullVersion().c_str());
    string strStatus;
         if (nStatus == 200) strStatus = "OK";
    else if (nStatus == 400) strStatus = "Bad Request";
    else if (nStatus == 403) strStatus = "Forbidden";
    else if (nStatus == 404) strStatus = "Not Found";
    else if (nStatus == 500) strStatus = "Internal Server Error";
    return strprintf(
            "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Connection: close\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: application/json\r\n"
            "Server: namecoin-json-rpc/%s\r\n"
            "\r\n"
            "%s",
        nStatus,
        strStatus.c_str(),
        rfc1123Time().c_str(),
        strMsg.size(),
        FormatFullVersion().c_str(),
        strMsg.c_str());
}

int ReadHTTPStatus(std::basic_istream<char>& stream)
{
    string str;
    getline(stream, str);
    vector<string> vWords;
    boost::split(vWords, str, boost::is_any_of(" "));
    if (vWords.size() < 2)
        return 500;
    return atoi(vWords[1].c_str());
}

int ReadHTTPHeader(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet)
{
    int nLen = 0;
    loop
    {
        string str;
        std::getline(stream, str);
        if (str.empty() || str == "\r")
            break;
        string::size_type nColon = str.find(":");
        if (nColon != string::npos)
        {
            string strHeader = str.substr(0, nColon);
            boost::trim(strHeader);
            boost::to_lower(strHeader);
            string strValue = str.substr(nColon+1);
            boost::trim(strValue);
            mapHeadersRet[strHeader] = strValue;
            if (strHeader == "content-length")
                nLen = atoi(strValue.c_str());
        }
    }
    return nLen;
}

int ReadHTTP(std::basic_istream<char>& stream, map<string, string>& mapHeadersRet, string& strMessageRet)
{
    mapHeadersRet.clear();
    strMessageRet = "";

    // Read status
    int nStatus = ReadHTTPStatus(stream);

    // Read header
    int nLen = ReadHTTPHeader(stream, mapHeadersRet);
    if (nLen < 0 || nLen > MAX_SIZE)
        return 500;

    // Read message
    if (nLen > 0)
    {
        vector<char> vch(nLen);
        stream.read(&vch[0], nLen);
        strMessageRet = string(vch.begin(), vch.end());
    }

    return nStatus;
}

string EncodeBase64(string s)
{
    BIO *b64, *bmem;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, s.c_str(), s.size());
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    string result(bptr->data, bptr->length);
    BIO_free_all(b64);

    return result;
}

string DecodeBase64(string s)
{
    BIO *b64, *bmem;

    char* buffer = static_cast<char*>(calloc(s.size(), sizeof(char)));

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new_mem_buf(const_cast<char*>(s.c_str()), s.size());
    bmem = BIO_push(b64, bmem);
    BIO_read(bmem, buffer, s.size());
    BIO_free_all(bmem);

    string result(buffer);
    free(buffer);
    return result;
}

bool HTTPAuthorized(map<string, string>& mapHeaders)
{
    string strAuth = mapHeaders["authorization"];
    if (strAuth.substr(0,6) != "Basic ")
        return false;
    string strUserPass64 = strAuth.substr(6); boost::trim(strUserPass64);
    string strUserPass = DecodeBase64(strUserPass64);
    string::size_type nColon = strUserPass.find(":");
    if (nColon == string::npos)
        return false;
    string strUser = strUserPass.substr(0, nColon);
    string strPassword = strUserPass.substr(nColon+1);
    return (strUser == mapArgs["-rpcuser"] && strPassword == mapArgs["-rpcpassword"]);
}

//
// JSON-RPC protocol.  Bitcoin speaks version 1.0 for maximum compatibility,
// but uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
// unspecified (HTTP errors and contents of 'error').
//
// 1.0 spec: http://json-rpc.org/wiki/specification
// 1.2 spec: http://groups.google.com/group/json-rpc/web/json-rpc-over-http
// http://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
//

string JSONRPCRequest(const string& strMethod, const Array& params, const Value& id)
{
    Object request;
    request.push_back(Pair("method", strMethod));
    request.push_back(Pair("params", params));
    request.push_back(Pair("id", id));
    return write_string(Value(request), false) + "\n";
}

string JSONRPCReply(const Value& result, const Value& error, const Value& id)
{
    Object reply;
    if (error.type() != null_type)
        reply.push_back(Pair("result", Value::null));
    else
        reply.push_back(Pair("result", result));
    reply.push_back(Pair("error", error));
    reply.push_back(Pair("id", id));
    return write_string(Value(reply), false) + "\n";
}

void ErrorReply(std::ostream& stream, const Object& objError, const Value& id)
{
    // Send error reply from json-rpc error object
    int nStatus = 500;
    int code = find_value(objError, "code").get_int();
    if (code == -32600) nStatus = 400;
    else if (code == -32601) nStatus = 404;
    string strReply = JSONRPCReply(Value::null, objError, id);
    stream << HTTPReply(nStatus, strReply) << std::flush;
}

bool ClientAllowed(const string& strAddress)
{
    if (strAddress == asio::ip::address_v4::loopback().to_string())
        return true;
    const vector<string>& vAllow = mapMultiArgs["-rpcallowip"];
    BOOST_FOREACH(string strAllow, vAllow)
        if (WildcardMatch(strAddress, strAllow))
            return true;
    return false;
}

#ifdef USE_SSL
//
// IOStream device that speaks SSL but can also speak non-SSL
//
class SSLIOStreamDevice : public iostreams::device<iostreams::bidirectional> {
public:
    SSLIOStreamDevice(SSLStream &streamIn, bool fUseSSLIn) : stream(streamIn)
    {
        fUseSSL = fUseSSLIn;
        fNeedHandshake = fUseSSLIn;
    }

    void handshake(ssl::stream_base::handshake_type role)
    {
        if (!fNeedHandshake) return;
        fNeedHandshake = false;
        stream.handshake(role);
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        handshake(ssl::stream_base::server); // HTTPS servers read first
        if (fUseSSL) return stream.read_some(asio::buffer(s, n));
        return stream.next_layer().read_some(asio::buffer(s, n));
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        handshake(ssl::stream_base::client); // HTTPS clients write first
        if (fUseSSL) return asio::write(stream, asio::buffer(s, n));
        return asio::write(stream.next_layer(), asio::buffer(s, n));
    }
    bool connect(const std::string& server, const std::string& port)
    {
        ip::tcp::resolver resolver(stream.get_io_service());
        ip::tcp::resolver::query query(server.c_str(), port.c_str());
        ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        ip::tcp::resolver::iterator end;
        boost::system::error_code error = asio::error::host_not_found;
        while (error && endpoint_iterator != end)
        {
            stream.lowest_layer().close();
            stream.lowest_layer().connect(*endpoint_iterator++, error);
        }
        if (error)
            return false;
        return true;
    }

private:
    bool fNeedHandshake;
    bool fUseSSL;
    SSLStream& stream;
};
#endif

/**
 * Class encapsulating the state necessary for writing to a client connection.
 * This is used to hold the state for async method calls.
 */
class ClientConnectionOutput
{

private:

    /* The stream for outputting.  */
#ifdef USE_SSL
  SSLStream* sslStream;
  SSLIOStreamDevice* d;
  iostreams::stream<SSLIOStreamDevice>* stream;
#else
  ip::tcp::iostream* stream;
#endif

public:

  /* Basic constructor.  */
#ifdef USE_SSL
  inline ClientConnectionOutput (asio::io_service& io, ssl::context& c,
                                 bool fUseSSL)
    : sslStream(new SSLStream (io, c)),
      d(new SSLIOStreamDevice (*sslStream, fUseSSL)),
      stream(new iostreams::stream<SSLIOStreamDevice> (*d))
  {}
#else
  inline ClientConnectionOutput ()
    : stream(new ip::tcp::iostream ())
  {}
#endif

  /* Destructor freeing everything.  */
  inline ~ClientConnectionOutput ()
  {
    delete stream;
#ifdef USE_SSL
    delete d;
    delete sslStream;
#endif
  }

  /* Wait for incoming connection.  */
  inline void
  waitForConnection (ip::tcp::acceptor& acc, ip::tcp::endpoint& peer)
  {
#ifdef USE_SSL
    acc.accept(sslStream->lowest_layer(), peer);
#else
    acc.accept(*stream->rdbuf(), peer);
#endif
  }

  /* Return the stream held.  */
#ifdef USE_SSL
  inline iostreams::stream<SSLIOStreamDevice>&
#else
  inline ip::tcp::iostream&
#endif
  getStream ()
  {
    return *stream;
  }

};

/* Execute an RPC call, can be used as thread object for async calls.  */
static void
ExecuteRpcCall (ClientConnectionOutput* out, rpcfn_type method,
                json_spirit::Array params, json_spirit::Value id)
{
  try
    {
      // Execute
      Value result = method (params, false);

      // Send reply
      string strReply = JSONRPCReply (result, json_spirit::Value::null, id);
      out->getStream () << HTTPReply (200, strReply) << std::flush;
    }
  catch (Object& objError)
    {
      ErrorReply (out->getStream (), objError, id);
    }
  catch (const boost::thread_interrupted& e)
    {
      ErrorReply (out->getStream (),
                  JSONRPCError (RPC_ASYNC_INTERRUPT,
                                "async method interrupted"), id);
    }
  catch (const std::exception& e)
    {
      ErrorReply (out->getStream (),
                  JSONRPCError (RPC_MISC_ERROR, e.what ()), id);
    }

  delete out;
}

void ThreadRPCServer(void* parg)
{
    IMPLEMENT_RANDOMIZE_STACK(ThreadRPCServer(parg));
    try
    {
        vnThreadsRunning[4]++;
        ThreadRPCServer2(parg);
        vnThreadsRunning[4]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[4]--;
        PrintException(&e, "ThreadRPCServer()");
    } catch (...) {
        vnThreadsRunning[4]--;
        PrintException(NULL, "ThreadRPCServer()");
    }
    printf("ThreadRPCServer exiting\n");
}

void ThreadRPCServer2(void* parg)
{
    printf("ThreadRPCServer started\n");

    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
    {
        string strWhatAmI = "To use namecoind";
        if (mapArgs.count("-server"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-server\"");
        else if (mapArgs.count("-daemon"))
            strWhatAmI = strprintf(_("To use the %s option"), "\"-daemon\"");
        std::string format_str = _("Warning: %s, you must set rpcpassword=<password>\nin the configuration file: %s\n"
              "If the file does not exist, create it with owner-readable-only file permissions.\n");
        PrintConsole(format_str.c_str(),
                strWhatAmI.c_str(),
                GetConfigFile().c_str());
        CreateThread(Shutdown, NULL);
        return;
    }

    bool fUseSSL = GetBoolArg("-rpcssl");
    asio::ip::address bindAddress = mapArgs.count("-rpcallowip") ? asio::ip::address_v4::any() : asio::ip::address_v4::loopback();

    asio::io_service io_service;
    ip::tcp::endpoint endpoint(bindAddress, GetArg("-rpcport", GetDefaultRPCPort()));
    ip::tcp::acceptor acceptor(io_service, endpoint);

    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

#ifdef USE_SSL
    ssl::context context(io_service, ssl::context::sslv23);
    if (fUseSSL)
    {
        context.set_options(ssl::context::no_sslv2);
        filesystem::path certfile = GetArg("-rpcsslcertificatechainfile", "server.cert");
        if (!certfile.is_complete()) certfile = filesystem::path(GetDataDir()) / certfile;
        if (filesystem::exists(certfile)) context.use_certificate_chain_file(certfile.string().c_str());
        else printf("ThreadRPCServer ERROR: missing server certificate file %s\n", certfile.string().c_str());
        filesystem::path pkfile = GetArg("-rpcsslprivatekeyfile", "server.pem");
        if (!pkfile.is_complete()) pkfile = filesystem::path(GetDataDir()) / pkfile;
        if (filesystem::exists(pkfile)) context.use_private_key_file(pkfile.string().c_str(), ssl::context::pem);
        else printf("ThreadRPCServer ERROR: missing server private key file %s\n", pkfile.string().c_str());

        string ciphers = GetArg("-rpcsslciphers",
                                         "TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!AH:!3DES:@STRENGTH");
        SSL_CTX_set_cipher_list(context.impl(), ciphers.c_str());
    }
#else
    if (fUseSSL)
        throw runtime_error("-rpcssl=1, but namecoin compiled without full openssl libraries.");
#endif

    // Threads running async methods at the moment.
    typedef std::list<boost::thread*> ThreadList;
    ThreadList asyncThreads;

    loop
    {
        /* Shut down.  Do this here before we block again in the accept call
           below, when the last command was "stop", it can exit now.  */
        if (fShutdown)
        {
            printf("Waiting for %d async RPC call threads to finish...\n",
                   asyncThreads.size());

            for (ThreadList::iterator i = asyncThreads.begin();
                 i != asyncThreads.end(); ++i)
            {
                (*i)->interrupt();
            }

            for (ThreadList::iterator i = asyncThreads.begin();
                 i != asyncThreads.end(); ++i)
            {
                (*i)->join();
                delete *i;
            }

            return;
        }

        // Clean up async threads.
        printf("Trying to clean up %d async RPC call threads...\n",
               asyncThreads.size());
        for (ThreadList::iterator i = asyncThreads.begin();
             i != asyncThreads.end(); )
        {
            if ((*i)->timed_join(boost::posix_time::seconds(0)))
            {
                printf("Async RPC call thread finished.\n");
                delete *i;
                i = asyncThreads.erase (i);
            }
            else
            {
                /* Explicitly increment iterator here in case we didn't
                   delete the element above.  */
                ++i;
            }
        }

        // Accept connection
        std::auto_ptr<ClientConnectionOutput> out;
#ifdef USE_SSL
        out.reset(new ClientConnectionOutput(io_service, context, fUseSSL));
#else
        out.reset(new ClientConnectionOutput());
#endif

        ip::tcp::endpoint peer;
        vnThreadsRunning[4]--;
        out->waitForConnection (acceptor, peer);
        vnThreadsRunning[4]++;

        // Restrict callers by IP
        if (!ClientAllowed(peer.address().to_string()))
        {
            // Only send a 403 if we're not using SSL to prevent a DoS during the SSL handshake.
            if (!fUseSSL)
                out->getStream() << HTTPReply(403, "") << std::flush;
            continue;
        }

        map<string, string> mapHeaders;
        string strRequest;

        boost::thread api_caller(ReadHTTP, boost::ref(out->getStream()),
                                 boost::ref(mapHeaders),
                                 boost::ref(strRequest));
        if (!api_caller.timed_join(boost::posix_time::seconds(GetArg("-rpctimeout", 30))))
        {   // Timed out:
            acceptor.cancel();
            printf("ThreadRPCServer ReadHTTP timeout\n");
            continue;
        }

        // Check authorization
        if (mapHeaders.count("authorization") == 0)
        {
            out->getStream() << HTTPReply(401, "") << std::flush;
            continue;
        }
        if (!HTTPAuthorized(mapHeaders))
        {
            // Deter brute-forcing short passwords
            if (mapArgs["-rpcpassword"].size() < 15)
                MilliSleep(50);

            out->getStream() << HTTPReply(401, "") << std::flush;
            printf("ThreadRPCServer incorrect password attempt\n");
            continue;
        }

        Value id = Value::null;
        try
        {
            // Parse request
            Value valRequest;
            if (!read_string(strRequest, valRequest) || valRequest.type() != obj_type)
                throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");
            const Object& request = valRequest.get_obj();

            // Parse id now so errors from here on will have the id
            id = find_value(request, "id");

            // Bail early if not yet initialised.
            if (rpcWarmupStatus)
              throw JSONRPCError (RPC_IN_WARMUP, rpcWarmupStatus);

            // Parse method
            Value valMethod = find_value(request, "method");
            if (valMethod.type() == null_type)
                throw JSONRPCError(RPC_INVALID_REQUEST, "Missing method");
            if (valMethod.type() != str_type)
                throw JSONRPCError(RPC_INVALID_REQUEST, "Method must be a string");
            string strMethod = valMethod.get_str();
            if (strMethod != "getwork" && strMethod != "getworkaux" && strMethod != "getauxblock" && strMethod != "buildmerkletree" && strMethod != "getmemorypool")
                printf("ThreadRPCServer method=%s\n", strMethod.c_str());

            // Parse params
            Value valParams = find_value(request, "params");
            Array params;
            if (valParams.type() == array_type)
                params = valParams.get_array();
            else if (valParams.type() == null_type)
                params = Array();
            else
                throw JSONRPCError(RPC_INVALID_REQUEST, "Params must be an array");

            // Find method
            map<string, rpcfn_type>::iterator mi = mapCallTable.find(strMethod);
            if (mi == mapCallTable.end())
                throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

            // Observe safe mode
            string strWarning = GetWarnings("rpc");
            if (strWarning != "" && !GetBoolArg("-disablesafemode") && !setAllowInSafeMode.count(strMethod))
                throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

            // Check for asynchronous execution and call the method.
            const bool async = (setCallAsync.count(strMethod) > 0);
            if (!async)
                ExecuteRpcCall(out.release(), (*mi).second, params, id);
            else
            {
                std::auto_ptr<boost::thread> runner;
                runner.reset (new boost::thread (&ExecuteRpcCall,
                                  out.release(),
                                  (*mi).second, params, id));
                asyncThreads.push_back (runner.release());
            }
        }
        catch (Object& objError)
        {
            ErrorReply(out->getStream(), objError, id);
        }
        catch (std::exception& e)
        {
            ErrorReply(out->getStream(),
                       JSONRPCError(RPC_PARSE_ERROR, e.what()), id);
        }
    }
}




Object CallRPC(const string& strMethod, const Array& params)
{
    if (mapArgs["-rpcuser"] == "" && mapArgs["-rpcpassword"] == "")
        throw runtime_error(strprintf(
            _("You must set rpcpassword=<password> in the configuration file:\n%s\n"
              "If the file does not exist, create it with owner-readable-only file permissions."),
                GetConfigFile().c_str()));

    // Connect to localhost
    bool fUseSSL = GetBoolArg("-rpcssl");
#ifdef USE_SSL
    asio::io_service io_service;
    ssl::context context(io_service, ssl::context::sslv23);
    context.set_options(ssl::context::no_sslv2);
    SSLStream sslStream(io_service, context);
    SSLIOStreamDevice d(sslStream, fUseSSL);
    iostreams::stream<SSLIOStreamDevice> stream(d);
    if (!d.connect(GetArg("-rpcconnect", "127.0.0.1"), GetArg("-rpcport", itostr(GetDefaultRPCPort()))))
        throw runtime_error("couldn't connect to server");
#else
    if (fUseSSL)
        throw runtime_error("-rpcssl=1, but namecoin compiled without full openssl libraries.");

    ip::tcp::iostream stream(GetArg("-rpcconnect", "127.0.0.1"), GetArg("-rpcport", itostr(GetDefaultRPCPort())));
    if (stream.fail())
        throw runtime_error("couldn't connect to server");
#endif


    // HTTP basic authentication
    string strUserPass64 = EncodeBase64(mapArgs["-rpcuser"] + ":" + mapArgs["-rpcpassword"]);
    map<string, string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = string("Basic ") + strUserPass64;

    // Send request
    string strRequest = JSONRPCRequest(strMethod, params, 1);
    string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive reply
    map<string, string> mapHeaders;
    string strReply;
    int nStatus = ReadHTTP(stream, mapHeaders, strReply);
    if (nStatus == 401)
        throw runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != 400 && nStatus != 404 && nStatus != 500)
        throw runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw runtime_error("no response from server");

    // Parse reply
    Value valReply;
    if (!read_string(strReply, valReply))
        throw runtime_error("couldn't parse reply from server");
    const Object& reply = valReply.get_obj();
    if (reply.empty())
        throw runtime_error("expected reply to have result, error and id properties");

    return reply;
}




template<typename T>
void ConvertTo(Value& value, bool fAllowNull=false)
{
    if (fAllowNull && value.type() == null_type)
        return;
    if (value.type() == str_type)
    {
        // reinterpret string as unquoted json value
        Value value2;
        string strJSON = value.get_str();
        if (!read_string(strJSON, value2))
            throw runtime_error(string("Error parsing JSON:")+strJSON);
        ConvertTo<T>(value2, fAllowNull);
        value = value2;
    }
    else
    {
        value = value.get_value<T>();
    }
}

// Convert strings to command-specific RPC representation
Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    Array params;
    BOOST_FOREACH(const std::string &param, strParams)
        params.push_back(param);
    RPCConvertValues(strMethod, params);
    return params;
}

void RPCConvertValues(const std::string &strMethod, json_spirit::Array &params)
{
    int n = params.size();

    //
    // Special case non-string parameter types
    //
    if (strMethod == "name_filter"            && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "name_filter"            && n > 2) ConvertTo<boost::int64_t>(params[2]);
    if (strMethod == "name_filter"            && n > 3) ConvertTo<boost::int64_t>(params[3]);
    if (strMethod == "sendtoname"             && n > 1) ConvertTo<double>(params[1]);

    if (strMethod == "setgenerate"            && n > 0) ConvertTo<bool>(params[0]);
    if (strMethod == "setgenerate"            && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "sendtoaddress"          && n > 1) ConvertTo<double>(params[1]);
    if (strMethod == "settxfee"               && n > 0) ConvertTo<double>(params[0]);
    if (strMethod == "getamountreceived"      && n > 1) ConvertTo<boost::int64_t>(params[1]); // deprecated
    if (strMethod == "setmininput"            && n > 0) ConvertTo<double>(params[0]);
    if (strMethod == "getreceivedbyaddress"   && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "getreceivedbyaccount"   && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "getreceivedbylabel"     && n > 1) ConvertTo<boost::int64_t>(params[1]); // deprecated
    if (strMethod == "getallreceived"         && n > 0) ConvertTo<boost::int64_t>(params[0]); // deprecated
    if (strMethod == "getallreceived"         && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "listreceivedbyaddress"  && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "listreceivedbyaddress"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "listreceivedbyaccount"  && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "listreceivedbyaccount"  && n > 1) ConvertTo<bool>(params[1]);
    if (strMethod == "listreceivedbylabel"    && n > 0) ConvertTo<boost::int64_t>(params[0]); // deprecated
    if (strMethod == "listreceivedbylabel"    && n > 1) ConvertTo<bool>(params[1]); // deprecated
    if (strMethod == "getbalance"             && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "move"                   && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "move"                   && n > 3) ConvertTo<boost::int64_t>(params[3]);
    if (strMethod == "sendfrom"               && n > 2) ConvertTo<double>(params[2]);
    if (strMethod == "sendfrom"               && n > 3) ConvertTo<boost::int64_t>(params[3]);
    if (strMethod == "listtransactions"       && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "listtransactions"       && n > 2) ConvertTo<boost::int64_t>(params[2]);
    if (strMethod == "walletpassphrase"       && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "getworkaux"             && n > 2) ConvertTo<boost::int64_t>(params[2]);
    if (strMethod == "listaccounts"           && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "getblockbycount"        && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "getblockhash"           && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "sendmany"               && n > 1)
    {
        string s = params[1].get_str();
        Value v;
        if (!read_string(s, v) || v.type() != obj_type)
            throw runtime_error("type mismatch");
        params[1] = v.get_obj();
    }
    if (strMethod == "sendmany"               && n > 2) ConvertTo<boost::int64_t>(params[2]);
    if (strMethod == "importprivkey"          && n > 2) ConvertTo<bool>(params[2]);
    if (strMethod == "importaddress"          && n > 2) ConvertTo<bool>(params[2]);
    if (strMethod == "listunspent"            && n > 0) ConvertTo<boost::int64_t>(params[0]);
    if (strMethod == "listunspent"            && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "listunspent"            && n > 2) ConvertTo<Array>(params[2]);
    if (strMethod == "getrawtransaction"      && n > 1) ConvertTo<boost::int64_t>(params[1]);
    if (strMethod == "createrawtransaction"   && n > 0) ConvertTo<Array>(params[0]);
    if (strMethod == "createrawtransaction"   && n > 1) ConvertTo<Object>(params[1]);
    if (strMethod == "createrawtransaction"   && n > 2) ConvertTo<Object>(params[2]);
    if (strMethod == "signrawtransaction"     && n > 1) ConvertTo<Array>(params[1], true);
    if (strMethod == "signrawtransaction"     && n > 2) ConvertTo<Array>(params[2], true);
    if (strMethod == "listsinceblock"         && n > 1) ConvertTo<boost::int64_t>(params[1]);
}

int CommandLineRPC(int argc, char *argv[])
{
    string strPrint;
    int nRet = 0;
    try
    {
        // Skip switches
        while (argc > 1 && IsSwitchChar(argv[1][0]))
        {
            argc--;
            argv++;
        }

        // Method
        if (argc < 2)
            throw runtime_error("too few parameters");
        string strMethod = argv[1];

        // Parameters default to strings
        std::vector<std::string> strParams(&argv[2], &argv[argc]);
        Array params = RPCConvertValues(strMethod, strParams);

        // Execute
        Object reply = CallRPC(strMethod, params);

        // Parse reply
        const Value& result = find_value(reply, "result");
        const Value& error  = find_value(reply, "error");
        const Value& id     = find_value(reply, "id");

        if (error.type() != null_type)
        {
            // Error
            strPrint = "error: " + write_string(error, false);
            int code = find_value(error.get_obj(), "code").get_int();
            nRet = abs(code);
        }
        else
        {
            // Result
            if (result.type() == null_type)
                strPrint = "";
            else if (result.type() == str_type)
                strPrint = result.get_str();
            else
                strPrint = write_string(result, true);
        }
    }
    catch (std::exception& e)
    {
        strPrint = string("error: ") + e.what();
        nRet = 87;
    }
    catch (...)
    {
        PrintException(NULL, "CommandLineRPC()");
    }

    if (strPrint != "")
    {
#if defined(__WXMSW__) && defined(GUI)
        // Windows GUI apps can't print to command line,
        // so settle for a message box yuck
        MyMessageBox(strPrint, "Namecoin", wxOK);
#else
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
#endif
    }
    return nRet;
}

json_spirit::Value ExecuteRPC(const std::string &strMethod, const std::vector<std::string> &vParams)
{
    Array params = RPCConvertValues(strMethod, vParams);
    return ExecuteRPC(strMethod, params);
}

json_spirit::Value ExecuteRPC(const std::string &strMethod, const Array &params)
{
    // Find method
    map<string, rpcfn_type>::iterator mi = mapCallTable.find(strMethod);
    if (mi == mapCallTable.end())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");

    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode") && !setAllowInSafeMode.count(strMethod))
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);

    // Execute
    Value result = (*(*mi).second)(params, false);
    return result;
}


#ifdef TEST
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
    // Turn off microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFile("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    try
    {
        if (argc >= 2 && string(argv[1]) == "-server")
        {
            printf("server ready\n");
            ThreadRPCServer(NULL);
        }
        else
        {
            return CommandLineRPC(argc, argv);
        }
    }
    catch (std::exception& e) {
        PrintException(&e, "main()");
    } catch (...) {
        PrintException(NULL, "main()");
    }
    return 0;
}
#endif
