#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "nametablemodel.h"
#include "transactiontablemodel.h"

#include "../headers.h"
#include "../wallet.h"
#include "../base58.h"
#include "../namecoin.h"
#include "ui_interface.h"

#include <QSet>
#include <QTimer>
#include <QDateTime>

#include <boost/xpressive/xpressive_dynamic.hpp>

std::map<std::vector<unsigned char>, PreparedNameFirstUpdate> mapMyNameFirstUpdate;
std::map<uint160, std::vector<unsigned char> > mapMyNameHashes;

QString
SendCoinsRecipient::getAddress (const WalletModel& model) const
{
  if (model.validateAddress (recipient))
    return recipient;

  QString address;
  if (!model.checkRecipientName (recipient, address))
    address = "";
  return address;
}

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    nameTableModel(0), transactionTableModel(0),
    cachedBalance(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    addressTableModel = new AddressTableModel(wallet, this);
    nameTableModel = new NameTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);

    fSyncedAtLeastOnce = false;    // For sending automatic name_firstupdate

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

qint64 WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    if(nBestHeight != cachedNumBlocks)
    {
        // Balance and number of transactions might have changed
        cachedNumBlocks = nBestHeight;
        checkBalanceChanged();

        if (!IsInitialBlockDownload())
        {
            if (!fSyncedAtLeastOnce)
            {
                // On client start it fails to broadcast (too few connections?), so we wait for full synchronization
                QDateTime lastBlockDate = QDateTime::fromTime_t(pindexBest->GetBlockTime());
                QDateTime currentDate = QDateTime::currentDateTime();
                int secs = lastBlockDate.secsTo(currentDate);
                if (secs < 90*60 && nBestHeight >= GetNumBlocksOfPeers())
                    fSyncedAtLeastOnce = true;
            }

            if (fSyncedAtLeastOnce)
                sendPendingNameFirstUpdates();
        }
    }
}

void WalletModel::checkBalanceChanged()
{
    qint64 newBalance = getBalance();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newImmatureBalance = getImmatureBalance();

    if(cachedBalance != newBalance || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance)
    {
        cachedBalance = newBalance;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        emit balanceChanged(newBalance, newUnconfirmedBalance, newImmatureBalance);
    }
}

void WalletModel::sendPendingNameFirstUpdates()
{
    CRITICAL_BLOCK(cs_main)
    {
        for (std::map<std::vector<unsigned char>, PreparedNameFirstUpdate>::iterator mi = mapMyNameFirstUpdate.begin();
                mi != mapMyNameFirstUpdate.end(); )
        {
            const std::vector<unsigned char> &vchName = mi->first;

            std::map<std::vector<unsigned char>, uint256>::const_iterator it1 = mapMyNames.find(vchName);
            if (it1 == mapMyNames.end())
            {
                printf("Automatic name_firstupdate failed - no tx id for name %s\n", stringFromVch(vchName).c_str());
                wallet->EraseNameFirstUpdate(vchName);
                mapMyNameFirstUpdate.erase(mi++);
                continue;
            }
            uint256 wtxInHash = it1->second;
            bool fSkip = false;
            CRITICAL_BLOCK(wallet->cs_wallet)
            {
                std::map<uint256, CWalletTx>::const_iterator it2 = wallet->mapWallet.find(wtxInHash);
                if (it2 == wallet->mapWallet.end())
                {
                    printf("Automatic name_firstupdate failed - no wallet transaction for name %s (hash %s)\n",
                           stringFromVch(vchName).c_str(),
                           wtxInHash.GetHex().c_str());
                    wallet->EraseNameFirstUpdate(vchName);
                    mapMyNameFirstUpdate.erase(mi++);
                    fSkip = true;
                }
                else if (it2->second.GetDepthInMainChain() < MIN_FIRSTUPDATE_DEPTH)
                {
                    mi++;
                    fSkip = true;
                }
            }
            if (fSkip)
                continue;
                
            printf("Sending automatic name_firstupdate for name %s\n", stringFromVch(vchName).c_str());

            CWalletTx wtx = mi->second.wtx;

            // Currently we reserve the key when preparing firstupdate transaction. If the user changes
            // name configuration before broadcasting the transaction, the key is forever left unused.
            CReserveKey dummyKey(NULL);
            
            if (!wallet->CommitTransaction(wtx, dummyKey))
            {
                printf("Automatic name_firstupdate failed. Name: %s, rand: %s, prevTx: %s, value: %s\n",
                       stringFromVch(vchName).c_str(),
                       HexStr(CBigNum(mi->second.rand).getvch()).c_str(),
                       wtxInHash.GetHex().c_str(),
                       stringFromVch(mi->second.vchData).c_str());
            }
            else
            {
                // Report the rand value, so the user has a chance to resubmit name_firstupdate manually (e.g. if the network forks)
                printf("Automatic name_firstupdate done. Name: %s, rand: %s, prevTx: %s, value: %s\n",
                       stringFromVch(vchName).c_str(),
                       HexStr(CBigNum(mi->second.rand).getvch()).c_str(),
                       wtxInHash.GetHex().c_str(),
                       stringFromVch(mi->second.vchData).c_str());
            }

            wallet->EraseNameFirstUpdate(vchName);
            mapMyNameFirstUpdate.erase(mi++);
        }
    }
}

