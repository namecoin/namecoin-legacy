#include "managenamespage.h"
#include "ui_managenamespage.h"

#include "walletmodel.h"
#include "nametablemodel.h"
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "../base58.h"
#include "../main.h"
#include "../hook.h"
#include "../wallet.h"
#include "../namecoin.h"
#include "ui_interface.h"
#include "configurenamedialog.h"

#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QMenu>

extern std::map<std::vector<unsigned char>, PreparedNameFirstUpdate> mapMyNameFirstUpdate;

ManageNamesPage::ManageNamesPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageNamesPage),
    model(0),
    walletModel(0),
    proxyModel(0)
{
    ui->setupUi(this);

    // Context menu actions
    QAction *copyNameAction = new QAction(tr("Copy &Name"), this);
    QAction *copyValueAction = new QAction(tr("Copy &Value"), this);
    QAction *configureNameAction = new QAction(tr("&Configure Name..."), this);
    
    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyNameAction);
    contextMenu->addAction(copyValueAction);
    contextMenu->addAction(configureNameAction);
    
    // Connect signals for context menu actions
    connect(copyNameAction, SIGNAL(triggered()), this, SLOT(onCopyNameAction()));
    connect(copyValueAction, SIGNAL(triggered()), this, SLOT(onCopyValueAction()));
    connect(configureNameAction, SIGNAL(triggered()), this, SLOT(on_configureNameButton_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_configureNameButton_clicked()));
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Catch focus changes to make the appropriate button the default one (Submit or Configure)
    ui->registerName->installEventFilter(this);
    ui->tableView->installEventFilter(this);

    ui->registerName->setMaxLength(MAX_NAME_LENGTH);
}

ManageNamesPage::~ManageNamesPage()
{
    delete ui;
}

void ManageNamesPage::setModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    model = walletModel->getNameTableModel();

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    ui->tableView->horizontalHeader()->setHighlightSections(false);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(
            NameTableModel::Name, 320);
    ui->tableView->horizontalHeader()->setResizeMode(
            NameTableModel::Value, QHeaderView::Stretch);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    selectionChanged();
}

void ManageNamesPage::on_submitNameButton_clicked()
{
    if (!walletModel)
        return;

    QString name = ui->registerName->text();

    if (!walletModel->nameAvailable(name))
    {
        QMessageBox::warning(this, tr("Name registration"), tr("Name not available"));
        ui->registerName->setFocus();
        return;
    }

    QString msg;
    if (name.startsWith("d/"))
        msg = tr("Are you sure you want to register domain name %1, which corresponds to domain %2?").arg(name).arg(name.mid(2) + ".bit");
    else
        msg = tr("Are you sure you want to register non-domain name %1?").arg(name);

    if (QMessageBox::Yes != QMessageBox::question(this, tr("Confirm name registration"),
          msg,
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel))
    {
        return;
    }

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;
    
    QString err_msg;

    try
    {
        WalletModel::NameNewReturn res = walletModel->nameNew(name);

        if (res.ok)
        {
            ui->registerName->setText("d/");
            ui->submitNameButton->setDefault(true);

            int newRowIndex;
            // FIXME: CT_NEW may have been sent from nameNew (via transaction).
            // Currently updateEntry is modified so it does not complain
            model->updateEntry(name, "", NameTableEntry::NAME_NEW, CT_NEW, &newRowIndex);
            ui->tableView->selectRow(newRowIndex);
            ui->tableView->setFocus();

            ConfigureNameDialog dlg(name, "", true, this);
            dlg.setModel(walletModel);
            if (dlg.exec() == QDialog::Accepted)
            {
                LOCK(cs_main);
                if (mapMyNameFirstUpdate.count(vchFromString(name.toStdString())) != 0)
                    model->updateEntry(name, dlg.getReturnData(), NameTableEntry::NAME_NEW, CT_UPDATED);
                else
                {
                    // name_firstupdate could have been sent, while the user was editing the value
                    // Do nothing
                }
            }

            return;
        }

        err_msg = res.err_msg;
    }
    catch (std::exception& e) 
    {
        err_msg = e.what();
    }

    if (err_msg == "ABORTED")
        return;

    QMessageBox::warning(this, tr("Name registration failed"), err_msg);
}

bool ManageNamesPage::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        if (object == ui->registerName)
        {
            ui->submitNameButton->setDefault(true);
            ui->configureNameButton->setDefault(false);
        }
        else if (object == ui->tableView)
        {
            ui->submitNameButton->setDefault(false);
            ui->configureNameButton->setDefault(true);
        }
    }
    return QDialog::eventFilter(object, event);
}

void ManageNamesPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        ui->configureNameButton->setEnabled(true);
    }
    else
    {
        ui->configureNameButton->setEnabled(false);
    }
}

void ManageNamesPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if (index.isValid())
        contextMenu->exec(QCursor::pos());
}

void ManageNamesPage::onCopyNameAction()
{
    GUIUtil::copyEntryData(ui->tableView, NameTableModel::Name);
}

void ManageNamesPage::onCopyValueAction()
{
    GUIUtil::copyEntryData(ui->tableView, NameTableModel::Value);
}

void ManageNamesPage::on_configureNameButton_clicked()
{
    if(!ui->tableView->selectionModel())
        return;
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows(NameTableModel::Name);
    if(indexes.isEmpty())
        return;

    QModelIndex index = indexes.at(0);

    QString name = index.data(Qt::EditRole).toString();
    QString value = index.sibling(index.row(), NameTableModel::Value).data(Qt::EditRole).toString();

    std::vector<unsigned char> vchName = vchFromString(name.toStdString());
    bool fFirstUpdate = mapMyNameFirstUpdate.count(vchName) != 0;

    ConfigureNameDialog dlg(name, value, fFirstUpdate, this);
    dlg.setModel(walletModel);
    if (dlg.exec() == QDialog::Accepted && fFirstUpdate)
    {
        LOCK(cs_main);
        // name_firstupdate could have been sent, while the user was editing the value
        if (mapMyNameFirstUpdate.count(vchName) != 0)
            model->updateEntry(name, dlg.getReturnData(), NameTableEntry::NAME_NEW, CT_UPDATED);
    }
}

void ManageNamesPage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Registered Names Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Name", NameTableModel::Name, Qt::EditRole);
    writer.addColumn("Value", NameTableModel::Value, Qt::EditRole);
    writer.addColumn("Expires In", NameTableModel::ExpiresIn, Qt::EditRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}
