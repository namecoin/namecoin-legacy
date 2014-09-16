// Copyright (c) 2010-2011 Vincent Durham
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
#include "headers.h"
#include "db.h"
#include "keystore.h"
#include "wallet.h"
#include "init.h"
#include "auxpow.h"
#include "namecoin.h"

#include "bitcoinrpc.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"
#include <boost/xpressive/xpressive_dynamic.hpp>

using namespace std;
using namespace json_spirit;

static const bool NAME_DEBUG = false;
extern int64 AmountFromValue(const Value& value);
extern Object JSONRPCError(int code, const string& message);
template<typename T> void ConvertTo(Value& value, bool fAllowNull=false);

static const int BUG_WORKAROUND_BLOCK_START = 139750;   // Bug was not exploited before block 139872, so skip checking earlier blocks
static const int BUG_WORKAROUND_BLOCK = 150000;         // Point of hard fork

map<vector<unsigned char>, uint256> mapMyNames;
map<vector<unsigned char>, set<uint256> > mapNamePending;

#ifdef GUI
extern std::map<uint160, std::vector<unsigned char> > mapMyNameHashes;
#endif

extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);

// forward decls
extern bool Solver(const CKeyStore& keystore, const CScript& scriptPubKey, uint256 hash, int nHashType, CScript& scriptSigRet);
extern bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CTransaction& txTo, unsigned int nIn, int nHashType);
extern bool IsConflictedTx (DatabaseSet& dbset, const CTransaction& tx, vchType& name);
extern void rescanfornames();
extern Value sendtoaddress(const Array& params, bool fHelp);

const int NAME_COIN_GENESIS_EXTRA = 521;
uint256 hashNameCoinGenesisBlock("000000000062b72c5e2ceb45fbc8587e807c155b0da735e6483dfba2f0a9c770");

class CNamecoinHooks : public CHooks
{
public:
    virtual bool IsStandard(const CScript& scriptPubKey);
    virtual void AddToWallet(CWalletTx& tx);
    virtual bool CheckTransaction(const CTransaction& tx);
    virtual bool ConnectInputs(DatabaseSet& dbset,
            map<uint256, CTxIndex>& mapTestPool,
            const CTransaction& tx,
            vector<CTransaction>& vTxPrev,
            vector<CTxIndex>& vTxindex,
            CBlockIndex* pindexBlock,
            CDiskTxPos& txPos,
            bool fBlock,
            bool fMiner);
    virtual bool DisconnectInputs (DatabaseSet& dbset,
            const CTransaction& tx,
            CBlockIndex* pindexBlock);
    virtual bool ConnectBlock (CBlock& block, DatabaseSet& dbset,
                               CBlockIndex* pindex);
    virtual bool DisconnectBlock (CBlock& block, DatabaseSet& dbset,
                                  CBlockIndex* pindex);
    virtual bool ExtractAddress(const CScript& script, string& address);
    virtual bool GenesisBlock(CBlock& block);
    virtual bool Lockin(int nHeight, uint256 hash);
    virtual int LockinHeight();
    virtual string IrcPrefix();
    virtual void AcceptToMemoryPool(DatabaseSet& dbset, const CTransaction& tx);
    virtual void RemoveFromMemoryPool(const CTransaction& tx);

    virtual void MessageStart(char* pchMessageStart)
    {
        // Make the message start different
        pchMessageStart[3] = 0xfe;
    }
    virtual bool IsMine(const CTransaction& tx);
    virtual bool IsMine(const CTransaction& tx, const CTxOut& txout, bool ignore_name_new = false);
    virtual int GetOurChainID()
    {
        return 0x0001;
    }

    virtual int GetAuxPowStartBlock()
    {
        if (fTestNet)
            return 0;
        return 19200;
    }

    virtual int GetFullRetargetStartBlock()
    {
        if (fTestNet)
            return 0;
        return 19200;
    }

    string GetAlertPubkey1()
    {
        return "04ba207043c1575208f08ea6ac27ed2aedd4f84e70b874db129acb08e6109a3bbb7c479ae22565973ebf0ac0391514511a22cb9345bdb772be20cfbd38be578b0c";
    }

    string GetAlertPubkey2()
    {
        return "04fc4366270096c7e40adb8c3fcfbff12335f3079e5e7905bce6b1539614ae057ee1e61a25abdae4a7a2368505db3541cd81636af3f7c7afe8591ebc85b2a1acdd";
    }
};

int64 getAmount(Value value)
{
    ConvertTo<double>(value);
    double dAmount = value.get_real();
    int64 nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    return nAmount;
}

vchType
vchFromValue (const Value& value)
{
  const std::string str = value.get_str ();
  return vchFromString (str);
}

vchType
vchFromString (const std::string& str)
{
  const unsigned char* strbeg;
  strbeg = reinterpret_cast<const unsigned char*> (str.c_str ());
  return vchType(strbeg, strbeg + str.size ());
}

string stringFromVch(const vector<unsigned char> &vch) {
    string res;
    vector<unsigned char>::const_iterator vi = vch.begin();
    while (vi != vch.end()) {
        res += (char)(*vi);
        vi++;
    }
    return res;
}

// Increase expiration to 36000 gradually starting at block 24000.
// Use for validation purposes and pass the chain height.
int GetExpirationDepth(int nHeight) {
    if (nHeight < 24000)
        return 12000;
    if (nHeight < 48000)
        return nHeight - 12000;
    return 36000;
}

// For display purposes, pass the name height.
int GetDisplayExpirationDepth(int nHeight) {
    if (nHeight < 12000)
        return 12000;
    return 36000;
}

int64 GetNetworkFee(int nHeight)
{
    // Speed up network fee decrease 4x starting at 24000
    if (nHeight >= 24000)
        nHeight += (nHeight - 24000) * 3;
    if ((nHeight >> 13) >= 60)
        return 0;
    int64 nStart = 50 * COIN;
    if (fTestNet)
        nStart = 10 * CENT;
    int64 nRes = nStart >> (nHeight >> 13);
    nRes -= (nRes >> 14) * (nHeight % 8192);
    return nRes;
}

int GetTxPosHeight(const CNameIndex& txPos)
{
    return txPos.nHeight;
}

int GetTxPosHeight(const CDiskTxPos& txPos)
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(txPos.nFile, txPos.nBlockPos, false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return pindex->nHeight;
}

int GetTxPosHeight2(const CDiskTxPos& txPos, int nHeight)
{
    nHeight = GetTxPosHeight(txPos);
    return nHeight;
}


int
GetNameHeight (DatabaseSet& dbset, vector<unsigned char> vchName)
{
  vector<CNameIndex> vtxPos;
  if (dbset.name ().ExistsName (vchName))
    {
      if (!dbset.name ().ReadName (vchName, vtxPos))
        return error("GetNameHeight() : failed to read from name DB");
      if (vtxPos.empty ())
        return -1;

      CNameIndex& txPos = vtxPos.back ();
      return GetTxPosHeight (txPos);
    }

  return -1;
}

CScript RemoveNameScriptPrefix(const CScript& scriptIn)
{
    int op;
    vector<vector<unsigned char> > vvch;
    CScript::const_iterator pc = scriptIn.begin();

    if (!DecodeNameScript(scriptIn, op, vvch,  pc))
        throw runtime_error("RemoveNameScriptPrefix() : could not decode name script");
    return CScript(pc, scriptIn.end());
}

bool IsMyName(const CTransaction& tx, const CTxOut& txout)
{
    const CScript& scriptPubKey = RemoveNameScriptPrefix(txout.scriptPubKey);
    CScript scriptSig;
    if (!Solver(*pwalletMain, scriptPubKey, 0, 0, scriptSig))
        return false;
    return true;
}

bool CreateTransactionWithInputTx(const vector<pair<CScript, int64> >& vecSend, const CWalletTx& wtxIn, int nTxOut, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet)
{
    int64 nValue = 0;
    BOOST_FOREACH(const PAIRTYPE(CScript, int64)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    wtxNew.pwallet = pwalletMain;

    CRITICAL_BLOCK(cs_main)
    {
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
        {
            nFeeRet = nTransactionFee;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                int64 nTotalValue = nValue + nFeeRet;
                printf("CreateTransactionWithInputTx: total value = %d\n", nTotalValue);
                double dPriority = 0;
                // vouts to the payees
                BOOST_FOREACH(const PAIRTYPE(CScript, int64)& s, vecSend)
                    wtxNew.vout.push_back(CTxOut(s.second, s.first));

                int64 nWtxinCredit = wtxIn.vout[nTxOut].nValue;

                // Choose coins to use
                set<pair<const CWalletTx*, unsigned int> > setCoins;
                int64 nValueIn = 0;
                printf("CreateTransactionWithInputTx: SelectCoins(%s), nTotalValue = %s, nWtxinCredit = %s\n", FormatMoney(nTotalValue - nWtxinCredit).c_str(), FormatMoney(nTotalValue).c_str(), FormatMoney(nWtxinCredit).c_str());
                if (nTotalValue - nWtxinCredit > 0)
                {
                    if (!pwalletMain->SelectCoins(nTotalValue - nWtxinCredit, setCoins, nValueIn))
                        return false;
                }

                printf("CreateTransactionWithInputTx: selected %d tx outs, nValueIn = %s\n", setCoins.size(), FormatMoney(nValueIn).c_str());

                vector<pair<const CWalletTx*, unsigned int> >
                    vecCoins(setCoins.begin(), setCoins.end());

                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int)& coin, vecCoins)
                {
                    int64 nCredit = coin.first->vout[coin.second].nValue;
                    dPriority += (double)nCredit * coin.first->GetDepthInMainChain();
                }

                // Input tx always at first position
                vecCoins.insert(vecCoins.begin(), make_pair(&wtxIn, nTxOut));

                nValueIn += nWtxinCredit;
                dPriority += (double)nWtxinCredit * wtxIn.GetDepthInMainChain();

                // Fill a vout back to self with any change
                int64 nChange = nValueIn - nTotalValue;
                if (nChange >= CENT)
                {
                    // Note: We use a new key here to keep it from being obvious which side is the change.
                    //  The drawback is that by not reusing a previous key, the change may be lost if a
                    //  backup is restored, if the backup doesn't have the new private key for the change.
                    //  If we reused the old key, it would be possible to add code to look for and
                    //  rediscover unknown transactions that were written with keys of ours to recover
                    //  post-backup change.

                    // Reserve a new key pair from key pool
                    vector<unsigned char> vchPubKey = reservekey.GetReservedKey();
                    assert(pwalletMain->HaveKey(vchPubKey));

                    // -------------- Fill a vout to ourself, using same address type as the payment
                    // Now sending always to hash160 (GetBitcoinAddressHash160 will return hash160, even if pubkey is used)
                    CScript scriptChange;
                    if (vecSend[0].first.GetBitcoinAddressHash160() != 0)
                        scriptChange.SetBitcoinAddress(vchPubKey);
                    else
                        scriptChange << vchPubKey << OP_CHECKSIG;

                    // Insert change txn at random position:
                    vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size());
                    wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
                }
                else
                    reservekey.ReturnKey();

                // Fill vin
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int)& coin, vecCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int)& coin, vecCoins)
                {
                    if (!SignSignature(*pwalletMain, *coin.first, wtxNew, nIn++))
                        return false;
                }

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK);
                if (nBytes >= MAX_BLOCK_SIZE_GEN/5)
                    return false;
                dPriority /= nBytes;

                // Check that enough fee is included
                int64 nPayFee = nTransactionFee * (1 + (int64)nBytes / 1000);
                bool fAllowFree = CTransaction::AllowFree(dPriority);
                int64 nMinFee = wtxNew.GetMinFee(1, fAllowFree);
                if (nFeeRet < max(nPayFee, nMinFee))
                {
                    nFeeRet = max(nPayFee, nMinFee);
                    printf("CreateTransactionWithInputTx: re-iterating (nFreeRet = %s)\n", FormatMoney(nFeeRet).c_str());
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    printf("CreateTransactionWithInputTx succeeded:\n%s", wtxNew.ToString().c_str());
    return true;
}

