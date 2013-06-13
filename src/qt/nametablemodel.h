#ifndef NAMETABLEMODEL_H
#define NAMETABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

class NameTablePriv;
class CWallet;
class WalletModel; 

/**
   Qt model for "Manage Names" page.
 */
class NameTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit NameTableModel(CWallet *wallet, WalletModel *parent = 0);
    ~NameTableModel();

    enum ColumnIndex {
        Name = 0,
        Value = 1,
        ExpiresIn = 2
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    /*@}*/

private:
    WalletModel *walletModel;
    CWallet *wallet;
    QStringList columns;
    NameTablePriv *priv;
    int cachedNumBlocks;
    
    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

public slots:
    void updateEntry(const QString &name, const QString &value, int nHeight, int status, int *outNewRowIndex = NULL);
    void updateExpiration();
    void updateTransaction(const QString &hash, int status);

    friend class NameTablePriv;
};

struct NameTableEntry
{
    QString name;
    QString value;
    int nHeight;

    static const int NAME_NEW = -1;             // Dummy nHeight value for not-yet-created names
    static const int NAME_NON_EXISTING = -2;    // Dummy nHeight value for unitinialized entries
    static const int NAME_UNCONFIRMED = -3;     // Dummy nHeight value for unconfirmed name transactions

    bool HeightValid() { return nHeight >= 0; }
    static bool CompareHeight(int nOldHeight, int nNewHeight);    // Returns true if new height is better

    NameTableEntry() : nHeight(NAME_NON_EXISTING) {}
    NameTableEntry(const QString &name, const QString &value, int nHeight):
        name(name), value(value), nHeight(nHeight) {}
    NameTableEntry(const std::string &name, const std::string &value, int nHeight):
        name(QString::fromStdString(name)), value(QString::fromStdString(value)), nHeight(nHeight) {}
};

#endif // NAMETABLEMODEL_H
