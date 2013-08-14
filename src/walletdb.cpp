// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "db.h"
#include "wallet.h"
#include <boost/filesystem.hpp>

#ifdef GUI
#include "namecoin.h"
extern std::map<std::vector<unsigned char>, PreparedNameFirstUpdate> mapMyNameFirstUpdate;
extern std::map<uint160, std::vector<unsigned char> > mapMyNameHashes;   // Name for name_new hash (to show name in transaction list)
#endif

using namespace std;
using namespace boost;

uint64 nAccountingEntryNumber = 0;

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

#ifdef GUI
bool CWalletDB::WriteNameFirstUpdate(const std::vector<unsigned char>& vchName,
                                     const uint256& hex,
                                     const uint64& rand,
                                     const std::vector<unsigned char>& vchData,
                                     const CWalletTx &wtx)
{
    CDataStream ssValue;
    ssValue << hex << rand << vchData << wtx;

    nWalletDBUpdated++;
    return Write(make_pair(string("name_firstupdate"), vchName), ssValue, true);
}

bool CWalletDB::EraseNameFirstUpdate(const std::vector<unsigned char>& vchName)
{
    nWalletDBUpdated++;
    return Erase(make_pair(string("name_firstupdate"), vchName));
}
#endif

bool CWalletDB::ReadAccount(const string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount& account)
{
    return Write(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return Write(make_tuple(string("acentry"), acentry.strAccount, ++nAccountingEntryNumber), acentry);
}

int64 CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64 nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    int64 nCreditDebit = 0;

    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_tuple(string("acentry"), (fAllAccounts? string("") : strAccount), uint64(0));
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        entries.push_back(acentry);
    }

    pcursor->close();
}


