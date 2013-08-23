#include "nametablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"
#include "guiconstants.h"

#include "../headers.h"
#include "../namecoin.h"
#include "ui_interface.h"

#include <QTimer>

extern std::map<std::vector<unsigned char>, PreparedNameFirstUpdate> mapMyNameFirstUpdate;

// ExpiresIn column is right-aligned as it contains numbers
static int column_alignments[] = {
        Qt::AlignLeft|Qt::AlignVCenter,     // Name
        Qt::AlignLeft|Qt::AlignVCenter,     // Value
        Qt::AlignLeft|Qt::AlignVCenter,     // Address
        Qt::AlignRight|Qt::AlignVCenter     // Expires in
    };

struct NameTableEntryLessThan
{
    bool operator()(const NameTableEntry &a, const NameTableEntry &b) const
    {
        return a.name < b.name;
    }
    bool operator()(const NameTableEntry &a, const QString &b) const
    {
        return a.name < b;
    }
    bool operator()(const QString &a, const NameTableEntry &b) const
    {
        return a < b.name;
    }
};

// Returns true if new height is better
/*static*/ bool NameTableEntry::CompareHeight(int nOldHeight, int nNewHeight)
{
    if (nOldHeight == NAME_NON_EXISTING)
        return true;

    // We use optimistic way, assuming that unconfirmed transaction will eventually become confirmed,
    // so we update the name in the table immediately. Ideally we need a separate way of displaying
    // unconfirmed names (e.g. grayed out)
    if (nNewHeight == NAME_UNCONFIRMED)
        return true;

    // Here we rely on the fact that dummy height values are always negative
    return nNewHeight > nOldHeight;
}

// Private implementation
class NameTablePriv
{
public:
    CWallet *wallet;
    QList<NameTableEntry> cachedNameTable;
    NameTableModel *parent;

    NameTablePriv(CWallet *wallet, NameTableModel *parent):
        wallet(wallet), parent(parent) {}

    void refreshNameTable()
    {
        cachedNameTable.clear();

        std::map< std::vector<unsigned char>, NameTableEntry > vNamesO;

        CRITICAL_BLOCK(wallet->cs_mapWallet)
        {
            CTxIndex txindex;
            uint256 hash;
            CTxDB txdb("r");
            CTransaction tx;

            std::vector<unsigned char> vchName;
            std::vector<unsigned char> vchValue;
            int nHeight;

            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, wallet->mapWallet)
            {
                hash = item.second.GetHash();
                bool fConfirmed;
                bool fTransferred = false;
                // TODO: Maybe CMerkleTx::GetDepthInMainChain() would be faster?
                if (!txdb.ReadDiskTx(hash, tx, txindex))
                {
                    tx = item.second;
                    fConfirmed = false;
                }
                else
                    fConfirmed = true;

                if (tx.nVersion != NAMECOIN_TX_VERSION)
                    continue;

                // name
                if (!GetNameOfTx(tx, vchName))
                    continue;

                // value
                if (!GetValueOfNameTx(tx, vchValue))
                    continue;

                if (!hooks->IsMine(wallet->mapWallet[tx.GetHash()]))
                    fTransferred = true;
                    
                // height
                if (fConfirmed)
                {
                    nHeight = GetTxPosHeight(txindex.pos);
                    if (nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
                        continue;  // Expired
                }
                else
                    nHeight = NameTableEntry::NAME_UNCONFIRMED;

                // get last active name only
                std::map< std::vector<unsigned char>, NameTableEntry >::iterator mi = vNamesO.find(vchName);
                if (mi != vNamesO.end() && !NameTableEntry::CompareHeight(mi->second.nHeight, nHeight))
                    continue;

                std::string strAddress = "";
                GetNameAddress(tx, strAddress);

                vNamesO[vchName] = NameTableEntry(stringFromVch(vchName), stringFromVch(vchValue), strAddress, nHeight, fTransferred);
            }
        }        

        // Add existing names
        BOOST_FOREACH(const PAIRTYPE(std::vector<unsigned char>, NameTableEntry)& item, vNamesO)
            if (!item.second.transferred)
                cachedNameTable.append(item.second);

        // Add pending names (name_new)
        BOOST_FOREACH(const PAIRTYPE(std::vector<unsigned char>, PreparedNameFirstUpdate)& item, mapMyNameFirstUpdate)
        {
            std::string strAddress = "";
            GetNameAddress(item.second.wtx, strAddress);
            cachedNameTable.append(NameTableEntry(stringFromVch(item.first), stringFromVch(item.second.vchData), strAddress, NameTableEntry::NAME_NEW));
        }

        // qLowerBound() and qUpperBound() require our cachedNameTable list to be sorted in asc order
        qSort(cachedNameTable.begin(), cachedNameTable.end(), NameTableEntryLessThan());
    }