// nTxOut is the output from wtxIn that we should grab
// requires cs_main lock
string SendMoneyWithInputTx(const CScript& scriptPubKey, int64 nValue, int64 nNetFee, const CWalletTx& wtxIn, CWalletTx& wtxNew, bool fAskFee)
{
    int nTxOut = IndexOfNameOutput(wtxIn);
    CReserveKey reservekey(pwalletMain);
    int64 nFeeRequired;
    vector< pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    if (nNetFee)
    {
        CScript scriptFee;
        scriptFee << OP_RETURN;
        vecSend.push_back(make_pair(scriptFee, nNetFee));
    }

    if (!CreateTransactionWithInputTx(vecSend, wtxIn, nTxOut, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendMoney() : %s", strError.c_str());
        return strError;
    }

#ifdef GUI
    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired))
        return "ABORTED";
#else
    if (fAskFee && !ThreadSafeAskFee(nFeeRequired, "Namecoin", NULL))
        return "ABORTED";
#endif

    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

bool GetValueOfTxPos(const CNameIndex& txPos, vector<unsigned char>& vchValue, uint256& hash, int& nHeight)
{
    nHeight = GetTxPosHeight(txPos);
    vchValue = txPos.vValue;
    CTransaction tx;
    if (!tx.ReadFromDisk(txPos.txPos))
        return error("GetValueOfTxPos() : could not read tx from disk");
    hash = tx.GetHash();
    return true;
}

bool GetValueOfTxPos(const CDiskTxPos& txPos, vector<unsigned char>& vchValue, uint256& hash, int& nHeight)
{
    nHeight = GetTxPosHeight(txPos);
    CTransaction tx;
    if (!tx.ReadFromDisk(txPos))
        return error("GetValueOfTxPos() : could not read tx from disk");
    if (!GetValueOfNameTx(tx, vchValue))
        return error("GetValueOfTxPos() : could not decode value from tx");
    hash = tx.GetHash();
    return true;
}

bool GetValueOfName(CNameDB& dbName, const vector<unsigned char> &vchName, vector<unsigned char>& vchValue, int& nHeight)
{
    //vector<CDiskTxPos> vtxPos;
    vector<CNameIndex> vtxPos;
    if (!dbName.ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;
    /*CDiskTxPos& txPos = vtxPos.back();

    uint256 hash;

    return GetValueOfTxPos(txPos, vchValue, hash, nHeight);*/

    CNameIndex& txPos = vtxPos.back();
    nHeight = txPos.nHeight;
    vchValue = txPos.vValue;
    return true;
}

bool GetTxOfName(CNameDB& dbName, const vector<unsigned char> &vchName, CTransaction& tx)
{
    //vector<CDiskTxPos> vtxPos;
    vector<CNameIndex> vtxPos;
    if (!dbName.ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;
    //CDiskTxPos& txPos = vtxPos.back();
    CNameIndex& txPos = vtxPos.back();
    //int nHeight = GetTxPosHeight(txPos);
    int nHeight = txPos.nHeight;
    if (nHeight + GetExpirationDepth(pindexBest->nHeight) < pindexBest->nHeight)
    {
        string name = stringFromVch(vchName);
        printf("GetTxOfName(%s) : expired", name.c_str());
        return false;
    }

    if (!tx.ReadFromDisk(txPos.txPos))
        return error("GetTxOfName() : could not read tx from disk");
    return true;
}

bool GetNameAddress(const CTransaction& tx, std::string& strAddress)
{
    int op;
    int nOut;
    vector<vector<unsigned char> > vvch;
    if (!DecodeNameTx(tx, op, nOut, vvch, BUG_WORKAROUND_BLOCK))
        return false;
    const CTxOut& txout = tx.vout[nOut];
    const CScript& scriptPubKey = RemoveNameScriptPrefix(txout.scriptPubKey);
    strAddress = scriptPubKey.GetBitcoinAddress();
    return true;
}

bool GetNameAddress(const CDiskTxPos& txPos, std::string& strAddress)
{
    CTransaction tx;
    if (!tx.ReadFromDisk(txPos))
        return error("GetNameAddress() : could not read tx from disk");

    return GetNameAddress(tx, strAddress);
}

Value sendtoname(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendtoname <namecoinname> <amount> [comment] [comment-to]\n"
            "<amount> is a real and is rounded to the nearest 0.01"
            + HelpRequiringPassphrase());

    vector<unsigned char> vchName = vchFromValue(params[0]);
    CNameDB dbName("r");
    if (!dbName.ExistsName(vchName))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Name not found");

    string strAddress;
    CTransaction tx;
    GetTxOfName(dbName, vchName, tx);
    GetNameAddress(tx, strAddress);

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No valid namecoin address");

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

Value name_list(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "name_list [<name>]\n"
                "list my own names"
                );

    vchType vchNameUniq;
    if (params.size () == 1)
      vchNameUniq = vchFromValue (params[0]);

    std::map<vchType, int> vNamesI;
    std::map<vchType, Object> vNamesO;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
      {
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item,
                      pwalletMain->mapWallet)
          {
            const CWalletTx& tx = item.second;

            vchType vchName, vchValue;
            int nOut;
            if (!tx.GetNameUpdate (nOut, vchName, vchValue))
              continue;

            if(!vchNameUniq.empty () && vchNameUniq != vchName)
              continue;

            const int nHeight = tx.GetHeightInMainChain ();
            if (nHeight == -1)
              continue;
            assert (nHeight >= 0);

            // get last active name only
            if (vNamesI.find (vchName) != vNamesI.end ()
                && vNamesI[vchName] > nHeight)
              continue;

            Object oName;
            oName.push_back(Pair("name", stringFromVch(vchName)));
            oName.push_back(Pair("value", stringFromVch(vchValue)));
            if (!hooks->IsMine (tx))
                oName.push_back(Pair("transferred", 1));
            string strAddress = "";
            GetNameAddress(tx, strAddress);
            oName.push_back(Pair("address", strAddress));

            const int expiresIn = nHeight + GetDisplayExpirationDepth (nHeight) - pindexBest->nHeight;
            oName.push_back (Pair("expires_in", expiresIn));
            if (expiresIn <= 0)
              oName.push_back (Pair("expired", 1));

            vNamesI[vchName] = nHeight;
            vNamesO[vchName] = oName;
        }
    }

    Array oRes;
    BOOST_FOREACH(const PAIRTYPE(vector<unsigned char>, Object)& item, vNamesO)
        oRes.push_back(item.second);

    return oRes;
}

Value name_debug(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 0)
        throw runtime_error(
            "name_debug\n"
            "Dump pending transactions id in the debug file.\n");

    printf("Pending:\n----------------------------\n");
    pair<vector<unsigned char>, set<uint256> > pairPending;

    CRITICAL_BLOCK(cs_main)
        BOOST_FOREACH(pairPending, mapNamePending)
        {
            string name = stringFromVch(pairPending.first);
            printf("%s :\n", name.c_str());
            uint256 hash;
            BOOST_FOREACH(hash, pairPending.second)
            {
                printf("    ");
                if (!pwalletMain->mapWallet.count(hash))
                    printf("foreign ");
                printf("    %s\n", hash.GetHex().c_str());
            }
        }
    printf("----------------------------\n");
    return true;
}

Value name_debug1(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1)
        throw runtime_error(
            "name_debug1 <name>\n"
            "Dump name blocks number and transactions id in the debug file.\n");

    vector<unsigned char> vchName = vchFromValue(params[0]);
    printf("Dump name:\n");
    CRITICAL_BLOCK(cs_main)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
        {
            error("failed to read from name DB");
            return false;
        }
        //CDiskTxPos txPos;
        CNameIndex txPos;
        BOOST_FOREACH(txPos, vtxPos)
        {
            CTransaction tx;
            if (!tx.ReadFromDisk(txPos.txPos))
            {
                error("could not read txpos %s", txPos.txPos.ToString().c_str());
                continue;
            }
            printf("@%d %s\n", GetTxPosHeight(txPos), tx.GetHash().GetHex().c_str());
        }
    }
    printf("-------------------------\n");
    return true;
}

