// Copyright (c) 2010-2011 Vincent Durham
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
#include "headers.h"

#include "namecoin.h"

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

using namespace json_spirit;

static const bool NAME_DEBUG = false;
typedef Value(*rpcfn_type)(const Array& params, bool fHelp);
extern map<string, rpcfn_type> mapCallTable;
extern int64 AmountFromValue(const Value& value);
extern Object JSONRPCError(int code, const string& message);
template<typename T> void ConvertTo(Value& value);

extern bool SelectCoins(int64 nTargetValue, set<pair<CWalletTx*,unsigned int> >& setCoinsRet, int64& nValueRet);

static const int NAMECOIN_TX_VERSION = 0x7100;
static const int64 MIN_AMOUNT = CENT;
static const int MAX_NAME_LENGTH = 255;
static const int MAX_VALUE_LENGTH = 1023;
static const int OP_NAME_INVALID = 0x00;
static const int OP_NAME_NEW = 0x01;
static const int OP_NAME_FIRSTUPDATE = 0x02;
static const int OP_NAME_UPDATE = 0x03;
static const int OP_NAME_NOP = 0x04;
static const int MIN_FIRSTUPDATE_DEPTH = 12;

map<vector<unsigned char>, uint256> mapMyNames;
map<vector<unsigned char>, set<uint256> > mapNamePending;
extern CCriticalSection cs_mapWallet;
extern bool EraseFromWallet(uint256 hash);

// forward decls
extern bool DecodeNameScript(const CScript& script, int& op, vector<vector<unsigned char> > &vvch);
extern bool DecodeNameScript(const CScript& script, int& op, vector<vector<unsigned char> > &vvch, CScript::const_iterator& pc);
extern int IndexOfNameOutput(CWalletTx& wtx);
extern bool Solver(const CScript& scriptPubKey, uint256 hash, int nHashType, CScript& scriptSigRet);
extern bool VerifyScript(const CScript& scriptSig, const CScript& scriptPubKey, const CTransaction& txTo, unsigned int nIn, int nHashType);
extern bool GetValueOfNameTx(const CTransaction& tx, vector<unsigned char>& value);
extern bool IsConflictedTx(CTxDB& txdb, const CTransaction& tx, vector<unsigned char>& name);
extern bool GetNameOfTx(const CTransaction& tx, vector<unsigned char>& name);

const int NAME_COIN_GENESIS_EXTRA = 521;
uint256 hashNameCoinGenesisBlock("000000000062b72c5e2ceb45fbc8587e807c155b0da735e6483dfba2f0a9c770");

class CNamecoinHooks : public CHooks
{
public:
    virtual bool IsStandard(const CScript& scriptPubKey);
    virtual void AddToWallet(CWalletTx& tx);
    virtual bool CheckTransaction(const CTransaction& tx);
    virtual bool ConnectInputs(CTxDB& txdb,
            map<uint256, CTxIndex>& mapTestPool,
            const CTransaction& tx,
            vector<CTransaction>& vTxPrev,
            vector<CTxIndex>& vTxindex,
            CBlockIndex* pindexBlock,
            CDiskTxPos& txPos,
            bool fBlock,
            bool fMiner);
    virtual bool DisconnectInputs(CTxDB& txdb,
            const CTransaction& tx,
            CBlockIndex* pindexBlock);
    virtual bool ConnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex);
    virtual bool DisconnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex);
    virtual bool ExtractAddress(const CScript& script, string& address);
    virtual bool GenesisBlock(CBlock& block);
    virtual bool Lockin(int nHeight, uint256 hash);
    virtual int LockinHeight();
    virtual string IrcPrefix();
    virtual void AcceptToMemoryPool(CTxDB& txdb, const CTransaction& tx);

    virtual void MessageStart(char* pchMessageStart)
    {
        // Make the message start different
        pchMessageStart[3] = 0xfe;
    }
    virtual bool IsMine(const CTransaction& tx);
    virtual bool IsMine(const CTransaction& tx, const CTxOut& txout);
};