    void refreshName(const std::vector<unsigned char> &inName)
    {
        LOCK(cs_main);

        NameTableEntry nameObj(stringFromVch(inName), std::string(), std::string(), NameTableEntry::NAME_NON_EXISTING);

        CRITICAL_BLOCK(wallet->cs_mapWallet)
        {
            CTxIndex txindex;
            uint256 hash;
            CTxDB txdb("r");
            CTransaction tx;

            std::vector<unsigned char> vchName;
            std::vector<unsigned char> vchValue;
            int nHeight;

            BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, wallet->mapWallet)
            {
                hash = item.second.GetHash();
                bool fConfirmed;
                bool fTransferred = false;

                if (!txdb.ReadDiskTx(hash, tx, txindex))
                {
                    tx = item.second;
                    fConfirmed = false;
                }
                else
                    fConfirmed = true;

                if (tx.nVersion != NAMECOIN_TX_VERSION)
                    continue;

                // name
                if (!GetNameOfTx(tx, vchName) || vchName != inName)
                    continue;

                // value
                if (!GetValueOfNameTx(tx, vchValue))
                {
                    printf("refreshName(\"%s\"): skipping tx %s (GetValueOfNameTx returned false)\n", qPrintable(nameObj.name), hash.GetHex().c_str());
                    continue;
                }

                if (!hooks->IsMine(wallet->mapWallet[tx.GetHash()]))
                {
                    printf("refreshName(\"%s\"): tx %s - transferred\n", qPrintable(nameObj.name), hash.GetHex().c_str());
                    fTransferred = true;
                }

                // height
                if (fConfirmed)
                {
                    nHeight = GetTxPosHeight(txindex.pos);
                    if (nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
                    {
                        printf("refreshName(\"%s\"): tx %s, nHeight = %d - expired\n", qPrintable(nameObj.name), hash.GetHex().c_str(), nHeight);
                        continue;  // Expired
                    }
                    else
                    {
                        printf("refreshName(\"%s\"): tx %s, nHeight = %d\n", qPrintable(nameObj.name), hash.GetHex().c_str(), nHeight);
                    }
                }
                else
                {
                    printf("refreshName(\"%s\"): tx %s - unconfirmed\n", qPrintable(nameObj.name), hash.GetHex().c_str());
                    nHeight = NameTableEntry::NAME_UNCONFIRMED;
                }

                // get last active name only
                if (!NameTableEntry::CompareHeight(nameObj.nHeight, nHeight))
                {
                    printf("refreshName(\"%s\"): tx %s - skipped (more recent transaction exists)\n", qPrintable(nameObj.name), hash.GetHex().c_str());
                    continue;
                }
                
                std::string strAddress = "";
                GetNameAddress(tx, strAddress);

                nameObj.value = QString::fromStdString(stringFromVch(vchValue));
                nameObj.address = QString::fromStdString(strAddress);
                nameObj.nHeight = nHeight;
                nameObj.transferred = fTransferred;

                printf("refreshName(\"%s\") found tx %s, nHeight=%d, value: %s\n", qPrintable(nameObj.name), hash.GetHex().c_str(), nameObj.nHeight, qPrintable(nameObj.value));
            }
        }

        // Transferred name is not ours anymore - remove it from the table
        if (nameObj.transferred)
            nameObj.nHeight = NameTableEntry::NAME_NON_EXISTING;

        // Find name in model
        QList<NameTableEntry>::iterator lower = qLowerBound(
            cachedNameTable.begin(), cachedNameTable.end(), nameObj.name, NameTableEntryLessThan());
        QList<NameTableEntry>::iterator upper = qUpperBound(
            cachedNameTable.begin(), cachedNameTable.end(), nameObj.name, NameTableEntryLessThan());
        bool inModel = (lower != upper);

        if (inModel)
        {
            // In model - update or delete

            if (nameObj.nHeight != NameTableEntry::NAME_NON_EXISTING)
            {
                printf("refreshName result : %s - refreshed in the table\n", qPrintable(nameObj.name));
                updateEntry(nameObj, CT_UPDATED);
            }
            else
            {
                printf("refreshName result : %s - deleted from the table\n", qPrintable(nameObj.name));
                updateEntry(nameObj, CT_DELETED);
            }
        }
        else
        {
            // Not in model - add or do nothing

            if (nameObj.nHeight != NameTableEntry::NAME_NON_EXISTING)
            {
                printf("refreshName result : %s - added to the table\n", qPrintable(nameObj.name));
                updateEntry(nameObj, CT_NEW);
            }
            else
            {
                printf("refreshName result : %s - ignored (not in the table)\n", qPrintable(nameObj.name));
            }
        }
    }

