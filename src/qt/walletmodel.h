#ifndef WALLETMODEL_H
#define WALLETMODEL_H

#include <QObject>

#include "../allocators.h" /* for SecureString */
#include "../base58.h"     /* for uint256, uint64 */

class OptionsModel;
class AddressTableModel;
class NameTableModel;
class TransactionTableModel;
class WalletModel;
class CWallet;
class CWalletTx;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient
{
public:
    QString recipient;
    QString label;
    qint64 amount;

    /* Get the recipient address.  This translates a recipient name if
       applicable.  Returns "" if the value is not a valid address and also
       not resolvable as name.  */
    QString getAddress (const WalletModel& model) const;
};

/** Interface to Bitcoin wallet from Qt view code. */
class WalletModel : public QObject
{
    Q_OBJECT

public:
    explicit WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent = 0);
    ~WalletModel();

    enum StatusCode // Returned by sendCoins
    {
        OK,
        InvalidAmount,
        InvalidAddress,
        AmountExceedsBalance,
        AmountWithFeeExceedsBalance,
        DuplicateAddress,
        TransactionCreationFailed, // Error returned when wallet is still locked
        TransactionCommitFailed,
        Aborted
    };

    enum EncryptionStatus
    {
        Unencrypted,  // !wallet->IsCrypted()
        Locked,       // wallet->IsCrypted() && wallet->IsLocked()
        Unlocked      // wallet->IsCrypted() && !wallet->IsLocked()
    };

    OptionsModel *getOptionsModel();
    AddressTableModel *getAddressTableModel();
    NameTableModel *getNameTableModel();
    TransactionTableModel *getTransactionTableModel();

    qint64 getBalance() const;
    qint64 getUnconfirmedBalance() const;
    qint64 getImmatureBalance() const;
    int getNumTransactions() const;
    EncryptionStatus getEncryptionStatus() const;

    // Check address for validity
    bool validateAddress (const QString& address) const;

    /* Check if a given name can be used as a "sendtoname" recipient and
       if yes, set the address to the use that should be used.  */
    bool checkRecipientName (const QString& name, QString& address) const;

    // Return status record for SendCoins, contains error id + information
    struct SendCoinsReturn
    {
        SendCoinsReturn(StatusCode status,
                         qint64 fee=0,
                         QString hex=QString()):
            status(status), fee(fee), hex(hex) {}
        StatusCode status;
        qint64 fee; // is used in case status is "AmountWithFeeExceedsBalance"
        QString hex; // is filled with the transaction hash if status is "OK"
    };

    // Send coins to a list of recipients
    SendCoinsReturn sendCoins(const QList<SendCoinsRecipient> &recipients);

    bool nameAvailable(const QString &name);

    struct NameNewReturn
    {
         bool ok;
         QString err_msg;
         QString address;
         std::vector<unsigned char> vchName;
         uint256 hex;   // Transaction hash in hex
         uint64 rand;   // Secret number in hex
         uint160 hash;  // Hash of rand+name
    };

    // Register new name
    // Requires unlocked wallet; can throw exception instead of returning error
    NameNewReturn nameNew(const QString &name);
    
    // Create pending name update
    // Requires unlocked wallet; can throw exception instead of returning error
    QString nameFirstUpdatePrepare(const QString &name, const QString &data);

    // Send pending name updates, if they are 12 blocks old
    void sendPendingNameFirstUpdates();
    
    // Update name
    // Requires unlocked wallet; can throw exception instead of returning error
    QString nameUpdate(const QString &name, const QString &data, const QString &transferToAddress);

    /* Renew a name:  Calls nameUpdate internally, but uses the
       name's current value.  */
    QString nameRenew (const QString& name);

    // Wallet encryption
    bool setWalletEncrypted(bool encrypted, const SecureString &passphrase);
    // Passphrase only needed when unlocking
    bool setWalletLocked(bool locked, const SecureString &passPhrase=SecureString());
    bool changePassphrase(const SecureString &oldPass, const SecureString &newPass);
    // Wallet backup
    bool backupWallet(const QString &filename);

    // RAI object for unlocking wallet, returned by requestUnlock()
    class UnlockContext
    {
    public:
        UnlockContext(WalletModel *wallet, bool valid, bool relock);
        ~UnlockContext();

        bool isValid() const { return valid; }

        // Copy operator and constructor transfer the context
        UnlockContext(const UnlockContext& obj) { CopyFrom(obj); }
        UnlockContext& operator=(const UnlockContext& rhs) { CopyFrom(rhs); return *this; }
    private:
        WalletModel *wallet;
        bool valid;
        mutable bool relock; // mutable, as it can be set to false by copying

        void CopyFrom(const UnlockContext& rhs);
    };

    UnlockContext requestUnlock();

private:
    CWallet *wallet;

    // Wallet has an options model for wallet-specific options
    // (transaction fee, for example)
    OptionsModel *optionsModel;

    AddressTableModel *addressTableModel;
    NameTableModel *nameTableModel;
    TransactionTableModel *transactionTableModel;

    // Cache some values to be able to detect changes
    qint64 cachedBalance;
    qint64 cachedUnconfirmedBalance;
    qint64 cachedImmatureBalance;
    qint64 cachedNumTransactions;
    EncryptionStatus cachedEncryptionStatus;
    int cachedNumBlocks;

    QTimer *pollTimer;
    bool fSyncedAtLeastOnce;    // For sending automatic name_firstupdate

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void checkBalanceChanged();

    std::string nameFirstUpdateCreateTx(CWalletTx &wtx, const std::vector<unsigned char> &vchName, uint256 wtxInHash, uint64 rand, const std::vector<unsigned char> &vchValue, int64 *pnFeeRet = NULL);
    std::string nameFirstUpdateCreateTx(CWalletTx &wtx, const std::vector<unsigned char> &vchName, CWalletTx &wtxIn, uint64 rand, const std::vector<unsigned char> &vchValue, int64 *pnFeeRet = NULL);

signals:
    // Signal that balance in wallet changed
    void balanceChanged(qint64 balance, qint64 unconfirmedBalance, qint64 immatureBalance);

    // Number of transactions in wallet changed
    void numTransactionsChanged(int count);

    // Encryption status of wallet changed
    void encryptionStatusChanged(int status);

    // Signal emitted when wallet needs to be unlocked
    // It is valid behaviour for listeners to keep the wallet locked after this signal;
    // this means that the unlocking failed or was cancelled.
    void requireUnlock();

    // Asynchronous message notification
    void message(const QString &title, const QString &message, unsigned int style);

public slots:
    /* Wallet status might have changed */
    void updateStatus();
    /* New transaction, or transaction changed status */
    void updateTransaction(const QString &hash, int status);
    /* New, updated or removed address book entry */
    void updateAddressBook(const QString &address, const QString &label, bool isMine, int status);
    /* Current, immature or unconfirmed balance might have changed - emit 'balanceChanged' if so */
    void pollBalanceChanged();
};

#endif // WALLETMODEL_H