// Equivalent of name_firstupdate that does not send the transaction (the transaction is kept for 12 blocks).
// This is needed because of wallet encryption (otherwise we could store just hash+rand+value and create transaction
// on-the-fly after 12 blocks).
// Must hold cs_main lock.
std::string WalletModel::nameFirstUpdateCreateTx(CWalletTx &wtx, const std::vector<unsigned char> &vchName, uint256 wtxInHash, uint64 rand, const std::vector<unsigned char> &vchValue, int64 *pnFeeRet /* = NULL*/)
{
    LOCK(wallet->cs_wallet);

    std::map<uint256, CWalletTx>::iterator it = wallet->mapWallet.find(wtxInHash);
    if (it == wallet->mapWallet.end())
        return _("previous transaction is not in the wallet");

    return nameFirstUpdateCreateTx(wtx, vchName, it->second, rand, vchValue, pnFeeRet);
}

std::string WalletModel::nameFirstUpdateCreateTx(CWalletTx &wtx, const std::vector<unsigned char> &vchName, CWalletTx &wtxIn, uint64 rand, const std::vector<unsigned char> &vchValue, int64 *pnFeeRet /* = NULL*/)
{
    if (pnFeeRet)
        *pnFeeRet = 0;

    wtx.nVersion = NAMECOIN_TX_VERSION;
    
    if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
    {
        error("name_firstupdate() : there are %d pending operations on that name, including %s",
                mapNamePending[vchName].size(),
                mapNamePending[vchName].begin()->GetHex().c_str());
        return _("there are pending operations on that name");
    }

    {
        CNameDB dbName("r");
        CTransaction tx;
        if (GetTxOfName(dbName, vchName, tx))
        {
            error("name_firstupdate() : this name is already active with tx %s",
                    tx.GetHash().GetHex().c_str());
            return _("this name is already active");
        }
    }

    std::vector<unsigned char> vchRand = CBigNum(rand).getvch();

    std::vector<unsigned char> vchPubKey = wallet->GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    CScript scriptPubKey;
    scriptPubKey << OP_NAME_FIRSTUPDATE << vchName << vchRand << vchValue << OP_2DROP << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;

    std::vector<unsigned char> vchHash;
    bool found = false;
    BOOST_FOREACH(CTxOut& out, wtxIn.vout)
    {
        std::vector<std::vector<unsigned char> > vvch;
        int op;
        if (DecodeNameScript(out.scriptPubKey, op, vvch)) {
            if (op != OP_NAME_NEW)
                return _("previous transaction wasn't a name_new");
            vchHash = vvch[0];
            found = true;
        }
    }

    if (!found)
        return _("previous tx on this name is not a name tx");

    std::vector<unsigned char> vchToHash(vchRand);
    vchToHash.insert(vchToHash.end(), vchName.begin(), vchName.end());
    uint160 hash =  Hash160(vchToHash);
    if (uint160(vchHash) != hash)
        return _("previous tx used a different random value");

    int64 nNetFee = GetNetworkFee(pindexBest->nHeight);
    // Round up to CENT
    nNetFee += CENT - 1;
    nNetFee = (nNetFee / CENT) * CENT;

    //return SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, nNetFee, wtxIn, wtx, false);

    int64 nValue = MIN_AMOUNT;

    int nTxOut = IndexOfNameOutput(wtxIn);

    // TODO: since firstupdate can be called multiple times from GUI and only the last one is used, we should reuse reservekey
    CReserveKey reservekey(wallet);

    int64 nFeeRequired;
    std::vector< std::pair<CScript, int64> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    if (nNetFee)
    {
        CScript scriptFee;
        scriptFee << OP_RETURN;
        vecSend.push_back(make_pair(scriptFee, nNetFee));
    }

    if (!CreateTransactionWithInputTx(vecSend, wtxIn, nTxOut, wtx, reservekey, nFeeRequired))
    {
        std::string strError;
        if (nValue + nFeeRequired > wallet->GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("nameFirstUpdateCreateTx() : %s", strError.c_str());
        return strError;
    }

    if (pnFeeRet)
        *pnFeeRet = nNetFee + nFeeRequired;

    // Note: currently we do not notify the user about the name_firstupdate fee:
    // - it can be confusing, since name_firstupdate can be re-configured many times
    // - canceling the fee will leave the configured name in inconsistent state: name_new without pending name_firstupdate may result in losing the hex value (rand)
    //if (!uiInterface.ThreadSafeAskFee(nFeeRequired))
    //    return "ABORTED";

    // Take key pair from key pool so it won't be used again
    reservekey.KeepKey();
    
    if (!wtx.CheckTransaction())
        return _("Error: CheckTransaction failed for transaction created by nameFirstUpdateCreateTx");

    return "";
}

void WalletModel::updateTransaction(const QString &hash, int status)
{
    if(transactionTableModel)
        transactionTableModel->updateTransaction(hash, status);

    if (nameTableModel)
        nameTableModel->updateTransaction(hash, status);

    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if(cachedNumTransactions != newNumTransactions)
    {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}

void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, status);
}