    void updateEntry(const NameTableEntry &nameObj, int status, int *outNewRowIndex = NULL)
    {
        updateEntry(nameObj.name, nameObj.value, nameObj.address, nameObj.nHeight, status, outNewRowIndex);
    }

    void updateEntry(const QString &name, const QString &value, const QString &address, int nHeight, int status, int *outNewRowIndex = NULL)
    {
        // Find name in model
        QList<NameTableEntry>::iterator lower = qLowerBound(
            cachedNameTable.begin(), cachedNameTable.end(), name, NameTableEntryLessThan());
        QList<NameTableEntry>::iterator upper = qUpperBound(
            cachedNameTable.begin(), cachedNameTable.end(), name, NameTableEntryLessThan());
        int lowerIndex = (lower - cachedNameTable.begin());
        int upperIndex = (upper - cachedNameTable.begin());
        bool inModel = (lower != upper);

        switch(status)
        {
        case CT_NEW:
            if (inModel)
            {
                if (outNewRowIndex)
                {
                    *outNewRowIndex = parent->index(lowerIndex, 0).row();
                    // HACK: ManageNamesPage uses this to ensure updating and get selected row,
                    // so we do not write warning into the log in this case
                }
                else
                    OutputDebugStringF("Warning: NameTablePriv::updateEntry: Got CT_NOW, but entry is already in model\n");
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedNameTable.insert(lowerIndex, NameTableEntry(name, value, address, nHeight));
            parent->endInsertRows();
            if (outNewRowIndex)
                *outNewRowIndex = parent->index(lowerIndex, 0).row();
            break;
        case CT_UPDATED:
            if (!inModel)
            {
                OutputDebugStringF("Warning: NameTablePriv::updateEntry: Got CT_UPDATED, but entry is not in model\n");
                break;
            }
            lower->name = name;
            lower->value = value;
            lower->address = address;
            lower->nHeight = nHeight;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if (!inModel)
            {
                OutputDebugStringF("Warning: NameTablePriv::updateEntry: Got CT_DELETED, but entry is not in model\n");
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedNameTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedNameTable.size();
    }

    NameTableEntry *index(int idx)
    {
        if (idx >= 0 && idx < cachedNameTable.size())
        {
            return &cachedNameTable[idx];
        }
        else
        {
            return NULL;
        }
    }
};

NameTableModel::NameTableModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent), walletModel(parent), wallet(wallet), priv(0), cachedNumBlocks(0)
{
    columns << tr("Name") << tr("Value") << tr("Address") << tr("Expires in");
    priv = new NameTablePriv(wallet, this);
    priv->refreshNameTable();
    
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateExpiration()));
    timer->start(MODEL_UPDATE_DELAY);
}