bool CWalletDB::LoadWallet(CWallet* pwallet)
{
    pwallet->vchDefaultKey.clear();
    int nFileVersion = 0;
    vector<uint256> vWalletUpgrade;

    // Modify defaults
#ifndef __WXMSW__
    // Tray icon sometimes disappears on 9.10 karmic koala 64-bit, leaving no way to access the program
    fMinimizeToTray = false;
    fMinimizeOnClose = false;
#endif

    //// todo: shouldn't we catch exceptions and try to recover and continue?
    CRITICAL_BLOCK(pwallet->cs_mapWallet)
    CRITICAL_BLOCK(pwallet->cs_mapKeys)
    {
        // Get cursor
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
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            string strType;
            ssKey >> strType;
            if (strType == "name")
            {
                string strAddress;
                ssKey >> strAddress;
                ssValue >> pwallet->mapAddressBook[strAddress];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = pwallet->mapWallet[hash];
                ssValue >> wtx;
                wtx.pwallet = pwallet;

                if (wtx.GetHash() != hash)
                    printf("Error in wallet.dat, hash mismatch\n");

                // Undo serialize changes in 31600
                if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
                {
                    if (!ssValue.empty())
                    {
                        char fTmp;
                        char fUnused;
                        ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                        printf("LoadWallet() upgrading tx ver=%d %d '%s' %s\n", wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount.c_str(), hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = fTmp;
                    }
                    else
                    {
                        printf("LoadWallet() repairing tx ver=%d %s\n", wtx.fTimeReceivedIsTxTime, hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = 0;
                    }
                    vWalletUpgrade.push_back(hash);
                }

                hooks->AddToWallet(wtx);
                //// debug print
                //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
                //printf(" %12I64d  %s  %s  %s\n",
                //    wtx.vout[0].nValue,
                //    DateTimeStrFormat("%x %H:%M:%S", wtx.GetBlockTime()).c_str(),
                //    wtx.hashBlock.ToString().substr(0,20).c_str(),
                //    wtx.mapValue["message"].c_str());
                
#ifdef GUI
                int op, nOut;
                std::vector<std::vector<unsigned char> > vvch;
                if (DecodeNameTx(wtx, op, nOut, vvch) && op == OP_NAME_FIRSTUPDATE && vvch.size() == 3)
                {
                    std::vector<unsigned char> &vchName = vvch[0];
                    std::vector<unsigned char> &vchRand = vvch[1];

                    std::vector<unsigned char> vchToHash(vchRand);
                    vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
                    uint160 hash =  Hash160(vchToHash);
                    mapMyNameHashes[hash] = vchName;
                }
#endif
            }
            else if (strType == "acentry")
            {
                string strAccount;
                ssKey >> strAccount;
                uint64 nNumber;
                ssKey >> nNumber;
                if (nNumber > nAccountingEntryNumber)
                    nAccountingEntryNumber = nNumber;
            }
            else if (strType == "key" || strType == "wkey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CWalletKey wkey;
                if (strType == "key")
                    ssValue >> wkey.vchPrivKey;
                else
                    ssValue >> wkey;

                CKey key;
                if (!key.SetPrivKey(wkey.vchPrivKey))
                {
                    printf("Failed to set PrivKey\n");
                    return false;
                }
                else if (!key.SetPubKey(vchPubKey))
                {
                    printf("Failed to set PubKey\n");
                    return false;
                }
                else if (!pwallet->LoadKey(key))
                {
                    return false;
                    printf("Failed to add key\n");
                }
            }
            else if (strType == "mkey")
            {
                unsigned int nID;
                ssKey >> nID;
                CMasterKey kMasterKey;
                ssValue >> kMasterKey;
                if(pwallet->mapMasterKeys.count(nID) != 0)
                {
                    printf("Error reading wallet database: duplicate CMasterKey id %u\n", nID);
                    return false;
                }
                pwallet->mapMasterKeys[nID] = kMasterKey;
                if (pwallet->nMasterKeyMaxID < nID)
                    pwallet->nMasterKeyMaxID = nID;
            }
            else if (strType == "ckey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                vector<unsigned char> vchPrivKey;
                ssValue >> vchPrivKey;
                if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
                {
                    printf("Error reading wallet database: LoadCryptedKey failed\n");
                    return false;
                }
            }
            else if (strType == "defaultkey")
            {
                ssValue >> pwallet->vchDefaultKey;
            }
            else if (strType == "pool")
            {
                int64 nIndex;
                ssKey >> nIndex;
                pwallet->setKeyPool.insert(nIndex);
            }
            else if (strType == "address") 
            { 
                // Based on Codeshark's pull request: https://github.com/bitcoin/bitcoin/pull/2121/files

                uint160 hash160;
                ssKey >> hash160;

                if (!pwallet->LoadAddress(hash160))
                {
                    printf("Error reading wallet database: LoadAddress failed\n");
                    return false;
                }
            }
#ifdef GUI
            else if (strType == "name_firstupdate")
            {
                std::vector<unsigned char> vchName;
                uint256 wtxInHash;

                ssKey >> vchName;
                PreparedNameFirstUpdate prep;
                // Note: name, rand and data are stored unencrypted. Even if we encrypt them,
                // they are recoverable from prep.wtx, which has to be unencrypted (so it can be
                // auto-broadcasted, when name_new is 12 blocks old)
                ssValue >> wtxInHash >> prep.rand >> prep.vchData >> prep.wtx;
                
                prep.wtx.pwallet = pwallet;

                // TODO: also would be good to check that name, rand, wtxInHash and value match with prep.wtx
                // Note: wtxInHash IS NOT prep.wtx.GetHash(), it is the hash of previous name_new

                mapMyNames[vchName] = wtxInHash;
                mapMyNameFirstUpdate[vchName] = prep;

                std::vector<unsigned char> vchRand = CBigNum(prep.rand).getvch();
                std::vector<unsigned char> vchToHash(vchRand);
                vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
                uint160 hash = Hash160(vchToHash);
                mapMyNameHashes[hash] = vchName;
            }
#endif
            else if (strType == "version")
            {
                ssValue >> nFileVersion;
                if (nFileVersion == 10300)
                    nFileVersion = 300;
            }
            else if (strType == "setting")
            {
                string strKey;
                ssKey >> strKey;

                // Options
#ifndef GUI
                if (strKey == "fGenerateBitcoins")  ssValue >> fGenerateBitcoins;
#endif
                if (strKey == "nTransactionFee")    ssValue >> nTransactionFee;
                if (strKey == "nMinimumInputValue") ssValue >> nMinimumInputValue;
                if (strKey == "fLimitProcessors")   ssValue >> fLimitProcessors;
                if (strKey == "nLimitProcessors")   ssValue >> nLimitProcessors;
                if (strKey == "fMinimizeToTray")    ssValue >> fMinimizeToTray;
                if (strKey == "fMinimizeOnClose")   ssValue >> fMinimizeOnClose;
                if (strKey == "fUseProxy")          ssValue >> fUseProxy;
                if (strKey == "addrProxy")          ssValue >> addrProxy;
                if (fHaveUPnP && strKey == "fUseUPnP")           ssValue >> fUseUPnP;
            }
        }
        pcursor->close();
    }

    BOOST_FOREACH(uint256 hash, vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    printf("nFileVersion = %d\n", nFileVersion);

#ifndef GUI
    PrintSettingsToLog();
#endif

    // Upgrade
    if (nFileVersion < VERSION)
    {
        // Get rid of old debug.log file in current directory
        if (nFileVersion <= 105 && !pszSetDataDir[0])
            unlink("debug.log");

        WriteVersion(VERSION);
    }


    return true;
}