bool
WalletModel::validateAddress(const QString& address) const
{
    CBitcoinAddress addressParsed(address.toStdString());
    return addressParsed.IsValid();
}

bool
WalletModel::checkRecipientName (const QString& name, QString& address) const
{
  /* Check that the name contains a namespace and consists only of
     basic characters (a-z, 0-9 and dash).  This is to prevent
     tricking people into sending to other names than they intend, and
     to prevent squatting of the "no namespace" names.

     This check also distinguishes names from addresses, which do not
     have a namespace and contain upper case characters (most probably).  */

  using namespace boost::xpressive;
  static const char* regexStr = "^[a-z]+/[a-z0-9]+([-]?[a-z0-9])*$";
  static const sregex regex = sregex::compile (regexStr);
  smatch match;
  if (!regex_search (name.toStdString (), match, regex))
    return false;

  /* Query information about the name and try to find its address.  */

  const vchType vchName = vchFromString (name.toStdString ());
  CNameDB dbName("r");
  if (!dbName.ExistsName (vchName))
    return false;

  CTransaction tx;
  if (!GetTxOfName (dbName, vchName, tx))
    return false;

  std::string addr;
  if (!GetNameAddress (tx, addr))
    return false;

  address = QString::fromStdString (addr);
  return true;
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QList<SendCoinsRecipient> &recipients)
{
    qint64 total = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        const QString address = rcp.getAddress (*this);
        if (address == "")
          return InvalidAddress;
        setAddress.insert (address);

        if(rcp.amount <= 0)
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    if(total > getBalance())
    {
        return AmountExceedsBalance;
    }

    if((total + nTransactionFee) > getBalance())
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    {
        LOCK2(cs_main, wallet->cs_wallet);

        // Sendmany
        std::vector<std::pair<CScript, int64> > vecSend;
        foreach(const SendCoinsRecipient &rcp, recipients)
        {
            const std::string addr = rcp.getAddress (*this).toStdString ();
            const CBitcoinAddress parsedAddr(addr);

            CScript scriptPubKey;
            scriptPubKey.SetDestination (parsedAddr.Get ());

            vecSend.push_back(std::make_pair(scriptPubKey, rcp.amount));
        }

        CWalletTx wtx;
        CReserveKey keyChange(wallet);
        int64 nFeeRequired = 0;
        bool fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired);

        if(!fCreated)
        {
            if((total + nFeeRequired) > wallet->GetBalance())
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            return TransactionCreationFailed;
        }
        if(!uiInterface.ThreadSafeAskFee(nFeeRequired))
        {
            return Aborted;
        }
        if(!wallet->CommitTransaction(wtx, keyChange))
        {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to to the address book
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        const std::string strAddress = rcp.getAddress (*this).toStdString ();
        CTxDestination dest = CBitcoinAddress(strAddress).Get();
        std::string strLabel = rcp.label.toStdString();
        {
            LOCK(wallet->cs_wallet);

            std::map<CTxDestination, std::string>::iterator mi = wallet->mapAddressBook.find(dest);

            // Check if we have a new address or an updated label
            if (mi == wallet->mapAddressBook.end() || mi->second != strLabel)
            {
                wallet->SetAddressBookName(dest, strLabel);
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

bool WalletModel::nameAvailable(const QString &name)
{
    std::string strName = name.toStdString();
    std::vector<unsigned char> vchName(strName.begin(), strName.end());

    std::vector<CNameIndex> vtxPos;
    CNameDB dbName("r");
    if (!dbName.ReadName(vchName, vtxPos))
        return true;
   
    if (vtxPos.size() < 1)
        return true;

    CDiskTxPos txPos = vtxPos[vtxPos.size() - 1].txPos;
    CTransaction tx;
    if (!tx.ReadFromDisk(txPos))
        return true;     // This may indicate error, rather than name availability

    std::vector<unsigned char> vchValue;
    int nHeight;
    uint256 hash;
    if (txPos.IsNull() || !GetValueOfTxPos(txPos, vchValue, hash, nHeight))
        return true;

    // TODO: should we subtract MIN_FIRSTUPDATE_DEPTH blocks? I think name_new may be possible when the previous registration is just about to expire
    if(nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
        return true;    // Expired

    return false;
}

WalletModel::NameNewReturn WalletModel::nameNew(const QString &name)
{
    NameNewReturn ret;

    std::string strName = name.toStdString();
    ret.vchName = std::vector<unsigned char>(strName.begin(), strName.end());

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;

    uint64 rand = GetRand((uint64)-1);
    std::vector<unsigned char> vchRand = CBigNum(rand).getvch();
    std::vector<unsigned char> vchToHash(vchRand);
    vchToHash.insert(vchToHash.end(), ret.vchName.begin(), ret.vchName.end());
    uint160 hash = Hash160(vchToHash);

    std::vector<unsigned char> vchPubKey = wallet->GetKeyFromKeyPool();
    CScript scriptPubKeyOrig;
    scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    ret.address = QString::fromStdString(scriptPubKeyOrig.GetBitcoinAddress());
    CScript scriptPubKey;
    scriptPubKey << OP_NAME_NEW << hash << OP_2DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    {
        // Include additional fee to name_new, which will be re-used by name_firstupdate
        // In this way we can preconfigure name_firstupdate

        int64 nFirstUpdateFee = 0;
        int64 nPrevFirstUpdateFee;
        CReserveKey reservekey(wallet);
        
        PreparedNameFirstUpdate prep;
        prep.rand = rand;
        
        // 1st pass: compute fee for name_firstupdate
        // 2nd pass: try using that fee in name_new
        for (int pass = 1; pass <= 2; pass++)
        {
            nPrevFirstUpdateFee = nFirstUpdateFee;
            reservekey.ReturnKey();

            // Prepare name_new, but do not commit until we prepare name_firstupdate
            printf("name_new GUI: SendMoneyPrepare (pass %d)\n", pass);
            std::string strError = wallet->SendMoneyPrepare(scriptPubKey, MIN_AMOUNT + nFirstUpdateFee, wtx, reservekey, pass == 1);
            if (!strError.empty())
            {
                printf("name_new GUI error: %s\n", strError.c_str());
                ret.ok = false;
                ret.err_msg = QString::fromStdString(strError);
                return ret;
            }

            ret.hex = wtx.GetHash();
            ret.rand = rand;
            ret.hash = hash;

            // Prepare name_firstupdate (with empty value)
            // FIXME: AddSupportingTransactions will fail and write msg to the log
            // Though we manually call AddSupportingTransactions (near the end of this function)
            printf("name_new GUI: nameFirstUpdateCreateTx (pass %d)\n", pass);
            strError = nameFirstUpdateCreateTx(prep.wtx, ret.vchName, wtx, rand, prep.vchData, &nFirstUpdateFee);
            if (!strError.empty())
            {
                printf("name_new GUI error: %s\n", strError.c_str());
                ret.ok = false;
                ret.err_msg = QString::fromStdString(strError);
                return ret;
            }
            if (nPrevFirstUpdateFee == nFirstUpdateFee)
                break;
        }
        if (nPrevFirstUpdateFee != nFirstUpdateFee)
            printf("name_new GUI warning: cannot prepare fee for automatic name_firstupdate - fee changed from %s to %s\n", FormatMoney(nPrevFirstUpdateFee).c_str(), FormatMoney(nFirstUpdateFee).c_str());
            
        printf("Automatic name_firstupdate created for name %s (initial, with empty value), created tx: %s:\n%s", qPrintable(name), prep.wtx.GetHash().GetHex().c_str(), prep.wtx.ToString().c_str());

        // name_firstupdate prepared, let's commit name_new
        if (!wallet->CommitTransaction(wtx, reservekey))
        {
            ret.ok = false;
            ret.err_msg = tr("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
            return ret;
        }

        // name_new committed successfully, from this point we must return ok
        ret.ok = true;

        mapMyNames[ret.vchName] = ret.hex;
        mapMyNameHashes[ret.hash] = ret.vchName;
        mapMyNameFirstUpdate[ret.vchName] = prep;

        {
            CTxDB txdb("r"); 
            CRITICAL_BLOCK(wallet->cs_wallet)
            {
                // Fill vtxPrev by copying from previous transactions vtxPrev
                prep.wtx.AddSupportingTransactions(txdb);
                wallet->WriteNameFirstUpdate(ret.vchName, ret.hex, rand, prep.vchData, prep.wtx);
            }
        }
    }
    return ret;
}

QString WalletModel::nameFirstUpdatePrepare(const QString &name, const QString &data)
{
    std::string strName = name.toStdString();
    std::vector<unsigned char> vchName(strName.begin(), strName.end());

    std::string strData = data.toStdString();
    std::vector<unsigned char> vchValue(strData.begin(), strData.end());

    CRITICAL_BLOCK(cs_main)
    {
        std::map<std::vector<unsigned char>, uint256>::const_iterator it1 = mapMyNames.find(vchName);
        if (it1 == mapMyNames.end())
            return tr("Cannot find stored tx hash for name");

        std::map<std::vector<unsigned char>, PreparedNameFirstUpdate>::iterator it2 = mapMyNameFirstUpdate.find(vchName);
        if (it2 == mapMyNameFirstUpdate.end())
            return tr("Cannot find stored rand value for name");

        uint256 wtxInHash = it1->second;
        uint64 rand = it2->second.rand;

        CWalletTx wtx;
        std::string err_msg = nameFirstUpdateCreateTx(wtx, vchName, wtxInHash, rand, vchValue);
        if (err_msg != "")
            return QString::fromStdString(err_msg);
        it2->second.vchData = vchValue;
        it2->second.wtx = wtx;

        CRITICAL_BLOCK(wallet->cs_wallet)
            wallet->WriteNameFirstUpdate(vchName, wtxInHash, rand, vchValue, wtx);
        printf("Automatic name_firstupdate created for name %s, created tx: %s:\n%s", qPrintable(name), wtx.GetHash().GetHex().c_str(), wtx.ToString().c_str());
    }

    return "";
}

QString WalletModel::nameUpdate(const QString &name, const QString &data, const QString &transferToAddress)
{
    std::string strName = name.toStdString();
    std::vector<unsigned char> vchName(strName.begin(), strName.end());
    
    std::string strData = data.toStdString();
    std::vector<unsigned char> vchValue(strData.begin(), strData.end());

    CWalletTx wtx;
    wtx.nVersion = NAMECOIN_TX_VERSION;
    CScript scriptPubKeyOrig;
    
    if (transferToAddress != "")
    {
        std::string strAddress = transferToAddress.toStdString();
        uint160 hash160;
        bool isValid = AddressToHash160(strAddress, hash160);
        if (!isValid)
            return tr("Invalid Namecoin address");
        scriptPubKeyOrig.SetBitcoinAddress(strAddress);
    }
    else
    {
        std::vector<unsigned char> vchPubKey = wallet->GetKeyFromKeyPool();
        scriptPubKeyOrig.SetBitcoinAddress(vchPubKey);
    }

    CScript scriptPubKey;
    scriptPubKey << OP_NAME_UPDATE << vchName << vchValue << OP_2DROP << OP_DROP;
    scriptPubKey += scriptPubKeyOrig;

    CRITICAL_BLOCK(cs_main)
    CRITICAL_BLOCK(wallet->cs_mapWallet)
    {
        if (mapNamePending.count(vchName) && mapNamePending[vchName].size())
        {
            error("name_update() : there are %d pending operations on that name, including %s",
                    mapNamePending[vchName].size(),
                    mapNamePending[vchName].begin()->GetHex().c_str());
            return tr("There are pending operations on that name");
        }

        CNameDB dbName("r");
        CTransaction tx;
        if (!GetTxOfName(dbName, vchName, tx))
            return tr("Could not find a coin with this name");

        uint256 wtxInHash = tx.GetHash();

        if (!wallet->mapWallet.count(wtxInHash))
        {
            error("name_update() : this coin is not in your wallet %s",
                    wtxInHash.GetHex().c_str());
            return tr("This coin is not in your wallet");
        }

        CWalletTx& wtxIn = wallet->mapWallet[wtxInHash];
        return QString::fromStdString(SendMoneyWithInputTx(scriptPubKey, MIN_AMOUNT, 0, wtxIn, wtx, true));
    }
}

QString
WalletModel::nameRenew (const QString& name)
{
  const vchType vchName = vchFromString (name.toStdString ());

  vchType vchValue;
  CRITICAL_BLOCK(cs_main)
    {
      CNameDB dbName("r");
      std::vector<CNameIndex> vNameIndex;
      if (!dbName.ReadName (vchName, vNameIndex))
        return tr ("Failed to read from the name DB");
      if (vNameIndex.empty ())
        return tr ("No result returned");

      vchValue = vNameIndex.back ().vValue;
    }

  const std::string strValue = stringFromVch (vchValue);
  return nameUpdate (name, QString::fromStdString (strValue), "");
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

NameTableModel *WalletModel::getNameTableModel()
{
    return nameTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CKeyStore *wallet)
{
    OutputDebugStringF("NotifyKeyStoreStatusChanged\n");
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)
{
    OutputDebugStringF("NotifyAddressBookChanged %s %s isMine=%i status=%i\n", CBitcoinAddress(address).ToString().c_str(), label.c_str(), isMine, status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(CBitcoinAddress(address).ToString())),
                              Q_ARG(QString, QString::fromStdString(label)),
                              Q_ARG(bool, isMine),
                              Q_ARG(int, status));
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    if (!hash)
        return;     // Ignore coinbase transactions
    OutputDebugStringF("NotifyTransactionChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                              Q_ARG(int, status));
}

void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}