NameTableModel::~NameTableModel()
{
    delete priv;
}

void NameTableModel::updateExpiration()
{
    if (nBestHeight != cachedNumBlocks)
    {
        LOCK(cs_main);

        cachedNumBlocks = nBestHeight;
        // Blocks came in since last poll.
        // Delete expired names
        for (int i = 0, n = priv->size(); i < n; i++)
        {
            NameTableEntry *item = priv->index(i);
            if (!item->HeightValid())
                continue;       // Currently, unconfirmed names do not expire in the table
            int nHeight = item->nHeight;
            if (nHeight + GetDisplayExpirationDepth(nHeight) - pindexBest->nHeight <= 0)
            {
                priv->updateEntry(item->name, item->value, item->address, item->nHeight, CT_DELETED);
                // Data array changed - restart scan
                n = priv->size();
                i = -1;
            }            
        }
        // Invalidate expiration counter for all rows.
        // Qt is smart enough to only actually request the data for the
        // visible rows.
        emit dataChanged(index(0, ExpiresIn), index(priv->size()-1, ExpiresIn));
    }
}

void NameTableModel::updateTransaction(const QString &hash, int status)
{
    uint256 hash256;
    hash256.SetHex(hash.toStdString());

    CTransaction tx;

    {
        LOCK(wallet->cs_wallet);

        // Find transaction in wallet
        std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(hash256);
        if (mi == wallet->mapWallet.end())
            return;    // Not our transaction
        tx = mi->second;
    }

    std::vector<unsigned char> vchName;
    if (!GetNameOfTx(tx, vchName))
        return;   // Non-name transaction

    printf("updateTransaction (%s, status=%d) calls refreshName(\"%s\")\n", qPrintable(hash), status, stringFromVch(vchName).c_str());

    priv->refreshName(vchName);
}

int NameTableModel::rowCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int NameTableModel::columnCount(const QModelIndex &parent /*= QModelIndex()*/) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant NameTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    NameTableEntry *rec = static_cast<NameTableEntry*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case Name:
            return rec->name;
        case Value:
            return rec->value;
        case Address:
            return rec->address;
        case ExpiresIn:
            if (!rec->HeightValid())
            {
                if (rec->nHeight == NameTableEntry::NAME_NEW)
                    return QString("pending (new)");
                return QString("pending (update)");
            }
            else
                return rec->nHeight + GetDisplayExpirationDepth(rec->nHeight) - pindexBest->nHeight;
        }
    }
    else if (role == Qt::TextAlignmentRole)
        return column_alignments[index.column()];
    else if (role == Qt::FontRole)
    {
        QFont font;
        if (index.column() == Address)
            font = GUIUtil::bitcoinAddressFont();
        return font;
    }

    return QVariant();
}

QVariant NameTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal)
    {
        if (role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        }
        else if (role == Qt::ToolTipRole)
        {
            switch (section)
            {
            case Name:
                return tr("Name registered using Namecoin.");
            case Value:
                return tr("Data associated with the name.");
            case Address:
                return tr("Namecoin address to which the name is registered.");
            case ExpiresIn:
                return tr("Number of blocks, after which the name will expire. Update name to renew it.\nEmpty cell means pending (awaiting automatic name_firstupdate or awaiting network confirmation).");
            }
        } 
    }
    return QVariant();
}

Qt::ItemFlags NameTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    //NameTableEntry *rec = static_cast<NameTableEntry*>(index.internalPointer());

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

QModelIndex NameTableModel::index(int row, int column, const QModelIndex &parent /* = QModelIndex()*/) const
{
    Q_UNUSED(parent);
    NameTableEntry *data = priv->index(row);
    if (data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void NameTableModel::updateEntry(const QString &name, const QString &value, const QString &address, int nHeight, int status, int *outNewRowIndex /*= NULL*/)
{
    priv->updateEntry(name, value, address, nHeight, status, outNewRowIndex);
}

void NameTableModel::emitDataChanged(int idx)
{
    emit dataChanged(index(idx, 0), index(idx, columns.length()-1));
}