Value name_show(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "name_show <name>\n"
            "Show values of a name.\n"
            );

    Object oLastName;
    vector<unsigned char> vchName = vchFromValue(params[0]);
    string name = stringFromVch(vchName);
    CRITICAL_BLOCK(cs_main)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from name DB");

        if (vtxPos.size() < 1)
            throw JSONRPCError(RPC_WALLET_ERROR, "no result returned");

        CDiskTxPos txPos = vtxPos[vtxPos.size() - 1].txPos;
        CTransaction tx;
        if (!tx.ReadFromDisk(txPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from from disk");

        Object oName;
        vector<unsigned char> vchValue;
        int nHeight;
        uint256 hash;
        if (!txPos.IsNull() && GetValueOfTxPos(txPos, vchValue, hash, nHeight))
        {
            oName.push_back(Pair("name", name));
            string value = stringFromVch(vchValue);
            oName.push_back(Pair("value", value));
            oName.push_back(Pair("txid", tx.GetHash().GetHex()));
            string strAddress = "";
            GetNameAddress(txPos, strAddress);
            oName.push_back(Pair("address", strAddress));
            oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
            if(nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
            {
                oName.push_back(Pair("expired", 1));
            }
            oLastName = oName;
        }
    }
    return oLastName;
}

Value name_history(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "name_history <name>\n"
            "List all name values of a name.\n");

    Array oRes;
    vector<unsigned char> vchName = vchFromValue(params[0]);
    string name = stringFromVch(vchName);
    CRITICAL_BLOCK(cs_main)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
            throw JSONRPCError(RPC_WALLET_ERROR, "failed to read from name DB");

        CNameIndex txPos2;
        CDiskTxPos txPos;
        BOOST_FOREACH(txPos2, vtxPos)
        {
            txPos = txPos2.txPos;
            CTransaction tx;
            if (!tx.ReadFromDisk(txPos))
            {
                error("could not read txpos %s", txPos.ToString().c_str());
                continue;
            }

            Object oName;
            vector<unsigned char> vchValue;
            int nHeight;
            uint256 hash;
            if (!txPos.IsNull() && GetValueOfTxPos(txPos, vchValue, hash, nHeight))
            {
                oName.push_back(Pair("name", name));
                string value = stringFromVch(vchValue);
                oName.push_back(Pair("value", value));
                oName.push_back(Pair("txid", tx.GetHash().GetHex()));
                string strAddress = "";
                GetNameAddress(txPos, strAddress);
                oName.push_back(Pair("address", strAddress));
                oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
                if(nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
                {
                    oName.push_back(Pair("expired", 1));
                }
                oRes.push_back(oName);
            }
        }
    }
    return oRes;
}

Value name_filter(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 5)
        throw runtime_error(
                "name_filter [[[[[regexp] maxage=36000] from=0] nb=0] stat]\n"
                "scan and filter names\n"
                "[regexp] : apply [regexp] on names, empty means all names\n"
                "[maxage] : look in last [maxage] blocks\n"
                "[from] : show results from number [from]\n"
                "[nb] : show [nb] results, 0 means all\n"
                "[stats] : show some stats instead of results\n"
                "name_filter \"\" 5 # list names updated in last 5 blocks\n"
                "name_filter \"^id/\" # list all names from the \"id\" namespace\n"
                "name_filter \"^id/\" 36000 0 0 stat # display stats (number of names) on active names from the \"id\" namespace\n"
                );

    string strRegexp;
    int nFrom = 0;
    int nNb = 0;
    int nMaxAge = 36000;
    bool fStat = false;
    int nCountFrom = 0;
    int nCountNb = 0;


    if (params.size() > 0)
        strRegexp = params[0].get_str();

    if (params.size() > 1)
        nMaxAge = params[1].get_int();

    if (params.size() > 2)
        nFrom = params[2].get_int();

    if (params.size() > 3)
        nNb = params[3].get_int();

    if (params.size() > 4)
        fStat = (params[4].get_str() == "stat" ? true : false);


    CNameDB dbName("r");
    Array oRes;

    vector<unsigned char> vchName;
    vector<pair<vector<unsigned char>, CNameIndex> > nameScan;
    if (!dbName.ScanNames(vchName, 100000000, nameScan))
        throw JSONRPCError(RPC_WALLET_ERROR, "scan failed");

    // compile regex once
    using namespace boost::xpressive;
    smatch nameparts;
    sregex cregex = sregex::compile(strRegexp);

    pair<vector<unsigned char>, CNameIndex> pairScan;
    BOOST_FOREACH(pairScan, nameScan)
    {
        string name = stringFromVch(pairScan.first);

        // regexp
        if(strRegexp != "" && !regex_search(name, nameparts, cregex))
            continue;

        CNameIndex txName = pairScan.second;
        int nHeight = txName.nHeight;

        // max age
        if(nMaxAge != 0 && pindexBest->nHeight - nHeight >= nMaxAge)
            continue;

        // from limits
        nCountFrom++;
        if(nCountFrom < nFrom + 1)
            continue;

        Object oName;
        if (!fStat) {
            oName.push_back(Pair("name", name));
	        int nExpiresIn = nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight;
            if (nExpiresIn <= 0)
            {
                oName.push_back(Pair("expired", 1));
            }
            else
            {
                string value = stringFromVch(txName.vValue);
                oName.push_back(Pair("value", value));
                oName.push_back(Pair("expires_in", nExpiresIn));
            }
        }
        oRes.push_back(oName);

        nCountNb++;
        // nb limits
        if(nNb > 0 && nCountNb >= nNb)
            break;
    }

    if (NAME_DEBUG) {
        dbName.test();
    }

    if (fStat)
    {
        Object oStat;
        oStat.push_back(Pair("blocks",    (int)nBestHeight));
        oStat.push_back(Pair("count",     (int)oRes.size()));
        //oStat.push_back(Pair("sha256sum", SHA256(oRes), true));
        return oStat;
    }

    return oRes;
}

Value name_scan(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
                "name_scan [<start-name>] [<max-returned>]\n"
                "scan all names, starting at start-name and returning a maximum number of entries (default 500)\n"
                );

    vector<unsigned char> vchName;
    int nMax = 500;
    if (params.size() > 0)
    {
        vchName = vchFromValue(params[0]);
    }

    if (params.size() > 1)
    {
        Value vMax = params[1];
        ConvertTo<double>(vMax);
        nMax = (int)vMax.get_real();
    }

    CNameDB dbName("r");
    Array oRes;

    //vector<pair<vector<unsigned char>, CDiskTxPos> > nameScan;
    vector<pair<vector<unsigned char>, CNameIndex> > nameScan;
    if (!dbName.ScanNames(vchName, nMax, nameScan))
        throw JSONRPCError(RPC_WALLET_ERROR, "scan failed");

    //pair<vector<unsigned char>, CDiskTxPos> pairScan;
    pair<vector<unsigned char>, CNameIndex> pairScan;
    BOOST_FOREACH(pairScan, nameScan)
    {
        Object oName;
        string name = stringFromVch(pairScan.first);
        oName.push_back(Pair("name", name));
        //vector<unsigned char> vchValue;
        CTransaction tx;
        CNameIndex txName = pairScan.second;
        CDiskTxPos txPos = txName.txPos;
        //CDiskTxPos txPos = pairScan.second;
        //int nHeight = GetTxPosHeight(txPos);
        int nHeight = txName.nHeight;
        vector<unsigned char> vchValue = txName.vValue;
        if ((nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
            || txPos.IsNull()
            || !tx.ReadFromDisk(txPos))
            //|| !GetValueOfNameTx(tx, vchValue))
        {
            oName.push_back(Pair("expired", 1));
        }
        else
        {
            string value = stringFromVch(vchValue);
            //string strAddress = "";
            //GetNameAddress(tx, strAddress);
            oName.push_back(Pair("value", value));
            //oName.push_back(Pair("txid", tx.GetHash().GetHex()));
            //oName.push_back(Pair("address", strAddress));
            oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
        }
        oRes.push_back(oName);
    }

    if (NAME_DEBUG) {
        dbName.test();
    }
    return oRes;
}

