// Copyright (c) 2009-2011 Satoshi Nakamoto & Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "db.h"

LockedPageManager LockedPageManager::instance;

//////////////////////////////////////////////////////////////////////////////
//
// mapKeys
//

std::vector<unsigned char> CKeyStore::GenerateNewKey()
{
    RandAddSeedPerfmon();
    CKey key;
    key.MakeNewKey();
    if (!AddKey(key))
        throw std::runtime_error("GenerateNewKey() : AddKey failed");
    return key.GetPubKey();
}

bool CKeyStore::AddKey(const CKey& key)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!IsCrypted())
        {
            mapKeys[key.GetPubKey()] = key.GetPrivKey();
            mapPubKeys[Hash160(key.GetPubKey())] = key.GetPubKey();
            return true;
        }

        if (IsLocked())
            return false;

        std::vector<unsigned char> vchCryptedSecret;
        std::vector<unsigned char> vchPubKey = key.GetPubKey();
        if (!EncryptSecret(vMasterKey, key.GetPrivKey(), Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
            return false;

        if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
    }
    return true;
}

// Based on Codeshark's pull reqeust: https://github.com/bitcoin/bitcoin/pull/2121/files
bool CKeyStore::AddAddress(const uint160& hash160)
{
    std::vector<unsigned char> vchEmpty;

    // The key is watch-only. We don't have the secret. 
    CRITICAL_BLOCK(cs_mapKeys)
    {
        mapPubKeys[hash160] = vchEmpty;
        if (!IsCrypted())
            mapKeys[vchEmpty] = CPrivKey();
        else
            mapCryptedKeys[vchEmpty] = vchEmpty;
    }
    return true;
}

bool CKeyStore::AddCryptedKey(const std::vector<unsigned char> &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!SetCrypted())
            return false;

        mapCryptedKeys[vchPubKey] = vchCryptedSecret;
        mapPubKeys[Hash160(vchPubKey)] = vchPubKey;
    }
    return true;
}

bool CKeyStore::GetPrivKey(const std::vector<unsigned char> &vchPubKey, CPrivKey& keyOut) const
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!IsCrypted())
        {
            std::map<std::vector<unsigned char>, CPrivKey>::const_iterator mi = mapKeys.find(vchPubKey);
            if (mi != mapKeys.end())
            {
                keyOut = (*mi).second;
                return true;
            }
        }
        else
        {
            CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(vchPubKey);
            if (mi != mapCryptedKeys.end())
            {
                const std::vector<unsigned char> &vchCryptedSecret = (*mi).second;
                if (!DecryptSecret(vMasterKey, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), keyOut))
                {
                    return false;
                }
                return true;
            }
        }
        return false;
    }
}

bool CKeyStore::SetCrypted()
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (fUseCrypto)
            return true;

        if (!mapKeys.empty())
        {
            // importaddress: Imported (watch-only) addresses work equally for both encrypted and non-encrypted wallet,
            // so if the only key is the dummy watch-only key, we just move it to the crypted storage
            std::vector<unsigned char> vchEmpty;
            if (mapKeys.size() == 1 && mapKeys.begin()->first == vchEmpty && mapKeys.begin()->second == CPrivKey())
            {
                mapKeys.erase(vchEmpty);
                mapCryptedKeys[vchEmpty] = vchEmpty;
            }
            else
                return false;
        }

        fUseCrypto = true;
        return true;
    }
}

bool CKeyStore::Lock()
{
    if (!SetCrypted())
        return false;

    CRITICAL_BLOCK(cs_mapKeys)
        vMasterKey.clear();
#ifdef GUI
    NotifyStatusChanged(this);
#endif
    return true;
}

bool CKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!SetCrypted())
            return false;

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const std::vector<unsigned char> &vchPubKey = mi->first;
            const std::vector<unsigned char> &vchCryptedSecret = mi->second;
            CSecret vchSecret;
            if (!DecryptSecret(vMasterKeyIn, vchCryptedSecret, Hash(vchPubKey.begin(), vchPubKey.end()), vchSecret))
                return false;
            /*CKey key;
            key.SetPubKey(vchPubKey);
            key.SetSecret(vchSecret);
            if (key.GetPubKey() == vchPubKey)
                break;
            return false;*/
            break;
        }
        vMasterKey = vMasterKeyIn;
    }
#ifdef GUI
    NotifyStatusChanged(this);
#endif
    return true;
}

bool CKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!mapCryptedKeys.empty() || IsCrypted())
            return false;

        fUseCrypto = true;
        BOOST_FOREACH(KeyMap::value_type& mKey, mapKeys)
        {
            const std::vector<unsigned char> &vchPubKey = mKey.first;
            std::vector<unsigned char> vchCryptedSecret;

            // Skip encryption for watch-only addresses (importaddress)
            if (!vchPubKey.empty())
            {
                if (!EncryptSecret(vMasterKeyIn, mKey.second, Hash(vchPubKey.begin(), vchPubKey.end()), vchCryptedSecret))
                    return false;
            }
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        mapKeys.clear();
    }
    return true;
}
