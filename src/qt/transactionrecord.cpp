#include "transactionrecord.h"

#include "../headers.h"
#include "../wallet.h"
#include "../base58.h"
#include "ui_interface.h"

/* Return positive answer if transaction should be shown in list.
 */
bool TransactionRecord::showTransaction(const CWalletTx &wtx)
{
    if (wtx.IsCoinBase())
    {
        // Ensures we show generated coins / mined transactions at depth 1
        if (!wtx.IsInMainChain())
        {
            return false;
        }
    }
    return true;
}

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TransactionRecord> TransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    QList<TransactionRecord> parts;
    int64 nTime = wtx.GetTxTime();
    int64 nCredit = wtx.GetCredit(true);
    int64 nDebit = wtx.GetDebit();
    int64 nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    std::map<std::string, std::string> mapValue = wtx.mapValue;

    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
        {
            if(wallet->IsMine(txout))
            {
                TransactionRecord sub(hash, nTime);
                std::string address;
                sub.idx = parts.size(); // sequence number
                sub.credit = txout.nValue;
                if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*wallet, address))
                {
                    // Received by Bitcoin Address
                    sub.type = TransactionRecord::RecvWithAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else
                {
                    // Received by IP connection (deprecated features), or a multisignature or other non-simple transaction
                    sub.type = TransactionRecord::RecvFromOther;
                    sub.address = mapValue["from"];
                }
                if (wtx.IsCoinBase())
                {
                    // Generated
                    sub.type = TransactionRecord::Generated;
                }

                parts.append(sub);
            }
        }
    }
    else
    {
        bool fAllFromMe = true;
        int64 nCarriedOverCoin = 0;
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
        {
            if (!wallet->IsMine(txin))
            {
                // Check whether transaction input is name_* operation - in this case consider it ours
                CTransaction txPrev;
                uint256 hashBlock = 0;
                CTxDestination address;
                if (GetTransaction(txin.prevout.hash, txPrev, hashBlock) &&
                        txin.prevout.n < txPrev.vout.size() &&
                        hooks->ExtractAddress(txPrev.vout[txin.prevout.n].scriptPubKey, address)
                   )
                {
                    // This is our name transaction
                    // Accumulate the coin carried from name_new, because it is not actually spent
                    nCarriedOverCoin += txPrev.vout[txin.prevout.n].nValue;
                }
                else
                {
                    fAllFromMe = false;
                    break;
                }
            }
        }

        bool fAllToMe = true;
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (!wallet->IsMine(txout))
            {
                fAllToMe = false;
                break;
            }

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            int64 nChange = wtx.GetChange();

            parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf, "",
                            -(nDebit - nChange), nCredit - nChange));
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            int64 nTxFee = nDebit - (wtx.GetValueOut() - nCarriedOverCoin);

            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                TransactionRecord sub(hash, nTime);
                sub.idx = parts.size();

                if(wallet->IsMine(txout))
                {
                    // Ignore parts sent to self, as this is usually the change
                    // from a transaction sent back to our own address.
                    continue;
                }

                int64 nValue = txout.nValue;

                std::string address;
                if (ExtractDestination(txout.scriptPubKey, address))
                {
                    // Sent to Bitcoin Address
                    sub.type = TransactionRecord::SendToAddress;
                    sub.address = CBitcoinAddress(address).ToString();
                }
                else if (hooks->ExtractAddress(txout.scriptPubKey, address))
                {
                    sub.type = TransactionRecord::NameOp;
                    sub.address = address;

                    // Add carried coin (from name_new)
                    if (nCarriedOverCoin > 0)
                    {
                        // Note: we subtract nCarriedOverCoin equally from all name operations,
                        // until it becomes zero. It may fail for complex transactions, which
                        // update multiple names simultaneously (standard client never creates such transactions).
                        if (nValue >= nCarriedOverCoin)
                        {
                            nValue -= nCarriedOverCoin;
                            nCarriedOverCoin = 0;
                        }
                        else
                        {
                            nCarriedOverCoin -= nValue;
                            nValue = 0;
                        }
                    }
                }
                else
                {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    sub.type = TransactionRecord::SendToOther;
                    sub.address = mapValue["to"];
                }

                // Carried over coin can be used to pay fee, if it the required
                // amount was reserved in OP_NAME_NEW
                if (nCarriedOverCoin > 0)
                {
                    if (nTxFee >= nCarriedOverCoin)
                    {
                        nTxFee -= nCarriedOverCoin;
                        nCarriedOverCoin = 0;
                    }
                    else
                    {
                        nCarriedOverCoin -= nTxFee;
                        nTxFee = 0;
                    }
                }

                /* Add fee to first output */
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }

                sub.debit = -nValue;

                parts.append(sub);
            }
        }
        else
        {
            //
            // Check for name transferring operation
            //
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            {
                std::string address;
                // We do not check, if coin address belongs to us, assuming that the wallet can only contain
                // transactions involving us
                if (hooks->ExtractAddress(txout.scriptPubKey, address))
                    parts.append(TransactionRecord(hash, nTime, TransactionRecord::NameOp, address, 0, 0));
            }

            //
            // Mixed debit transaction, can't break down payees
            //
            if (parts.empty() || nNet != 0)
                parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "", nNet, 0));
        }
    }

    return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx)
{
    // Determine transaction status

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    status.sortKey = strprintf("%010d-%01d-%010u-%03d",
        (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived,
        idx);
    status.confirmed = wtx.IsConfirmed();
    status.depth = wtx.GetDepthInMainChain();
    status.cur_num_blocks = nBestHeight;

    if (!wtx.IsFinalTx())
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
        {
            status.status = TransactionStatus::OpenUntilBlock;
            status.open_for = wtx.nLockTime - nBestHeight + 1;
        }
        else
        {
            status.status = TransactionStatus::OpenUntilDate;
            status.open_for = wtx.nLockTime;
        }
    }
    else
    {
        if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
        {
            status.status = TransactionStatus::Offline;
        }
        else if (status.depth < NumConfirmations)
        {
            status.status = TransactionStatus::Unconfirmed;
        }
        else
        {
            status.status = TransactionStatus::HaveConfirmations;
        }
    }

    // For generated transactions, determine maturity
    if(type == TransactionRecord::Generated)
    {
        int64 nCredit = wtx.GetCredit(true);
        if (nCredit == 0)
        {
            status.maturity = TransactionStatus::Immature;

            if (wtx.IsInMainChain())
            {
                status.matures_in = wtx.GetBlocksToMaturity();

                // Check if the block was requested by anyone
                if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                    status.maturity = TransactionStatus::MaturesWarning;
            }
            else
            {
                status.maturity = TransactionStatus::NotAccepted;
            }
        }
        else
        {
            status.maturity = TransactionStatus::Mature;
        }
    }
}

bool TransactionRecord::statusUpdateNeeded()
{
    return status.cur_num_blocks != nBestHeight;
}

std::string TransactionRecord::getTxID()
{
    return hash.ToString() + strprintf("-%03d", idx);
}

