#ifndef MANAGENAMESPAGE_H
#define MANAGENAMESPAGE_H

#include <QDialog>

namespace Ui {
    class ManageNamesPage;
}
class WalletModel;
class NameTableModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
QT_END_NAMESPACE

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
    QSortFilterProxyModel *proxyModel;
    QMenu *contextMenu;
    
public slots:
    void exportClicked();

private slots:
    void on_submitNameButton_clicked();

    bool eventFilter(QObject *object, QEvent *event);
    void selectionChanged();

    /** Spawn contextual menu (right mouse menu) for name table entry */
    void contextualMenu(const QPoint &point);

    void onCopyNameAction();
    void onCopyValueAction();
    void on_configureNameButton_clicked();
};

#endif // MANAGENAMESPAGE_H