Value name_firstupdate(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
                "name_firstupdate <name> <rand> [<tx>] <value> [<toaddress>]\n"
                "Perform a first update after a name_new reservation.\n"
                "Note that the first update will go into a block 12 blocks after the name_new, at the soonest."
                + HelpRequiringPassphrase());

    const vchType vchName = vchFromValue (params[0]);
    const vchType vchRand = ParseHex (params[1].get_str ());

    vchType vchValue;
    if (params.size () == 3)
      vchValue = vchFromValue (params[2]);
    else
      vchValue = vchFromValue (params[3]);

    if (vchValue.size () > UI_MAX_VALUE_LENGTH)
      throw JSONRPCError(RPC_INVALID_PARAMETER, "the value is too long");

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    CRITICAL_BLOCK(cs_main)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_firstupdate() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            throw runtime_error("there are pending operations on that name");
        }
    }

    {
        CNameDB dbName("r");
        CTransaction tx;
        if (GetTxOfName(dbName, vchName, tx))
        {
            error("name_firstupdate() : this name is already active with tx %s",
                    tx.GetHash().GetHex().c_str());
            throw runtime_error("this name is already active");
        }
    }

    CScript scriptPubKeyOrig;
    if (params.size () == 5)
    {
        const std::string strAddress = params[4].get_str ();
        uint160 hash160;
        bool isValid = AddressToHash160 (strAddress, hash160);
        if (!isValid)
            throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY,
                                "Invalid namecoin address");
        scriptPubKeyOrig.SetBitcoinAddress (strAddress);
    }
    else
    {
        vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool ();
        scriptPubKeyOrig.SetBitcoinAddress (vchPubKey);
    }

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        // Make sure there is a previous NAME_NEW tx on this name
        // and that the random value matches
        uint256 wtxInHash;
        if (params.size() == 3)
        {
            if (!mapMyNames.count(vchName))
            {
                throw runtime_error("could not find a coin with this name, try specifying the name_new transaction id");
            }
            wtxInHash = mapMyNames[vchName];
        }
        else
        {
            wtxInHash.SetHex(params[2].get_str());
        }

        if (!pwalletMain->mapWallet.count(wtxInHash))
        {
            throw runtime_error("previous transaction is not in the wallet");
        }

        CScript scriptPubKey;
        scriptPubKey << OP_NAME_FIRSTUPDATE << vchName << vchRand << vchValue << OP_2DROP << OP_2DROP;
        scriptPubKey += scriptPubKeyOrig;

        CWalletTx& wtxIn = pwalletMain->mapWallet[wtxInHash];
        vector<unsigned char> vchHash;
        bool found = false;
        BOOST_FOREACH(CTxOut& out, wtxIn.vout)
        {
            vector<vector<unsigned char> > vvch;
            int op;
            if (DecodeNameScript(out.scriptPubKey, op, vvch)) {
                if (op != OP_NAME_NEW)
                    throw runtime_error("previous transaction wasn't a name_new");
                vchHash = vvch[0];
                found = true;
            }
        }

        if (!found)
        {
            throw runtime_error("previous tx on this name is not a name tx");
        }

        vector<unsigned char> vchToHash(vchRand);
        vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
        uint160 hash =  Hash160(vchToHash);
        if (uint160(vchHash) != hash)
        {
            throw runtime_error("previous tx used a different random value");
        }

        int64 nNetFee = GetNetworkFee(pindexBest->nHeight);
        // Round up to CENT
        nNetFee += CENT - 1;
        nNetFee = (nNetFee / CENT) * CENT;
        string strError = SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, nNetFee, wtxIn, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_update(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
                "name_update <name> <value> [<toaddress>]\nUpdate and possibly transfer a name"
                + HelpRequiringPassphrase());

    const vchType vchName = vchFromValue (params[0]);
    const vchType vchValue = vchFromValue (params[1]);

    if (vchValue.size () > UI_MAX_VALUE_LENGTH)
      throw JSONRPCError(RPC_INVALID_PARAMETER, "the value is too long");

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;
    CScript scriptPubKeyOrig;

    if (params.size() == 3)
    {
        string strAddress = params[2].get_str();
        uint160 hash160;
        bool isValid = AddressToHash160(strAddress, hash160);
        if (!isValid)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid namecoin address");
        scriptPubKeyOrig.SetBitcoinAddress(strAddress);
    }
    else
    {
        vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool();
        scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    }

    CScript scriptPubKey;
    scriptPubKey << OP_NAME_UPDATE << vchName << vchValue << OP_2DROP << OP_DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_update() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            throw runtime_error("there are pending operations on that name");
        }

        EnsureWalletIsUnlocked();

        CNameDB dbName("r");
        CTransaction tx;
        if (!GetTxOfName(dbName, vchName, tx))
        {
            throw runtime_error("could not find a coin with this name");
        }

        uint256 wtxInHash = tx.GetHash();

        if (!pwalletMain->mapWallet.count(wtxInHash))
        {
            error("name_update() : this coin is not in your wallet %s",
                    wtxInHash.GetHex().c_str());
            throw runtime_error("this coin is not in your wallet");
        }

        CWalletTx& wtxIn = pwalletMain->mapWallet[wtxInHash];
        string strError = SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, 0, wtxIn, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_new(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "name_new <name>"
                + HelpRequiringPassphrase());

    vector<unsigned char> vchName = vchFromValue(params[0]);

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    uint64 rand = GetRand((uint64)-1);
    vector<unsigned char> vchRand = CBigNum(rand).getvch();
    vector<unsigned char> vchToHash(vchRand);
    vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
    uint160 hash =  Hash160(vchToHash);

    vector<unsigned char> vchPubKey = pwalletMain->GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    CScript scriptPubKey;
    scriptPubKey << OP_NAME_NEW << hash << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    {
        EnsureWalletIsUnlocked();

        string strError = pwalletMain->SendMoney(scriptPubKey, MIN_AMOUNT, wtx, false);
        if (strError != "")
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        mapMyNames[vchName] = wtx.GetHash();
    }

    printf("name_new : name=%s, rand=%s, tx=%s\n", stringFromVch(vchName).c_str(), HexStr(vchRand).c_str(), wtx.GetHash().GetHex().c_str());

    vector<Value> res;
    res.push_back(wtx.GetHash().GetHex());
    res.push_back(HexStr(vchRand));
    return res;
}

static Value
name_pending (const Array& params, bool fHelp)
{
  if (fHelp || params.size () != 0)
    throw runtime_error(
      "name_pending\n"
      "List all pending name operations known of.\n");

  Array res;

  CRITICAL_BLOCK (cs_main)
    {
      std::map<vchType, std::set<uint256> >::const_iterator i;
      for (i = mapNamePending.begin (); i != mapNamePending.end (); ++i)
        {
          if (i->second.empty ())
            continue;

          const std::string name = stringFromVch (i->first);

          for (std::set<uint256>::const_iterator j = i->second.begin ();
               j != i->second.end (); ++j)
            {
              CTransaction tx;
              uint256 hashBlock;
              if (!GetTransaction (*j, tx, hashBlock))
                {
                  printf ("name_pending: failed to GetTransaction of hash %s\n",
                          j->GetHex ().c_str ());
                  continue;
                }

              int op, nOut;
              std::vector<vchType> vvch;
              if (!DecodeNameTx (tx, op, nOut, vvch, -1))
                {
                  printf ("name_pending: failed to find name output in tx %s\n",
                          j->GetHex ().c_str ());
                  continue;
                }

              /* Decode the name operation.  */
              std::string value;
              std::string opString;
              switch (op)
                {
                case OP_NAME_FIRSTUPDATE:
                  assert (vvch.size () == 3);
                  opString = "name_firstupdate";
                  value = stringFromVch (vvch[2]);
                  break;

                case OP_NAME_UPDATE:
                  assert (vvch.size () == 2);
                  opString = "name_update";
                  value = stringFromVch (vvch[1]);
                  break;

                default:
                  printf ("name_pending: unexpected op code %d for tx %s\n",
                          op, j->GetHex ().c_str ());
                  continue;
                }

              /* See if it is owned by the wallet user.  */
              const CTxOut& txout = tx.vout[nOut];
              const bool isMine = IsMyName (tx, txout);

              /* Construct the JSON output.  */
              Object obj;
              obj.push_back (Pair ("name", name));
              obj.push_back (Pair ("txid", j->GetHex ()));
              obj.push_back (Pair ("op", opString));
              obj.push_back (Pair ("value", value));
              obj.push_back (Pair ("ismine", isMine));
              res.push_back (obj);
            }
        }
    }

  return res;
}

/* Implement name operations for createrawtransaction.  */
void
AddRawTxNameOperation (CTransaction& tx, const Object& obj)
{
  Value val = find_value (obj, "op");
  if (val.type () != str_type)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "Missing op key.");
  const std::string op = val.get_str ();

  if (op != "name_update")
    throw std::runtime_error ("Only name_update is implemented"
                              " for raw transactions at the moment.");

  val = find_value (obj, "name");
  if (val.type () != str_type)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "Missing name key.");
  const std::string name = val.get_str ();
  const std::vector<unsigned char> vchName = vchFromString (name);

  val = find_value (obj, "value");
  if (val.type () != str_type)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "Missing value key.");
  const std::string value = val.get_str ();

  val = find_value (obj, "address");
  if (val.type () != str_type)
    throw JSONRPCError (RPC_INVALID_PARAMETER, "Missing address key.");
  const std::string address = val.get_str ();
  if (!IsValidBitcoinAddress (address))
    {
      std::ostringstream msg;
      msg << "Invalid Namecoin address: " << address;
      throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, msg.str ());
    }

  tx.nVersion = NAMECOIN_TX_VERSION;

  /* Find the transaction input to add.  */

  CRITICAL_BLOCK(cs_main)
  {
    CNameDB dbName("r");
    CTransaction prevTx;
    if (!GetTxOfName (dbName, vchName, prevTx))
      throw std::runtime_error ("could not find a coin with this name");
    const uint256 prevTxHash = prevTx.GetHash();
    const int nTxOut = IndexOfNameOutput (prevTx);

    CTxIn in(COutPoint(prevTxHash, nTxOut));
    tx.vin.push_back (in);
  }

  /* Construct the transaction output.  */

  CScript scriptPubKeyOrig;
  scriptPubKeyOrig.SetBitcoinAddress (address);

  CScript scriptPubKey;
  scriptPubKey << OP_NAME_UPDATE << vchName << vchFromString (value)
               << OP_2DROP << OP_DROP;
  scriptPubKey += scriptPubKeyOrig;

  CTxOut out(MIN_AMOUNT, scriptPubKey);
  tx.vout.push_back (out);
}

void UnspendInputs(CWalletTx& wtx)
{
    set<CWalletTx*> setCoins;
    BOOST_FOREACH(const CTxIn& txin, wtx.vin)
    {
        if (!pwalletMain->IsMine(txin))
        {
            printf("UnspendInputs(): !mine %s", txin.ToString().c_str());
            continue;
        }
        CWalletTx& prev = pwalletMain->mapWallet[txin.prevout.hash];
        int nOut = txin.prevout.n;

        printf("UnspendInputs(): %s:%d spent %d\n", prev.GetHash().ToString().c_str(), nOut, prev.IsSpent(nOut));

        if (nOut >= prev.vout.size())
            throw runtime_error("CWalletTx::MarkSpent() : nOut out of range");
        prev.vfSpent.resize(prev.vout.size());
        if (prev.vfSpent[nOut])
        {
            prev.vfSpent[nOut] = false;
            prev.fAvailableCreditCached = false;
            prev.WriteToDisk();
        }
#ifdef GUI
        //pwalletMain->vWalletUpdated.push_back(prev.GetHash());
        pwalletMain->NotifyTransactionChanged(pwalletMain, prev.GetHash(), CT_DELETED);

#endif
    }
}