int64 getAmount(Value value)
{
    ConvertTo<double>(value);
    double dAmount = value.get_real();
    int64 nAmount = roundint64(dAmount * COIN);
    if (!MoneyRange(nAmount))
        throw JSONRPCError(-3, "Invalid amount");
    return nAmount;
}

vector<unsigned char> vchFromValue(const Value& value) {
    string strName = value.get_str();
    unsigned char *strbeg = (unsigned char*)strName.c_str();
    return vector<unsigned char>(strbeg, strbeg + strName.size());
}

vector<unsigned char> vchFromString(string str) {
    unsigned char *strbeg = (unsigned char*)str.c_str();
    return vector<unsigned char>(strbeg, strbeg + str.size());
}

string stringFromVch(vector<unsigned char> vch) {
    string res;
    vector<unsigned char>::iterator vi = vch.begin();
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
    if (nHeight < 24000)
        return 12000;
    return 36000;
}

int64 GetNetworkFee(int nHeight)
{
    // Speed up network fee decrease 4x starting at 24000
    if (nHeight >= 24000)
        nHeight += (nHeight - 24000) * 3;
    int64 nStart = 50 * COIN;
    if (fTestNet)
        nStart = 10 * CENT;
    int64 nRes = nStart >> (nHeight >> 13);
    nRes -= (nRes >> 14) * (nHeight % 8192);
    return nRes;
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


int GetNameHeight(CTxDB& txdb, vector<unsigned char> vchName) {
    CNameDB dbName("cr", txdb);
    vector<CDiskTxPos> vtxPos;
    if (dbName.ExistsName(vchName))
    {
        if (!dbName.ReadName(vchName, vtxPos))
            return error("GetNameHeight() : failed to read from name DB");
        if (vtxPos.empty())
            return -1;
        CDiskTxPos& txPos = vtxPos.back();
        return GetTxPosHeight(txPos);
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

bool SignNameSignature(const CTransaction& txFrom, CTransaction& txTo, unsigned int nIn, int nHashType=SIGHASH_ALL, CScript scriptPrereq=CScript())
{
    assert(nIn < txTo.vin.size());
    CTxIn& txin = txTo.vin[nIn];
    assert(txin.prevout.n < txFrom.vout.size());
    const CTxOut& txout = txFrom.vout[txin.prevout.n];

    // Leave out the signature from the hash, since a signature can't sign itself.
    // The checksig op will also drop the signatures from its hash.

    const CScript& scriptPubKey = RemoveNameScriptPrefix(txout.scriptPubKey);
    uint256 hash = SignatureHash(scriptPrereq + txout.scriptPubKey, txTo, nIn, nHashType);

    if (!Solver(scriptPubKey, hash, nHashType, txin.scriptSig))
        return false;

    txin.scriptSig = scriptPrereq + txin.scriptSig;

    // Test solution
    if (scriptPrereq.empty())
        if (!VerifyScript(txin.scriptSig, txout.scriptPubKey, txTo, nIn, 0))
            return false;

    return true;
}

bool IsMyName(const CTransaction& tx, const CTxOut& txout)
{
    const CScript& scriptPubKey = RemoveNameScriptPrefix(txout.scriptPubKey);
    CScript scriptSig;
    if (!Solver(scriptPubKey, 0, 0, scriptSig))
        return false;
    return true;
}

bool CreateTransactionWithInputTx(const vector<pair<CScript, int64> >& vecSend, CWalletTx& wtxIn, int nTxOut, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet)
{
    int64 nValue = 0;
    foreach (const PAIRTYPE(CScript, int64)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    CRITICAL_BLOCK(cs_main)
    {
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        CRITICAL_BLOCK(cs_mapWallet)
        {
            nFeeRet = nTransactionFee;
            loop
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                int64 nTotalValue = nValue + nFeeRet;
                printf("total value = %d\n", nTotalValue);
                double dPriority = 0;
                // vouts to the payees
                foreach (const PAIRTYPE(CScript, int64)& s, vecSend)
                    wtxNew.vout.push_back(CTxOut(s.second, s.first));

                int64 nWtxinCredit = wtxIn.vout[nTxOut].nValue;

                // Choose coins to use
                set<pair<CWalletTx*, unsigned int> > setCoins;
                int64 nValueIn = 0;
                if (!SelectCoins(nTotalValue - nWtxinCredit, setCoins, nValueIn))
                    return false;

                vector<pair<CWalletTx*, unsigned int> >
                    vecCoins(setCoins.begin(), setCoins.end());

                foreach(PAIRTYPE(CWalletTx*, unsigned int)& coin, vecCoins)
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
                    assert(mapKeys.count(vchPubKey));

                    // Fill a vout to ourself, using same address type as the payment
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
                foreach(PAIRTYPE(CWalletTx*, unsigned int)& coin, vecCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(), coin.second));

                // Sign
                int nIn = 0;
                foreach(PAIRTYPE(CWalletTx*, unsigned int)& coin, vecCoins)
                {
                    if (coin.first == &wtxIn && coin.second == nTxOut)
                    {
                        if (!SignNameSignature(*coin.first, wtxNew, nIn++))
                            throw runtime_error("could not sign name coin output");
                    }
                    else
                    {
                        if (!SignSignature(*coin.first, wtxNew, nIn++))
                            return false;
                    }
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
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

// nTxOut is the output from wtxIn that we should grab
string SendMoneyWithInputTx(CScript scriptPubKey, int64 nValue, int64 nNetFee, CWalletTx& wtxIn, CWalletTx& wtxNew, bool fAskFee)
{
    int nTxOut = IndexOfNameOutput(wtxIn);
    CRITICAL_BLOCK(cs_main)
    {
        CReserveKey reservekey;
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
            if (nValue + nFeeRequired > GetBalance())
                strError = strprintf(_("Error: This is an oversized transaction that requires a transaction fee of %s  "), FormatMoney(nFeeRequired).c_str());
            else
                strError = _("Error: Transaction creation failed  ");
            printf("SendMoney() : %s", strError.c_str());
            return strError;
        }

        if (fAskFee && !ThreadSafeAskFee(nFeeRequired, _("Sending..."), NULL))
            return "ABORTED";

        if (!CommitTransaction(wtxNew, reservekey))
            return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
    }
    MainFrameRepaint();
    return "";
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

bool GetValueOfName(CNameDB& dbName, vector<unsigned char> vchName, vector<unsigned char>& vchValue, int& nHeight)
{
    vector<CDiskTxPos> vtxPos;
    if (!dbName.ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;
    CDiskTxPos& txPos = vtxPos.back();

    uint256 hash;

    return GetValueOfTxPos(txPos, vchValue, hash, nHeight);
}

bool GetTxOfName(CNameDB& dbName, vector<unsigned char> vchName, CTransaction& tx)
{
    vector<CDiskTxPos> vtxPos;
    if (!dbName.ReadName(vchName, vtxPos) || vtxPos.empty())
        return false;
    CDiskTxPos& txPos = vtxPos.back();
    int nHeight = GetTxPosHeight(txPos);
    if (nHeight + GetExpirationDepth(pindexBest->nHeight) < pindexBest->nHeight)
    {
        string name = stringFromVch(vchName);
        printf("GetTxOfName(%s) : expired", name.c_str());
        return false;
    }

    if (!tx.ReadFromDisk(txPos))
        return error("GetTxOfName() : could not read tx from disk");
    return true;
}

Value name_list(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "name_list [<name>]\n"
                "list my own names"
                );

    vector<unsigned char> vchName;
    vector<unsigned char> vchLastName;
    int nMax = 500;

    Array oRes;

    CRITICAL_BLOCK(cs_mapWallet)
    for (;;)
    {
        CNameDB dbName("r");

        vector<pair<vector<unsigned char>, CDiskTxPos> > nameScan;
        if (!dbName.ScanNames(vchName, nMax, nameScan))
            throw JSONRPCError(-4, "scan failed");

        pair<vector<unsigned char>, CDiskTxPos> pairScan;
        foreach (pairScan, nameScan)
        {
            // skip previous last
            if (pairScan.first == vchLastName)
                continue;

            vchLastName = pairScan.first;
            string name = stringFromVch(pairScan.first);
            CDiskTxPos txPos = pairScan.second;
            vector<unsigned char> vchValue;
            int nHeight;
            uint256 hash;
            if (!txPos.IsNull() &&
                    GetValueOfTxPos(txPos, vchValue, hash, nHeight) &&
                    mapWallet.count(hash)
                    )
            {
                string value = stringFromVch(vchValue);
                Object oName;
                oName.push_back(Pair("name", name));
                oName.push_back(Pair("value", value));
                if (!hooks->IsMine(mapWallet[hash]))
                    oName.push_back(Pair("transferred", 1));
                oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
                oRes.push_back(oName);
            }
        }

        // break if nothing more
        if (vchName == vchLastName)
            break;
        vchName = vchLastName;
    }

    return oRes;
}

Value name_debug(const Array& params, bool fHelp)
{
    printf("Pending:\n----------------------------\n");
    pair<vector<unsigned char>, set<uint256> > pairPending;

    CRITICAL_BLOCK(cs_main)
    foreach (pairPending, mapNamePending)
    {
        string name = stringFromVch(pairPending.first);
        printf("%s :\n", name.c_str());
        uint256 hash;
        foreach(hash, pairPending.second)
        {
            printf("    ");
            if (!mapWallet.count(hash))
                printf("foreign ");
            printf("    %s\n", hash.GetHex().c_str());
        }
    }
    printf("----------------------------\n");
    return true;
}

Value name_debug1(const Array& params, bool fHelp)
{
    if (params.size() != 1)
        throw runtime_error("expecting a name");
    vector<unsigned char> vchName = vchFromValue(params[0]);
    printf("Dump name:\n");
    CRITICAL_BLOCK(cs_main)
    {
        vector<CDiskTxPos> vtxPos;
        CNameDB dbName("r");
        if (!dbName.ReadName(vchName, vtxPos))
        {
            error("failed to read from name DB");
            return false;
        }
        CDiskTxPos txPos;
        foreach(txPos, vtxPos)
        {
            CTransaction tx;
            if (!tx.ReadFromDisk(txPos))
            {
                error("could not read txpos %s", txPos.ToString().c_str());
                continue;
            }
            printf("@%d %s\n", GetTxPosHeight(txPos), tx.GetHash().GetHex().c_str());
        }
    }
    printf("-------------------------\n");
    return true;
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

    vector<pair<vector<unsigned char>, CDiskTxPos> > nameScan;
    if (!dbName.ScanNames(vchName, nMax, nameScan))
        throw JSONRPCError(-4, "scan failed");

    pair<vector<unsigned char>, CDiskTxPos> pairScan;
    foreach (pairScan, nameScan)
    {
        Object oName;
        string name = stringFromVch(pairScan.first);
        CDiskTxPos txPos = pairScan.second;
        oName.push_back(Pair("name", name));
        vector<unsigned char> vchValue;
        int nHeight;
        uint256 hash;
        if (!txPos.IsNull() && GetValueOfTxPos(txPos, vchValue, hash, nHeight))
        {
            string value = stringFromVch(vchValue);
            oName.push_back(Pair("value", value));
            oName.push_back(Pair("txid", hash.GetHex()));
            oName.push_back(Pair("expires_in", nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight));
        }
        else
        {
            oName.push_back(Pair("expired", 1));
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
    if (fHelp || params.size() < 3 || params.size() > 4)
        throw runtime_error(
                "name_firstupdate <name> <rand> [<tx>] <value>\n"
                "Perform a first update after a name_new reservation.\n"
                "Note that the first update will go into a block 12 blocks after the name_new, at the soonest."
                );
    vector<unsigned char> vchName = vchFromValue(params[0]);
    vector<unsigned char> vchRand = ParseHex(params[1].get_str());
    vector<unsigned char> vchValue;

    if (params.size() == 3)
    {
        vchValue = vchFromValue(params[2]);
    }
    else
    {
        vchValue = vchFromValue(params[3]);
    }


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

    CRITICAL_BLOCK(cs_main)
    {
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

        if (!mapWallet.count(wtxInHash))
        {
            throw runtime_error("previous transaction is not in the wallet");
        }

	    vector<unsigned char> strPubKey = GetKeyFromKeyPool();
	    CScript scriptPubKeyOrig;
    	scriptPubKeyOrig.SetBitcoinAddress(strPubKey);
	    CScript scriptPubKey;
    	scriptPubKey << OP_NAME_FIRSTUPDATE << vchName << vchRand << vchValue << OP_2DROP << OP_2DROP;
	    scriptPubKey += scriptPubKeyOrig;

        CWalletTx& wtxIn = mapWallet[wtxInHash];
        vector<unsigned char> vchHash;
        bool found = false;
        foreach (CTxOut& out, wtxIn.vout)
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
            throw JSONRPCError(-4, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_update(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
                "name_update <name> <value> [<toaddress>]\nUpdate and possibly transfer a name\n"
                );

    vector<unsigned char> vchName = vchFromValue(params[0]);
    vector<unsigned char> vchValue = vchFromValue(params[1]);

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;
    vector<unsigned char> strPubKey = GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;

    if (params.size() == 3)
    {
        string strAddress = params[2].get_str();
        scriptPubKeyOrig.SetBitcoinAddress(strAddress);
    }
    else
    {
        scriptPubKeyOrig.SetBitcoinAddress(strPubKey);
    }

    CScript scriptPubKey;
    scriptPubKey << OP_NAME_UPDATE << vchName << vchValue << OP_2DROP << OP_DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_firstupdate() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            throw runtime_error("there are pending operations on that name");
        }

        CNameDB dbName("r");
        CTransaction tx;
        if (!GetTxOfName(dbName, vchName, tx))
        {
            throw runtime_error("could not find a coin with this name");
        }

        uint256 wtxInHash = tx.GetHash();

        if (!mapWallet.count(wtxInHash))
        {
            error("name_update() : this coin is not in your wallet %s",
                    wtxInHash.GetHex().c_str());
            throw runtime_error("this coin is not in your wallet");
        }

        CWalletTx& wtxIn = mapWallet[wtxInHash];
        string strError = SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, 0, wtxIn, wtx, false);
        if (strError != "")
            throw JSONRPCError(-4, strError);
    }
    return wtx.GetHash().GetHex();
}

Value name_new(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "name_new <name>\n"
                );

    vector<unsigned char> vchName = vchFromValue(params[0]);

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    uint64 rand = GetRand((uint64)-1);
    vector<unsigned char> vchRand = CBigNum(rand).getvch();
    vector<unsigned char> vchToHash(vchRand);
    vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
    uint160 hash =  Hash160(vchToHash);

    vector<unsigned char> strPubKey = GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig.SetBitcoinAddress(strPubKey);
    CScript scriptPubKey;
    scriptPubKey << OP_NAME_NEW << hash << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;

    string strError = SendMoney(scriptPubKey, MIN_AMOUNT, wtx, false);
    if (strError != "")
        throw JSONRPCError(-4, strError);
    mapMyNames[vchName] = wtx.GetHash();
    vector<Value> res;
    res.push_back(wtx.GetHash().GetHex());
    res.push_back(HexStr(vchRand));
    return res;
}

void UnspendInputs(CWalletTx& wtx)
{
    set<CWalletTx*> setCoins;
    foreach(const CTxIn& txin, wtx.vin)
    {
        if (!txin.IsMine())
        {
            printf("UnspendInputs(): !mine %s", txin.ToString().c_str());
            continue;
        }
        CWalletTx& prev = mapWallet[txin.prevout.hash];
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
        vWalletUpdated.push_back(prev.GetHash());
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
    CRITICAL_BLOCK(cs_mapWallet)
    {
      uint256 hash;
      hash.SetHex(params[0].get_str());
      if (!mapWallet.count(hash))
        throw runtime_error("transaction not in wallet");

      if (!mapTransactions.count(hash))
        throw runtime_error("transaction not in memory - is already in blockchain?");
      CWalletTx wtx = mapWallet[hash];
      UnspendInputs(wtx);

      // We are not removing from mapTransactions because this can cause memory corruption
      // during mining.  The user should restart to clear the tx from memory.
      wtx.RemoveFromMemoryPool();
      EraseFromWallet(wtx.GetHash());
      vector<unsigned char> vchName;
      if (GetNameOfTx(wtx, vchName) && mapNamePending.count(vchName)) {
        printf("deletetransaction() : remove from pending");
        mapNamePending[vchName].erase(wtx.GetHash());
      }
      return "success, please restart program to clear memory";
    }
}

Value name_clean(const Array& params, bool fHelp)
{
    if (fHelp || params.size())
        throw runtime_error("name_clean\nClean unsatisfiable transactions from the wallet - including name_update on an already taken name\n");

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx> mapRemove;

        printf("-----------------------------\n");

        {
            CTxDB txdb("r");
            foreach(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            {
                CWalletTx& wtx = item.second;
                vector<unsigned char> vchName;
                if (wtx.GetDepthInMainChain() < 1 && IsConflictedTx(txdb, wtx, vchName))
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
            foreach(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            {
                CWalletTx& wtx = item.second;
                foreach(const CTxIn& txin, wtx.vin)
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

        foreach(PAIRTYPE(const uint256, CWalletTx)& item, mapRemove)
        {
            CWalletTx& wtx = item.second;

            UnspendInputs(wtx);
            wtx.RemoveFromMemoryPool();
            EraseFromWallet(wtx.GetHash());
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
            foreach(CDiskTxPos& txPos, vtxPos) {
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
        vector<pair<vector<unsigned char>, CDiskTxPos> >& nameScan)
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
            vector<CDiskTxPos> vtxPos;
            ssValue >> vtxPos;
            CDiskTxPos txPos;
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

CHooks* InitHook()
{
    mapCallTable.insert(make_pair("name_new", &name_new));
    mapCallTable.insert(make_pair("name_update", &name_update));
    mapCallTable.insert(make_pair("name_firstupdate", &name_firstupdate));
    mapCallTable.insert(make_pair("name_list", &name_list));
    mapCallTable.insert(make_pair("name_scan", &name_scan));
    mapCallTable.insert(make_pair("name_debug", &name_debug));
    mapCallTable.insert(make_pair("name_debug1", &name_debug1));
    mapCallTable.insert(make_pair("name_clean", &name_clean));
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

bool DecodeNameTx(const CTransaction& tx, int& op, int& nOut, vector<vector<unsigned char> >& vvch)
{
    bool found = false;

    for (int i = 0 ; i < tx.vout.size() ; i++)
    {
        const CTxOut& out = tx.vout[i];
        if (DecodeNameScript(out.scriptPubKey, op, vvch))
        {
            // If more than one name op, fail
            if (found)
                return false;
            nOut = i;
            found = true;
        }
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

    if (!DecodeNameTx(tx, op, nOut, vvch))
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

int IndexOfNameOutput(CWalletTx& wtx)
{
    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;

    bool good = DecodeNameTx(wtx, op, nOut, vvch);

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

    bool good = DecodeNameTx(tx, op, nOut, vvch);

    if (!good)
    {
        error("IsMine() hook : no output out script in name tx %s\n", tx.ToString().c_str());
        return false;
    }

    const CTxOut& txout = tx.vout[nOut];
    if (IsMyName(tx, txout))
    {
        printf("IsMine() hook : found my transaction %s nout %d\n", tx.GetHash().GetHex().c_str(), nOut);
        return true;
    }
    return false;
}

bool CNamecoinHooks::IsMine(const CTransaction& tx, const CTxOut& txout)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;

    vector<vector<unsigned char> > vvch;

    int op;
    int nOut;
 
    if (!DecodeNameScript(txout.scriptPubKey, op, vvch))
        return false;

    if (IsMyName(tx, txout))
    {
        printf("IsMine() hook : found my transaction %s value %ld\n", tx.GetHash().GetHex().c_str(), txout.nValue);
        return true;
    }
    return false;
}

void CNamecoinHooks::AcceptToMemoryPool(CTxDB& txdb, const CTransaction& tx)
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

    bool good = DecodeNameTx(tx, op, nOut, vvch);

    if (!good)
    {
        error("AcceptToMemoryPool() : no output out script in name tx %s", tx.ToString().c_str());
        return;
    }

    CRITICAL_BLOCK(cs_main)
    {
        if (op != OP_NAME_NEW)
        {
            mapNamePending[vvch[0]].insert(tx.GetHash());
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

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
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

bool IsConflictedTx(CTxDB& txdb, const CTransaction& tx, vector<unsigned char>& name)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return false;
    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
    if (!good)
        return error("IsConflictedTx() : could not decode a namecoin tx");
    int nPrevHeight;
    int nDepth;
    int64 nNetFee;

    switch (op)
    {
        case OP_NAME_FIRSTUPDATE:
            nPrevHeight = GetNameHeight(txdb, vvchArgs[0]);
            name = vvchArgs[0];
            if (nPrevHeight >= 0 && pindexBest->nHeight - nPrevHeight < GetExpirationDepth(pindexBest->nHeight))
                return true;
    }
    return false;
}

bool CNamecoinHooks::ConnectInputs(CTxDB& txdb,
        map<uint256, CTxIndex>& mapTestPool,
        const CTransaction& tx,
        vector<CTransaction>& vTxPrev,
        vector<CTxIndex>& vTxindex,
        CBlockIndex* pindexBlock,
        CDiskTxPos& txPos,
        bool fBlock,
        bool fMiner)
{
    bool nInput;
    bool found = false;

    int prevOp;
    vector<vector<unsigned char> > vvchPrevArgs;

    for (int i = 0 ; i < tx.vin.size() ; i++) {
        CTxOut& out = vTxPrev[i].vout[tx.vin[i].prevout.n];
        if (DecodeNameScript(out.scriptPubKey, prevOp, vvchPrevArgs))
        {
            if (found)
                return error("ConnectInputHook() : multiple previous name transactions");
            found = true;
            nInput = i;
        }
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

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
    if (!good)
        return error("ConnectInputsHook() : could not decode a namecoin tx");

    int nPrevHeight;
    int nDepth;
    int64 nNetFee;

    switch (op)
    {
        case OP_NAME_NEW:
            if (found)
                return error("ConnectInputsHook() : name_new tx pointing to previous namecoin tx");
            break;
        case OP_NAME_FIRSTUPDATE:
            nNetFee = GetNameNetFee(tx);
            if (nNetFee < GetNetworkFee(pindexBlock->nHeight))
                return error("ConnectInputsHook() : got tx %s with fee too low %d", tx.GetHash().GetHex().c_str(), nNetFee);
            if (!found || prevOp != OP_NAME_NEW)
                return error("ConnectInputsHook() : name_firstupdate tx without previous name_new tx");
            nPrevHeight = GetNameHeight(txdb, vvchArgs[0]);
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
                foreach (const PAIRTYPE(uint256, const CTxIndex&)& s, mapTestPool)
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
            // TODO CPU intensive
            nDepth = CheckTransactionAtRelativeDepth(pindexBlock, vTxindex[nInput], GetExpirationDepth(pindexBlock->nHeight));
            if ((fBlock || fMiner) && nDepth < 0)
                return error("ConnectInputsHook() : name_update on an expired name, or there is a pending transaction on the name");
            break;
        default:
            return error("ConnectInputsHook() : name transaction has unknown op");
    }

    if (fBlock)
    {
        CNameDB dbName("cr+", txdb);

        dbName.TxnBegin();

        if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
        {
            vector<CDiskTxPos> vtxPos;
            if (dbName.ExistsName(vvchArgs[0]))
            {
                if (!dbName.ReadName(vvchArgs[0], vtxPos))
                    return error("ConnectInputsHook() : failed to read from name DB");
            }
            vtxPos.push_back(txPos);
            if (!dbName.WriteName(vvchArgs[0], vtxPos))
                return error("ConnectInputsHook() : failed to write to name DB");
        }

        dbName.TxnCommit();
    }

    CRITICAL_BLOCK(cs_main)
    {
        if (fBlock && op != OP_NAME_NEW)
        {
            if (mapNamePending[vvchArgs[0]].count(tx.GetHash()))
                mapNamePending[vvchArgs[0]].erase(tx.GetHash());
            else
                printf("ConnectInputsHook() : connecting inputs on %s which was not in pending - must be someone elses\n",
                        tx.GetHash().GetHex().c_str());
        }
    }

    return true;
}

bool CNamecoinHooks::DisconnectInputs(CTxDB& txdb,
        const CTransaction& tx,
        CBlockIndex* pindexBlock)
{
    if (tx.nVersion != NAMECOIN_TX_VERSION)
        return true;

    vector<vector<unsigned char> > vvchArgs;
    int op;
    int nOut;

    bool good = DecodeNameTx(tx, op, nOut, vvchArgs);
    if (!good)
        return error("DisconnectInputsHook() : could not decode namecoin tx");
    if (op == OP_NAME_FIRSTUPDATE || op == OP_NAME_UPDATE)
    {
        CNameDB dbName("cr+", txdb);

        dbName.TxnBegin();

        vector<CDiskTxPos> vtxPos;
        if (!dbName.ReadName(vvchArgs[0], vtxPos))
            return error("DisconnectInputsHook() : failed to read from name DB");
        // vtxPos might be empty if we pruned expired transactions.  However, it should normally still not
        // be empty, since a reorg cannot go that far back.  Be safe anyway and do not try to pop if empty.
        if (vtxPos.size())
        {
            vtxPos.pop_back();
            // TODO validate that the first pos is the current tx pos
        }
        if (!dbName.WriteName(vvchArgs[0], vtxPos))
            return error("DisconnectInputsHook() : failed to write to name DB");

        dbName.TxnCommit();
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

    bool good = DecodeNameTx(tx, op, nOut, vvch);

    if (!good)
    {
        return error("name transaction has unknown script format");
    }

    if (vvch[0].size() > MAX_NAME_LENGTH)
    {
        return error("name transaction with name too long");
    }

    switch (op)
    {
        case OP_NAME_NEW:
            if (vvch[0].size() != 20)
            {
                return error("name_new tx with incorrect hash length");
            }
            break;
        case OP_NAME_FIRSTUPDATE:
            if (vvch[1].size() > 20)
            {
                return error("name_firstupdate tx with rand too big");
            }
            if (vvch[2].size() > MAX_VALUE_LENGTH)
            {
                return error("name_firstupdate tx with value too long");
            }
            break;
        case OP_NAME_UPDATE:
            if (vvch[1].size() > MAX_VALUE_LENGTH)
            {
                return error("name_update tx with value too long");
            }
            break;
        default:
            return error("name transaction has unknown op");
    }
    return true;
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
    string strName = stringFromVch(vvch[0]);
    if (op == OP_NAME_NEW)
        strName = HexStr(vvch[0]);

    address = strOp + ": " + strName;
    return true;
}

bool CNamecoinHooks::ConnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex)
{
    return true;
}

bool CNamecoinHooks::DisconnectBlock(CBlock& block, CTxDB& txdb, CBlockIndex* pindex)
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
    return 0;
}

bool CNamecoinHooks::Lockin(int nHeight, uint256 hash)
{
    return true;
}

string CNamecoinHooks::IrcPrefix()
{
    return "namecoin";
}

unsigned short GetDefaultPort()
{
    return fTestNet ? htons(18334) : htons(8334);
}

unsigned int pnSeed[] = { 0x58cea445, 0x2b562f4e, 0 };
const char *strDNSSeed[] = { NULL };

string GetDefaultDataDirSuffix() {
#ifdef __WXMSW__
    // Windows
    return string("Namecoin");
#else
#ifdef __WXMAC_OSX__
    return string("Namecoin");
#else
    return string(".namecoin");
#endif
#endif
}

unsigned char GetAddressVersion() { return ((unsigned char)(fTestNet ? 111 : 52)); }
