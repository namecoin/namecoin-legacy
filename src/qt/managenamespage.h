#ifndef MANAGENAMESPAGE_H
#define MANAGENAMESPAGE_H

#include <QDialog>
#include <QSortFilterProxyModel>

namespace Ui {
    class ManageNamesPage;
}
class WalletModel;
class NameTableModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

class NameFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit NameFilterProxyModel(QObject *parent = 0);

    void setNameSearch(const QString &search);
    void setValueSearch(const QString &search);
    void setAddressSearch(const QString &search);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    QString nameSearch, valueSearch, addressSearch;
};

/** Page for managing names */
class ManageNamesPage : public QDialog
{
    Q_OBJECT

public:
    explicit ManageNamesPage(QWidget *parent = 0);
    ~ManageNamesPage();

    void setModel(WalletModel *walletModel);

private:
    Ui::ManageNamesPage *ui;
    NameTableModel *model;
    WalletModel *walletModel;
    NameFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    
public slots:
    void exportClicked();

    void changedNameFilter(const QString &filter);
    void changedValueFilter(const QString &filter);
    void changedAddressFilter(const QString &filter);

private slots:
    void on_submitNameButton_clicked();

    bool eventFilter(QObject *object, QEvent *event);
    void selectionChanged();

    /** Spawn contextual menu (right mouse menu) for name table entry */
    void contextualMenu(const QPoint &point);

    void onCopyNameAction();
    void onCopyValueAction();
    void onCopyAddressAction();
    void on_configureNameButton_clicked();
    void on_renewNameButton_clicked();
};

#endif // MANAGENAMESPAGE_H