Value deletetransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "deletetransaction <txid>\nNormally used when a transaction cannot be confirmed due to a double spend.\nRestart the program after executing this call.\n"
                );

    if (params.size() != 1)
      throw runtime_error("missing txid");
    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
      uint256 hash;
      hash.SetHex(params[0].get_str());
      if (!pwalletMain->mapWallet.count(hash))
        throw runtime_error("transaction not in wallet");

      if (!mapTransactions.count(hash))
      {
        //throw runtime_error("transaction not in memory - is already in blockchain?");
        CTransaction tx;
        uint256 hashBlock = 0;
        if (GetTransaction(hash, tx, hashBlock /*, true*/) && hashBlock != 0)
          throw runtime_error("transaction is already in blockchain");
      }
      CWalletTx wtx = pwalletMain->mapWallet[hash];
      UnspendInputs(wtx);

      // We are not removing from mapTransactions because this can cause memory corruption
      // during mining.  The user should restart to clear the tx from memory.
      wtx.RemoveFromMemoryPool();
      pwalletMain->EraseFromWallet(wtx.GetHash());
      vector<unsigned char> vchName;
      if (GetNameOfTx(wtx, vchName) && mapNamePending.count(vchName)) {
        printf("deletetransaction() : remove from pending");
        mapNamePending[vchName].erase(wtx.GetHash());
      }
      return "success, please restart program to clear memory";
    }
}

void rescanfornames()
{
    printf("Scanning blockchain for names to create fast index...\n");

    /* The database should already be created although empty.  */

    CNameDB dbName("r+");
    dbName.ReconstructNameIndex();
}

Value name_clean(const Array& params, bool fHelp)
{
    if (fHelp || params.size())
        throw runtime_error("name_clean\nClean unsatisfiable transactions from the wallet - including name_update on an already taken name\n");

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        map<uint256, CWalletTx> mapRemove;

        printf("-----------------------------\n");

        {
            DatabaseSet dbset("r");
            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletMain->mapWallet)
            {
                CWalletTx& wtx = item.second;
                vchType vchName;
                if (wtx.GetDepthInMainChain () < 1
                    && IsConflictedTx (dbset, wtx, vchName))
                {
                    uint256 hash = wtx.GetHash();
                    mapRemove[hash] = wtx;
                }
            }
        }

        bool fRepeat = true;
        while (fRepeat)
        {
            fRepeat = false;
            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, pwalletMain->mapWallet)
            {
                CWalletTx& wtx = item.second;
                BOOST_FOREACH(const CTxIn& txin, wtx.vin)
                {
                    uint256 hash = wtx.GetHash();

                    // If this tx depends on a tx to be removed, remove it too
                    if (mapRemove.count(txin.prevout.hash) && !mapRemove.count(hash))
                    {
                        mapRemove[hash] = wtx;
                        fRepeat = true;
                    }
                }
            }
        }

        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapRemove)
        {
            CWalletTx& wtx = item.second;

            UnspendInputs(wtx);
            wtx.RemoveFromMemoryPool();
            pwalletMain->EraseFromWallet(wtx.GetHash());
            vector<unsigned char> vchName;
            if (GetNameOfTx(wtx, vchName) && mapNamePending.count(vchName))
            {
                string name = stringFromVch(vchName);
                printf("name_clean() : erase %s from pending of name %s",
                        wtx.GetHash().GetHex().c_str(), name.c_str());
                if (!mapNamePending[vchName].erase(wtx.GetHash()))
                    error("name_clean() : erase but it was not pending");
            }
            wtx.print();
        }

        printf("-----------------------------\n");
    }

    return true;
}

bool CNameDB::test()
{
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    loop
    {
        // Read next record
        CDataStream ssKey;
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue);
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "namei")
        {
            vector<unsigned char> vchName;
            ssKey >> vchName;
            string strName = stringFromVch(vchName);
            vector<CDiskTxPos> vtxPos;
            ssValue >> vtxPos;
            if (NAME_DEBUG)
              printf("NAME %s : ", strName.c_str());
            BOOST_FOREACH(CDiskTxPos& txPos, vtxPos) {
                txPos.print();
                if (NAME_DEBUG)
                  printf(" @ %d, ", GetTxPosHeight(txPos));
            }
            if (NAME_DEBUG)
              printf("\n");
        }
    }
    pcursor->close();
}

bool CNameDB::ScanNames(
        const vector<unsigned char>& vchName,
        int nMax,
        vector<pair<vector<unsigned char>, CNameIndex> >& nameScan)
        //vector<pair<vector<unsigned char>, CDiskTxPos> >& nameScan)
{
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("namei"), vchName);
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "namei")
        {
            vector<unsigned char> vchName;
            ssKey >> vchName;
            string strName = stringFromVch(vchName);
            //vector<CDiskTxPos> vtxPos;
            vector<CNameIndex> vtxPos;
            ssValue >> vtxPos;
            //CDiskTxPos txPos;
            CNameIndex txPos;
            if (!vtxPos.empty())
            {
                txPos = vtxPos.back();
            }
            nameScan.push_back(make_pair(vchName, txPos));
        }

        if (nameScan.size() >= nMax)
            break;
    }
    pcursor->close();
    return true;
}

// true - accept, false - reject
bool NameBugWorkaround(const CTransaction& tx, CTxDB &txdb, CDiskTxPos *pPrevTxPos = NULL)
{
    // Find previous name tx
    bool found = false;
    int prevOp;
    vector<vector<unsigned char> > vvchPrevArgs;

    for (int i = 0; i < tx.vin.size(); i++)
    {
        COutPoint prevout = tx.vin[i].prevout;
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(prevout.hash, txindex))
        {
            printf("NameBugWorkaround WARNING: cannot read tx index of previous tx (hash: %s)\n", prevout.hash.ToString().c_str());
            continue;
        }
        CTransaction txPrev;
        if (!txPrev.ReadFromDisk(txindex.pos))
        {
            printf("NameBugWorkaround WARNING: cannot read previous tx from disk (hash: %s)\n", prevout.hash.ToString().c_str());
            continue;
        }

        CTxOut& out = txPrev.vout[tx.vin[i].prevout.n];
        if (DecodeNameScript(out.scriptPubKey, prevOp, vvchPrevArgs))
        {
            if (found)
                return error("NameBugWorkaround WARNING: multiple previous name transactions");
            found = true;
            if (pPrevTxPos)
                *pPrevTxPos = txindex.pos;
        }
    }

    if (!found)
        return error("NameBugWorkaround WARNING: prev tx not found");

    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    // NameBugWorkaround is always called for the buggy interval only (before block BUG_WORKAROUND_BLOCK),
    // so we provide height as zero
    if (!DecodeNameTx(tx, op, nOut, vvchArgs, 0))
        return error("NameBugWorkaround WARNING: cannot decode name tx\n");

    if (op == OP_NAME_FIRSTUPDATE)
    {
        // Check hash
        const vector<unsigned char> &vchHash = vvchPrevArgs[0];
        const vector<unsigned char> &vchName = vvchArgs[0];
        const vector<unsigned char> &vchRand = vvchArgs[1];
        vector<unsigned char> vchToHash(vchRand);
        vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
        uint160 hash = Hash160(vchToHash);
        if (uint160(vchHash) != hash)
            return false;
    }
    else if (op == OP_NAME_UPDATE)
    {
        // Check name
        if (vvchPrevArgs[0] != vvchArgs[0])
            return false;
    }

    return true;
}

// Check that the last entry in name history matches the given tx pos
bool CheckNameTxPos(const vector<CNameIndex> &vtxPos, const CDiskTxPos& txPos)
{
    if (vtxPos.empty())
        return false;

    return vtxPos.back().txPos == txPos;
}

bool CNameDB::ReconstructNameIndex()
{
    CTxDB txdb("r");
    CTxIndex txindex;
    CBlockIndex* pindex = pindexGenesisBlock;
    CRITICAL_BLOCK(pwalletMain->cs_mapWallet)
    {
        while (pindex)
        {
            TxnBegin();
            CBlock block;
            block.ReadFromDisk(pindex);
            int nHeight = pindex->nHeight;

            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if (tx.nVersion != NAMECOIN_TX_VERSION)
                    continue;

                vector<vector<unsigned char> > vvchArgs;
                int op;
                int nOut;

                if (!DecodeNameTx(tx, op, nOut, vvchArgs, nHeight))
                    continue;

                if (op == OP_NAME_NEW)
                    continue;
                const vector<unsigned char> &vchName = vvchArgs[0];
                const vector<unsigned char> &vchValue = vvchArgs[op == OP_NAME_FIRSTUPDATE ? 2 : 1];

                if(!txdb.ReadDiskTx(tx.GetHash(), tx, txindex))
                    continue;

                // Bug workaround
                CDiskTxPos prevTxPos;
                if (nHeight >= BUG_WORKAROUND_BLOCK_START && nHeight < BUG_WORKAROUND_BLOCK)
                    if (!NameBugWorkaround(tx, txdb, &prevTxPos))
                    {
                        printf("NameBugWorkaround rejected tx %s at height %d (name %s)\n", tx.GetHash().ToString().c_str(), nHeight, stringFromVch(vchName).c_str());
                        continue;
                    }

                vector<CNameIndex> vtxPos;
                if (ExistsName(vchName))
                {
                    if (!ReadName(vchName, vtxPos))
                        return error("Rescanfornames() : failed to read from name DB");
                }

                if (op == OP_NAME_UPDATE && nHeight >= BUG_WORKAROUND_BLOCK_START && nHeight < BUG_WORKAROUND_BLOCK && !CheckNameTxPos(vtxPos, prevTxPos))
                {
                    printf("NameBugWorkaround rejected tx %s at height %d (name %s), because previous tx was also rejected\n", tx.GetHash().ToString().c_str(), nHeight, stringFromVch(vchName).c_str());
                    continue;
                }

                CNameIndex txPos2;
                txPos2.nHeight = nHeight;
                txPos2.vValue = vchValue;
                txPos2.txPos = txindex.pos;
                vtxPos.push_back(txPos2);
                if (!WriteName(vchName, vtxPos))
                {
                    return error("Rescanfornames() : failed to write to name DB");
                }

                //if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                //    ret++;
            }
            pindex = pindex->pnext;
            TxnCommit();
        }
    }
}

