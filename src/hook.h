// Copyright (c) 2010-2011 Vincent Durham
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HOOKS_H
#define BITCOIN_HOOKS_H

class CHooks
{
public:
    virtual bool IsStandard(const CScript& scriptPubKey) = 0;
    virtual void AddToWallet(CWalletTx& tx) = 0;
    virtual bool CheckTransaction(const CTransaction& tx) = 0;
    virtual bool ConnectInputs(DatabaseSet& dbset,
            std::map<uint256, CTxIndex>& mapTestPool,
            const CTransaction& tx,
            std::vector<CTransaction>& vTxPrev,
            std::vector<CTxIndex>& vTxindex,
            CBlockIndex* pindexBlock,
            CDiskTxPos& txPos,
            bool fBlock,
            bool fMiner) = 0;
    virtual bool DisconnectInputs (DatabaseSet& txdb,
            const CTransaction& tx,
            CBlockIndex* pindexBlock) = 0;
    virtual bool ConnectBlock (CBlock& block, DatabaseSet& dbset,
                               CBlockIndex* pindex) = 0;
    virtual bool DisconnectBlock (CBlock& block, DatabaseSet& txdb,
                                  CBlockIndex* pindex) = 0;
    virtual bool ExtractAddress(const CScript& script, std::string& address) = 0;
    virtual bool GenesisBlock(CBlock& block) = 0;
    virtual bool Lockin(int nHeight, uint256 hash) = 0;
    virtual int LockinHeight() = 0;
    virtual std::string IrcPrefix() = 0;
    virtual void MessageStart(char* pchMessageStart) = 0;
    virtual bool AcceptToMemoryPool(DatabaseSet& dbset, const CTransaction& tx) = 0;
    virtual void RemoveFromMemoryPool(const CTransaction& tx) = 0;

    /* These are for display and wallet management purposes.  Not for use to decide
     * whether to spend a coin. */
    virtual bool IsMine(const CTransaction& tx) = 0;
    virtual bool IsMine(const CTransaction& tx, const CTxOut& txout, bool ignore_name_new = false) = 0;
    virtual int GetOurChainID() = 0;

    virtual int GetAuxPowStartBlock() = 0;
    virtual int GetFullRetargetStartBlock() = 0;

    virtual std::string GetAlertPubkey1()
    {
        return "04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284";
    }

    virtual std::string GetAlertPubkey2() { return GetAlertPubkey1(); }
};

extern CHooks* InitHook();
extern std::string GetDefaultDataDirSuffix();

#endif
