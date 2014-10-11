#ifndef NAMECOIN_H
#define NAMECOIN_H

#include "json/json_spirit.h"

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

/* Maximum value length that is allowed by the UIs.  Currently,
   if the value is set above 520 bytes, it can't ever be updated again
   due to limitations in the scripting system.  Enforce this
   in the UIs.  */
static const int UI_MAX_VALUE_LENGTH = 520;

class CNameDB;
class CNameIndex;
class CDiskTxPos;
class uint256;

extern std::map<vchType, uint256> mapMyNames;
extern std::map<vchType, std::set<uint256> > mapNamePending;
extern std::set<vchType> setNewHashes;

std::string stringFromVch(const std::vector<unsigned char> &vch);
std::vector<unsigned char> vchFromString(const std::string &str);
int GetTxPosHeight(const CNameIndex& txPos);
int GetTxPosHeight(const CDiskTxPos& txPos);
int GetTxPosHeight2(const CDiskTxPos& txPos, int nHeight);
bool GetTxOfName(CNameDB& dbName, const std::vector<unsigned char> &vchName, CTransaction& tx);
int IndexOfNameOutput(const CTransaction& tx);
bool GetValueOfTxPos(const CNameIndex& txPos, std::vector<unsigned char>& vchValue, uint256& hash, int& nHeight);
bool GetValueOfTxPos(const CDiskTxPos& txPos, std::vector<unsigned char>& vchValue, uint256& hash, int& nHeight);
int GetDisplayExpirationDepth(int nHeight);
bool GetNameOfTx(const CTransaction& tx, std::vector<unsigned char>& name);
bool GetValueOfNameTx(const CTransaction& tx, std::vector<unsigned char>& value);
bool DecodeNameTx(const CTransaction& tx, int& op, int& nOut, std::vector<std::vector<unsigned char> >& vvch, int nHeight);
bool DecodeNameScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch, CScript::const_iterator& pc);
bool DecodeNameScript(const CScript& script, int& op, std::vector<std::vector<unsigned char> > &vvch);
bool GetNameAddress(const CTransaction& tx, std::string& strAddress);
std::string SendMoneyWithInputTx(const CScript& scriptPubKey, int64 nValue, int64 nNetFee, const CWalletTx& wtxIn, CWalletTx& wtxNew, bool fAskFee);
bool CreateTransactionWithInputTx(const std::vector<std::pair<CScript, int64> >& vecSend, const CWalletTx& wtxIn, int nTxOut, CWalletTx& wtxNew, CReserveKey& reservekey, int64& nFeeRet);
int64 GetNetworkFee(int nHeight);

/* Handle the name operation part of the RPC call createrawtransaction.  */
void AddRawTxNameOperation(CTransaction& tx, const json_spirit::Object& obj);

#endif // NAMECOIN_H