CHooks* InitHook()
{
    mapCallTable.insert(make_pair("name_new", &name_new));
    mapCallTable.insert(make_pair("name_update", &name_update));
    mapCallTable.insert(make_pair("name_firstupdate", &name_firstupdate));
    mapCallTable.insert(make_pair("name_list", &name_list));
    mapCallTable.insert(make_pair("name_scan", &name_scan));
    mapCallTable.insert(make_pair("name_filter", &name_filter));
    mapCallTable.insert(make_pair("name_show", &name_show));
    mapCallTable.insert(make_pair("name_history", &name_history));
    mapCallTable.insert(make_pair("name_debug", &name_debug));
    mapCallTable.insert(make_pair("name_debug1", &name_debug1));
    mapCallTable.insert(make_pair("name_clean", &name_clean));
    mapCallTable.insert(make_pair("name_pending", &name_pending));
    mapCallTable.insert(make_pair("sendtoname", &sendtoname));
    mapCallTable.insert(make_pair("deletetransaction", &deletetransaction));
    hashGenesisBlock = hashNameCoinGenesisBlock;
    printf("Setup namecoin genesis block %s\n", hashGenesisBlock.GetHex().c_str());
    return new CNamecoinHooks();
}

bool CNamecoinHooks::IsStandard(const CScript& scriptPubKey)
{
    return true;
}

bool DecodeNameScript(const CScript& script, int& op, vector<vector<unsigned char> > &vvch)
{
    CScript::const_iterator pc = script.begin();
    return DecodeNameScript(script, op, vvch, pc);
}

bool DecodeNameScript(const CScript& script, int& op, vector<vector<unsigned char> > &vvch, CScript::const_iterator& pc)
{
    opcodetype opcode;
    if (!script.GetOp(pc, opcode))
        return false;
    if (opcode < OP_1 || opcode > OP_16)
        return false;

    op = opcode - OP_1 + 1;

    for (;;) {
        vector<unsigned char> vch;
        if (!script.GetOp(pc, opcode, vch))
            return false;
        if (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
            break;
        if (!(opcode >= 0 && opcode <= OP_PUSHDATA4))
            return false;
        vvch.push_back(vch);
    }

    // move the pc to after any DROP or NOP
    while (opcode == OP_DROP || opcode == OP_2DROP || opcode == OP_NOP)
    {
        if (!script.GetOp(pc, opcode))
            break;
    }

    pc--;

    if ((op == OP_NAME_NEW && vvch.size() == 1) ||
            (op == OP_NAME_FIRSTUPDATE && vvch.size() == 3) ||
            (op == OP_NAME_UPDATE && vvch.size() == 2))
        return true;
    return error("invalid number of arguments for name op");
}

bool DecodeNameTx(const CTransaction& tx, int& op, int& nOut, vector<vector<unsigned char> >& vvch, int nHeight)
{
    bool found = false;

    if (nHeight < 0)
    {
        nHeight = pindexBest->nHeight;
        /*
        CTxDB txdb("r");
        CTxIndex txindex;
        if (txdb.ReadTxIndex(tx.GetHash(), txindex))
        {
            nHeight = GetTxPosHeight(txindex.pos);
            if (nHeight == 0)
                nHeight = BUG_WORKAROUND_BLOCK;
        }
        else
            nHeight = BUG_WORKAROUND_BLOCK;
        */
    }

    // Bug workaround
    if (nHeight >= BUG_WORKAROUND_BLOCK)
    {
        // Strict check - bug disallowed
        for (int i = 0; i < tx.vout.size(); i++)
        {
            const CTxOut& out = tx.vout[i];

            vector<vector<unsigned char> > vvchRead;

            if (DecodeNameScript(out.scriptPubKey, op, vvchRead))
            {
                // If more than one name op, fail
                if (found)
                {
                    vvch.clear();
                    return false;
                }
                nOut = i;
                found = true;
                vvch = vvchRead;
            }
        }

        if (!found)
            vvch.clear();
    }
    else
    {
        // Name bug: before the hard-fork point, we reproduce the buggy behavior
        // of concatenating args (vvchPrevArgs not cleared between calls)
        bool fBug = false;
        for (int i = 0; i < tx.vout.size(); i++)
        {
            const CTxOut& out = tx.vout[i];

            int nOldSize = vvch.size();
            if (DecodeNameScript(out.scriptPubKey, op, vvch))
            {
                // If more than one name op, fail
                if (found)
                    return false;
                nOut = i;
                found = true;
            }
            if (nOldSize != 0 && vvch.size() != nOldSize)
                fBug = true;
        }
        if (fBug)
            printf("Name bug warning: argument concatenation happened in tx %s (block height %d)\n", tx.GetHash().GetHex().c_str(), nHeight);
    }

    return found;
}

int64 GetNameNetFee(const CTransaction& tx)
{
    int64 nFee = 0;

    for (int i = 0 ; i < tx.vout.size() ; i++)
    {
        const CTxOut& out = tx.vout[i];
        if (out.scriptPubKey.size() == 1 && out.scriptPubKey[0] == OP_RETURN)
        {
            nFee += out.nValue;
        }
    }

    return nFee;
}

bool GetValueOfNameTx(const CTransaction& tx, vector<unsigned char>& value)
{
    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    if (!DecodeNameTx(tx, op, nOut, vvch, -1))
        return false;

    switch (op)
    {
        case OP_NAME_NEW:
            return false;
        case OP_NAME_FIRSTUPDATE:
            value = vvch[2];
            return true;
        case OP_NAME_UPDATE:
            value = vvch[1];
            return true;
        default:
            return false;
    }
}

int IndexOfNameOutput(const CTransaction& tx)
{
    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvch, -1);

    if (!good)
        throw runtime_error("IndexOfNameOutput() : name output not found");
    return nOut;
}

void CNamecoinHooks::AddToWallet(CWalletTx& wtx)
{
}

bool CNamecoinHooks::IsMine(const CTransaction& tx)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;

    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    // We do the check under the correct rule set (post-hardfork)
    bool good = DecodeNameTx(tx, op, nOut, vvch, BUG_WORKAROUND_BLOCK);

    if (!good)
    {
        error("IsMine() hook : no output out script in name tx %s\n", tx.ToString().c_str());
        return false;
    }

    const CTxOut& txout = tx.vout[nOut];
    if (IsMyName(tx, txout))
    {
        //printf("IsMine() hook : found my transaction %s nout %d\n", tx.GetHash().GetHex().c_str(), nOut);
        return true;
    }
    return false;
}

bool CNamecoinHooks::IsMine(const CTransaction& tx, const CTxOut& txout, bool ignore_name_new /* = false*/)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;

    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    if (!DecodeNameScript(txout.scriptPubKey, op, vvch))
        return false;

    if (ignore_name_new && op == OP_NAME_NEW)
        return false;

    if (IsMyName(tx, txout))
    {
        //printf("IsMine() hook : found my transaction %s value %ld\n", tx.GetHash().GetHex().c_str(), txout.nValue);
        return true;
    }
    return false;
}

void
CNamecoinHooks::AcceptToMemoryPool (DatabaseSet& dbset, const CTransaction& tx)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return;

    if (tx.vout.size() < 1)
    {
        error("AcceptToMemoryPool() : no output in name tx %s\n", tx.ToString().c_str());
        return;
    }

    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvch, BUG_WORKAROUND_BLOCK);

    if (!good)
    {
        error("AcceptToMemoryPool() : no output out script in name tx %s", tx.ToString().c_str());
        return;
    }

    if (op != OP_NAME_NEW)
    {
        CRITICAL_BLOCK(cs_main)
            mapNamePending[vvch[0]].insert(tx.GetHash());
    }
}

void CNamecoinHooks::RemoveFromMemoryPool(const CTransaction& tx)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return;

    if (tx.vout.size() < 1)
        return;

    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    if (!DecodeNameTx(tx, op, nOut, vvch, BUG_WORKAROUND_BLOCK))
        return;

    if (op != OP_NAME_NEW)
    {
        CRITICAL_BLOCK(cs_main)
        {
            std::map<std::vector<unsigned char>, std::set<uint256> >::iterator mi = mapNamePending.find(vvch[0]);
            if (mi != mapNamePending.end())
                mi->second.erase(tx.GetHash());
        }
    }
}

int CheckTransactionAtRelativeDepth(CBlockIndex* pindexBlock, CTxIndex& txindex, int maxDepth)
{
    for (CBlockIndex* pindex = pindexBlock; pindex && pindexBlock->nHeight - pindex->nHeight < maxDepth; pindex = pindex->pprev)
        if (pindex->nBlockPos == txindex.pos.nBlockPos && pindex->nFile == txindex.pos.nFile)
            return pindexBlock->nHeight - pindex->nHeight;
    return -1;
}

bool GetNameOfTx(const CTransaction& tx, vector<unsigned char>& name)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;
    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs, -1);
    if (!good)
        return error("GetNameOfTx() : could not decode a namecoin tx");

    switch (op)
    {
        case OP_NAME_FIRSTUPDATE:
        case OP_NAME_UPDATE:
            name = vvchArgs[0];
            return true;
    }
    return false;
}

bool
IsConflictedTx (DatabaseSet& dbset, const CTransaction& tx, vchType& name)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;
    vector<vchType> vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs, pindexBest->nHeight);
    if (!good)
        return error("IsConflictedTx() : could not decode a namecoin tx");
    int nPrevHeight;
    int nDepth;
    int64 nNetFee;

    switch (op)
    {
        case OP_NAME_FIRSTUPDATE:
            nPrevHeight = GetNameHeight (dbset, vvchArgs[0]);
            name = vvchArgs[0];
            if (nPrevHeight >= 0 && pindexBest->nHeight - nPrevHeight < GetExpirationDepth(pindexBest->nHeight))
                return true;
    }
    return false;
}

bool
CNamecoinHooks::ConnectInputs (DatabaseSet& dbset,
                               map<uint256, CTxIndex>& mapTestPool,
                               const CTransaction& tx,
                               vector<CTransaction>& vTxPrev,
                               vector<CTxIndex>& vTxindex,
                               CBlockIndex* pindexBlock, CDiskTxPos& txPos,
                               bool fBlock, bool fMiner)
{
    int nInput;
    bool found = false;

    int prevOp;
    vector<vector<unsigned char> > vvchPrevArgs;

    // Bug workaround
    if (fMiner || !fBlock || pindexBlock->nHeight >= BUG_WORKAROUND_BLOCK)
    {
        // Strict check - bug disallowed
        for (int i = 0; i < tx.vin.size(); i++)
        {
            CTxOut& out = vTxPrev[i].vout[tx.vin[i].prevout.n];

            vector<vector<unsigned char> > vvchPrevArgsRead;

            if (DecodeNameScript(out.scriptPubKey, prevOp, vvchPrevArgsRead))
            {
                if (found)
                    return error("ConnectInputHook() : multiple previous name transactions");
                found = true;
                nInput = i;

                vvchPrevArgs = vvchPrevArgsRead;
            }
        }
    }
    else
    {
        // Name bug: before hard-fork point, we reproduce the buggy behavior
        // of concatenating args (vvchPrevArgs not cleared between calls)
        bool fBug = false;
        for (int i = 0; i < tx.vin.size(); i++)
        {
            CTxOut& out = vTxPrev[i].vout[tx.vin[i].prevout.n];

            int nOldSize = vvchPrevArgs.size();
            if (DecodeNameScript(out.scriptPubKey, prevOp, vvchPrevArgs))
            {
                if (found)
                    return error("ConnectInputHook() : multiple previous name transactions");
                found = true;
                nInput = i;
            }
            if (nOldSize != 0 && vvchPrevArgs.size() != nOldSize)
                fBug = true;
        }
        if (fBug)
            printf("Name bug warning: argument concatenation happened in tx %s (block height %d)\n", tx.GetHash().GetHex().c_str(), pindexBlock->nHeight);
    }

    if (tx.nVersion != NAMECOIN_TX_VERSION)
    {
        // Make sure name-op outputs are not spent by a regular transaction, or the name
        // would be lost
        if (found)
            return error("ConnectInputHook() : a non-namecoin transaction with a namecoin input");
        return true;
    }

    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs, pindexBlock->nHeight);
    if (!good)
        return error("ConnectInputsHook() : could not decode a namecoin tx");

    int nPrevHeight;
    int nDepth;
    int64 nNetFee;

    bool fBugWorkaround = false;

    // HACK: The following two checks are redundant after hard-fork at block 150000, because it is performed
    // in CheckTransaction. However, before that, we do not know height during CheckTransaction
    // and cannot apply the right set of rules
    if (vvchArgs[0].size() > MAX_NAME_LENGTH)
        return error("name transaction with name too long");

    switch (op)
    {
        case OP_NAME_NEW:
            if (found)
                return error("ConnectInputsHook() : name_new tx pointing to previous namecoin tx");

            // HACK: The following check is redundant after hard-fork at block 150000, because it is performed
            // in CheckTransaction. However, before that, we do not know height during CheckTransaction
            // and cannot apply the right set of rules
            if (vvchArgs[0].size() != 20)
                return error("name_new tx with incorrect hash length");

            break;
        case OP_NAME_FIRSTUPDATE:
            nNetFee = GetNameNetFee(tx);
            if (nNetFee < GetNetworkFee(pindexBlock->nHeight))
                return error("ConnectInputsHook() : got tx %s with fee too low %d", tx.GetHash().GetHex().c_str(), nNetFee);
            if (!found || prevOp != OP_NAME_NEW)
                return error("ConnectInputsHook() : name_firstupdate tx without previous name_new tx");

            // HACK: The following two checks are redundant after hard-fork at block 150000, because it is performed
            // in CheckTransaction. However, before that, we do not know height during CheckTransaction
            // and cannot apply the right set of rules
            if (vvchArgs[1].size() > 20)
                return error("name_firstupdate tx with rand too big");
            if (vvchArgs[2].size() > MAX_VALUE_LENGTH)
                return error("name_firstupdate tx with value too long");

            {
                // Check hash
                const vector<unsigned char> &vchHash = vvchPrevArgs[0];
                const vector<unsigned char> &vchName = vvchArgs[0];
                const vector<unsigned char> &vchRand = vvchArgs[1];
                vector<unsigned char> vchToHash(vchRand);
                vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
                uint160 hash = Hash160(vchToHash);
                if (uint160(vchHash) != hash)
                {
                    if (pindexBlock->nHeight >= BUG_WORKAROUND_BLOCK)
                        return error("ConnectInputsHook() : name_firstupdate hash mismatch");
                    else
                    {
                        // Accept bad transactions before the hard-fork point, but do not write them to name DB
                        printf("ConnectInputsHook() : name_firstupdate mismatch bug workaround\n");
                        fBugWorkaround = true;
                    }
                }
            }

            nPrevHeight = GetNameHeight (dbset, vvchArgs[0]);
            if (nPrevHeight >= 0 && pindexBlock->nHeight - nPrevHeight < GetExpirationDepth(pindexBlock->nHeight))
                return error("ConnectInputsHook() : name_firstupdate on an unexpired name");
            nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], MIN_FIRSTUPDATE_DEPTH);
            // Do not accept if in chain and not mature
            if ((fBlock || fMiner) && nDepth >= 0 && nDepth < MIN_FIRSTUPDATE_DEPTH)
                return false;

            // Do not mine if previous name_new is not visible.  This is if
            // name_new expired or not yet in a block
            if (fMiner)
            {
                // TODO CPU intensive
                nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], GetExpirationDepth(pindexBlock->nHeight));
                if (nDepth == -1)
                    return error("ConnectInputsHook() : name_firstupdate cannot be mined if name_new is not already in chain and unexpired");
                // Check that no other pending txs on this name are already in the block to be mined
                set<uint256>& setPending = mapNamePending[vvchArgs[0]];
                BOOST_FOREACH(const PAIRTYPE(uint256, CTxIndex)& s, mapTestPool)
                {
                    if (setPending.count(s.first))
                    {
                        printf("ConnectInputsHook() : will not mine %s because it clashes with %s",
                                tx.GetHash().GetHex().c_str(),
                                s.first.GetHex().c_str());
                        return false;
                    }
                }
            }
            break;
        case OP_NAME_UPDATE:
            if (!found || (prevOp != OP_NAME_FIRSTUPDATE && prevOp != OP_NAME_UPDATE))
                return error("name_update tx without previous update tx");

            // HACK: The following check is redundant after hard-fork at block 150000, because it is performed
            // in CheckTransaction. However, before that, we do not know height during CheckTransaction
            // and cannot apply the right set of rules
            if (vvchArgs[1].size() > MAX_VALUE_LENGTH)
                return error("name_update tx with value too long");

            // Check name
            if (vvchPrevArgs[0] != vvchArgs[0])
            {
                if (pindexBlock->nHeight >= BUG_WORKAROUND_BLOCK)
                    return error("ConnectInputsHook() : name_update name mismatch");
                else
                {
                    // Accept bad transactions before the hard-fork point, but do not write them to name DB
                    printf("ConnectInputsHook() : name_update mismatch bug workaround");
                    fBugWorkaround = true;
                }
            }

            // TODO CPU intensive
            nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], GetExpirationDepth(pindexBlock->nHeight));
            if ((fBlock || fMiner) && nDepth < 0)
                return error("ConnectInputsHook() : name_update on an expired name, or there is a pending transaction on the name");
            break;
        default:
            return error("ConnectInputsHook() : name transaction has unknown op");
    }

    // If fBugWorkaround is in action, do not update name DB (i.e. just silently accept bad tx to avoid early hard-fork)
    // Also disallow mining bad txes

    if (fMiner && fBugWorkaround)
        return error("ConnectInputsHook(): mismatch bug workaround - should not mine this tx");

    if (!fBugWorkaround)
    {
        if (!fBlock && op == OP_NAME_UPDATE)
        {
            vector<CNameIndex> vtxPos;
            if (dbset.name ().ExistsName (vvchArgs[0])
                && !dbset.name ().ReadName (vvchArgs[0], vtxPos))
              return error("ConnectInputsHook() : failed to read from name DB");
            // Valid tx on top of buggy tx: if not in block, reject
            if (!CheckNameTxPos(vtxPos, vTxindex[nInput].pos))
                return error("ConnectInputsHook() : Name bug workaround: tx %s rejected, since previous tx (%s) is not in the name DB\n", tx.GetHash().ToString().c_str(), vTxPrev[nInput].GetHash().ToString().c_str());
        }

        if (fBlock)
        {
            if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
            {
                //vector<CDiskTxPos> vtxPos;
                vector<CNameIndex> vtxPos;
                if (dbset.name ().ExistsName (vvchArgs[0])
                    && !dbset.name ().ReadName (vvchArgs[0], vtxPos))
                  return error("ConnectInputsHook() : failed to read from name DB");

                if (op == OP_NAME_UPDATE && !CheckNameTxPos(vtxPos, vTxindex[nInput].pos))
                {
                    printf("ConnectInputsHook() : Name bug workaround: tx %s rejected, since previous tx (%s) is not in the name DB\n", tx.GetHash().ToString().c_str(), vTxPrev[nInput].GetHash().ToString().c_str());
                    // Valid tx on top of buggy tx: reject only after hard-fork
                    if (pindexBlock->nHeight >= BUG_WORKAROUND_BLOCK)
                        return false;
                    else
                        fBugWorkaround = true;
                }

                if (!fBugWorkaround)
                {
                    vector<unsigned char> vchValue; // add
                    int nHeight;
                    uint256 hash;
                    GetValueOfTxPos(txPos, vchValue, hash, nHeight);
                    CNameIndex txPos2;
                    txPos2.nHeight = pindexBlock->nHeight;
                    txPos2.vValue = vchValue;
                    txPos2.txPos = txPos;
                    vtxPos.push_back(txPos2); // fin add
                    if (!dbset.name ().WriteName (vvchArgs[0], vtxPos))
                      return error("ConnectInputsHook() : failed to write to name DB");
                }
            }

            if (op != OP_NAME_NEW)
                CRITICAL_BLOCK(cs_main)
                {
                    std::map<std::vector<unsigned char>, std::set<uint256> >::iterator mi = mapNamePending.find(vvchArgs[0]);
                    if (mi != mapNamePending.end())
                        mi->second.erase(tx.GetHash());
                }
        }
    }

    return true;
}

bool
CNamecoinHooks::DisconnectInputs (DatabaseSet& dbset, const CTransaction& tx,
                                  CBlockIndex* pindexBlock)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return true;

    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs, pindexBlock->nHeight);
    if (!good)
        return error("DisconnectInputsHook() : could not decode namecoin tx");
    if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
    {
        //vector<CDiskTxPos> vtxPos;
        vector<CNameIndex> vtxPos;
        if (!dbset.name ().ReadName (vvchArgs[0], vtxPos))
            return error("DisconnectInputsHook() : failed to read from name DB");
        // vtxPos might be empty if we pruned expired transactions.  However, it should normally still not
        // be empty, since a reorg cannot go that far back.  Be safe anyway and do not try to pop if empty.
        if (vtxPos.size())
        {
            CTxIndex txindex;
            if (!dbset.tx ().ReadTxIndex (tx.GetHash (), txindex))
                return error("DisconnectInputsHook() : failed to read tx index");

            if (vtxPos.back().txPos == txindex.pos)
                vtxPos.pop_back();

            // TODO validate that the first pos is the current tx pos
        }
        if (!dbset.name ().WriteName (vvchArgs[0], vtxPos))
            return error("DisconnectInputsHook() : failed to write to name DB");
    }

    return true;
}

bool CNamecoinHooks::CheckTransaction(const CTransaction& tx)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return true;

    vector<vector<unsigned char> > vvch;
    int op;
    int nOut;

    // HACK: We do not know height here, so we check under both old and new rule sets (before/after hardfork)
    // The correct check is duplicated in ConnectInputs.
    bool ret[2];
    for (int iter = 0; iter < 2; iter++)
    {
        ret[iter] = true;

        bool good = DecodeNameTx(tx, op, nOut, vvch, iter == 0 ? 0 : BUG_WORKAROUND_BLOCK);

        if (!good)
        {
            ret[iter] = error("name transaction has unknown script format");
            continue;
        }

        if (vvch[0].size() > MAX_NAME_LENGTH)
        {
            ret[iter] = error("name transaction with name too long");
            continue;
        }

        switch (op)
        {
            case OP_NAME_NEW:
                if (vvch[0].size() != 20)
                    ret[iter] = error("name_new tx with incorrect hash length");
                break;
            case OP_NAME_FIRSTUPDATE:
                if (vvch[1].size() > 20)
                    ret[iter] = error("name_firstupdate tx with rand too big");
                if (vvch[2].size() > MAX_VALUE_LENGTH)
                    ret[iter] = error("name_firstupdate tx with value too long");
                break;
            case OP_NAME_UPDATE:
                if (vvch[1].size() > MAX_VALUE_LENGTH)
                    ret[iter] = error("name_update tx with value too long");
                break;
            default:
                ret[iter] = error("name transaction has unknown op");
        }
    }
    return ret[0] || ret[1];
}

static string nameFromOp(int op)
{
    switch (op)
    {
        case OP_NAME_NEW:
            return "name_new";
        case OP_NAME_UPDATE:
            return "name_update";
        case OP_NAME_FIRSTUPDATE:
            return "name_firstupdate";
        default:
            return "<unknown name op>";
    }
}

bool CNamecoinHooks::ExtractAddress(const CScript& script, string& address)
{
    if (script.size() == 1 && script[0] == OP_RETURN)
    {
        address = string("network fee");
        return true;
    }
    vector<vector<unsigned char> > vvch;
    int op;
    if (!DecodeNameScript(script, op, vvch))
        return false;

    string strOp = nameFromOp(op);
    string strName;
    if (op == OP_NAME_NEW)
    {
#ifdef GUI
        LOCK(cs_main);

        std::map<uint160, std::vector<unsigned char> >::const_iterator mi = mapMyNameHashes.find(uint160(vvch[0]));
        if (mi != mapMyNameHashes.end())
            strName = stringFromVch(mi->second);
        else
#endif
            strName = HexStr(vvch[0]);
    }
    else
        strName = stringFromVch(vvch[0]);

    address = strOp + ": " + strName;
    return true;
}

bool
CNamecoinHooks::ConnectBlock (CBlock& block, DatabaseSet& dbset,
                              CBlockIndex* pindex)
{
    return true;
}

bool
CNamecoinHooks::DisconnectBlock (CBlock& block, DatabaseSet& dbset,
                                 CBlockIndex* pindex)
{
    return true;
}

bool GenesisBlock(CBlock& block, int extra)
{
    block = CBlock();
    block.hashPrevBlock = 0;
    block.nVersion = 1;
    block.nTime    = 1303000001;
    block.nBits    = 0x1c007fff;
    block.nNonce   = 0xa21ea192U;
    const char* pszTimestamp = "... choose what comes next.  Lives of your own, or a return to chains. -- V";
    CTransaction txNew;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << block.nBits << CBigNum(++extra) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = 50 * COIN;
    txNew.vout[0].scriptPubKey = CScript() << ParseHex("04b620369050cd899ffbbc4e8ee51e8c4534a855bb463439d63d235d4779685d8b6f4870a238cf365ac94fa13ef9a2a22cd99d0d5ee86dcabcafce36c7acf43ce5") << OP_CHECKSIG;
    block.vtx.push_back(txNew);
    block.hashMerkleRoot = block.BuildMerkleTree();
    printf("====================================\n");
    printf("Merkle: %s\n", block.hashMerkleRoot.GetHex().c_str());
    printf("Block: %s\n", block.GetHash().GetHex().c_str());
    block.print();
    assert(block.GetHash() == hashGenesisBlock);
    return true;
}

bool CNamecoinHooks::GenesisBlock(CBlock& block)
{
    if (fTestNet)
        return false;

    return ::GenesisBlock(block, NAME_COIN_GENESIS_EXTRA);
}

int CNamecoinHooks::LockinHeight()
{
    if (fTestNet)
        return 0;

    return 193000;
}

bool CNamecoinHooks::Lockin(int nHeight, uint256 hash)
{
    if (!fTestNet)
        if ((nHeight == 2016 && hash != uint256("0x0000000000660bad0d9fbde55ba7ee14ddf766ed5f527e3fbca523ac11460b92")) ||
                (nHeight ==   4032 && hash != uint256("0x0000000000493b5696ad482deb79da835fe2385304b841beef1938655ddbc411")) ||
                (nHeight ==   6048 && hash != uint256("0x000000000027939a2e1d8bb63f36c47da858e56d570f143e67e85068943470c9")) ||
                (nHeight ==   8064 && hash != uint256("0x000000000003a01f708da7396e54d081701ea406ed163e519589717d8b7c95a5")) ||
                (nHeight ==  10080 && hash != uint256("0x00000000000fed3899f818b2228b4f01b9a0a7eeee907abd172852df71c64b06")) ||
                (nHeight ==  12096 && hash != uint256("0x0000000000006c06988ff361f124314f9f4bb45b6997d90a7ee4cedf434c670f")) ||
                (nHeight ==  14112 && hash != uint256("0x00000000000045d95e0588c47c17d593c7b5cb4fb1e56213d1b3843c1773df2b")) ||
                (nHeight ==  16128 && hash != uint256("0x000000000001d9964f9483f9096cf9d6c6c2886ed1e5dec95ad2aeec3ce72fa9")) ||
                (nHeight ==  18940 && hash != uint256("0x00000000000087f7fc0c8085217503ba86f796fa4984f7e5a08b6c4c12906c05")) ||
                (nHeight ==  30240 && hash != uint256("0xe1c8c862ff342358384d4c22fa6ea5f669f3e1cdcf34111f8017371c3c0be1da")) ||
                (nHeight ==  57000 && hash != uint256("0xaa3ec60168a0200799e362e2b572ee01f3c3852030d07d036e0aa884ec61f203")) ||
                (nHeight == 112896 && hash != uint256("0x73f880e78a04dd6a31efc8abf7ca5db4e262c4ae130d559730d6ccb8808095bf")) ||
                (nHeight == 182000 && hash != uint256("0xd47b4a8fd282f635d66ce34ebbeb26ffd64c35b41f286646598abfd813cba6d9")) ||
                (nHeight == 193000 && hash != uint256("0x3b85e70ba7f5433049cfbcf0ae35ed869496dbedcd1c0fafadb0284ec81d7b58")))
            return false;
    return true;
}

string CNamecoinHooks::IrcPrefix()
{
    return "namecoin";
}

unsigned short GetDefaultPort()
{
    return fTestNet ? 18334 : 8334;
}

unsigned int pnSeed[] = { 0x58cea445, 0x2b562f4e, 0x291f20b2, 0 };
const char *strDNSSeed[] = { NULL };

string GetDefaultDataDirSuffix() {
#ifdef __WXMSW__
    // Windows
    return string("Namecoin");
#else
#ifdef MAC_OSX
    return string("Namecoin");
#else
    return string(".namecoin");
#endif
#endif
}

unsigned char GetAddressVersion() { return ((unsigned char)(fTestNet ? 111 : 52)); }
