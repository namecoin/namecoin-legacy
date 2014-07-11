#include "config.h"

#include "managenamespage.h"
#include "managenamespage.ui.h"

#include "walletmodel.h"
#include "nametablemodel.h"
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "../headers.h"
#include "../wallet.h"
#include "../namecoin.h"
#include "guiconstants.h"
#include "ui_interface.h"
#include "configurenamedialog.h"

#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QMenu>
#include <QScrollBar>

extern std::map<std::vector<unsigned char>, PreparedNameFirstUpdate> mapMyNameFirstUpdate;

//
// NameFilterProxyModel
//

NameFilterProxyModel::NameFilterProxyModel(QObject *parent /* = 0*/)
    : QSortFilterProxyModel(parent)
{
}

void NameFilterProxyModel::setNameSearch(const QString &search)
{
    nameSearch = search;
    invalidateFilter();
}

void NameFilterProxyModel::setValueSearch(const QString &search)
{
    valueSearch = search;
    invalidateFilter();
}

void NameFilterProxyModel::setAddressSearch(const QString &search)
{
    addressSearch = search;
    invalidateFilter();
}

bool NameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    QString name = index.sibling(index.row(), NameTableModel::Name).data(Qt::EditRole).toString();
    QString value = index.sibling(index.row(), NameTableModel::Value).data(Qt::EditRole).toString();
    QString address = index.sibling(index.row(), NameTableModel::Address).data(Qt::EditRole).toString();

    Qt::CaseSensitivity case_sens = filterCaseSensitivity();
    return name.contains(nameSearch, case_sens)
        && value.contains(valueSearch, case_sens)
        && address.startsWith(addressSearch, Qt::CaseSensitive);   // Address is always case-sensitive
}

//
// ManageNamesPage
//

const static int COLUMN_WIDTH_NAME = 300,
                 COLUMN_WIDTH_ADDRESS = 256,
                 COLUMN_WIDTH_EXPIRES_IN = 100;

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
    QAction *copyAddressAction = new QAction(tr("Copy &Address"), this);
    QAction *configureNameAction = new QAction(tr("&Configure Name..."), this);
    
    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyNameAction);
    contextMenu->addAction(copyValueAction);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(configureNameAction);
    
    // Connect signals for context menu actions
    connect(copyNameAction, SIGNAL(triggered()), this, SLOT(onCopyNameAction()));
    connect(copyValueAction, SIGNAL(triggered()), this, SLOT(onCopyValueAction()));
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(onCopyAddressAction()));
    connect(configureNameAction, SIGNAL(triggered()), this, SLOT(on_configureNameButton_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_configureNameButton_clicked()));
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Catch focus changes to make the appropriate button the default one (Submit or Configure)
    ui->registerName->installEventFilter(this);
    ui->submitNameButton->installEventFilter(this);
    ui->tableView->installEventFilter(this);
    ui->nameFilter->installEventFilter(this);
    ui->valueFilter->installEventFilter(this);
    ui->addressFilter->installEventFilter(this);
    ui->configureNameButton->installEventFilter(this);

    ui->registerName->setMaxLength(MAX_NAME_LENGTH);
    
    ui->nameFilter->setMaxLength (MAX_NAME_LENGTH);
    ui->valueFilter->setMaxLength (UI_MAX_VALUE_LENGTH);
    GUIUtil::setupAddressWidget (ui->addressFilter, this, true);
    
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->nameFilter->setPlaceholderText(tr("Name filter"));
    ui->valueFilter->setPlaceholderText(tr("Value filter"));
    ui->addressFilter->setPlaceholderText(tr("Address filter"));
#endif

    ui->nameFilter->setFixedWidth(COLUMN_WIDTH_NAME);
    ui->addressFilter->setFixedWidth(COLUMN_WIDTH_ADDRESS);
    ui->horizontalSpacer_ExpiresIn->changeSize(
        COLUMN_WIDTH_EXPIRES_IN + ui->tableView->verticalScrollBar()->sizeHint().width()
            
#ifdef Q_OS_MAC
        // Not sure if this is needed, but other Mac code adds 2 pixels to scroll bar width;
        // see transactionview.cpp, search for verticalScrollBar()->sizeHint()
        + 2
#endif

        ,
        ui->horizontalSpacer_ExpiresIn->sizeHint().height(),
        QSizePolicy::Fixed);
}

ManageNamesPage::~ManageNamesPage()
{
    delete ui;
}

void ManageNamesPage::setModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    model = walletModel->getNameTableModel();

    proxyModel = new NameFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    ui->tableView->horizontalHeader()->setHighlightSections(false);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(
            NameTableModel::Name, COLUMN_WIDTH_NAME);
#if QT_VERSION < 0x050000
    ui->tableView->horizontalHeader()->setResizeMode(
            NameTableModel::Value, QHeaderView::Stretch);
#else
    ui->tableView->horizontalHeader()->setSectionResizeMode(
            NameTableModel::Value, QHeaderView::Stretch);
#endif
    ui->tableView->horizontalHeader()->resizeSection(
            NameTableModel::Address, COLUMN_WIDTH_ADDRESS);
    ui->tableView->horizontalHeader()->resizeSection(
            NameTableModel::ExpiresIn, COLUMN_WIDTH_EXPIRES_IN);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));
            
    connect(ui->nameFilter, SIGNAL(textChanged(QString)), this, SLOT(changedNameFilter(QString)));
    connect(ui->valueFilter, SIGNAL(textChanged(QString)), this, SLOT(changedValueFilter(QString)));
    connect(ui->addressFilter, SIGNAL(textChanged(QString)), this, SLOT(changedAddressFilter(QString)));

    selectionChanged();
}

void ManageNamesPage::changedNameFilter(const QString &filter)
{
    if (!proxyModel)
        return;
    proxyModel->setNameSearch(filter);
}

void ManageNamesPage::changedValueFilter(const QString &filter)
{
    if (!proxyModel)
        return;
    proxyModel->setValueSearch(filter);
}

void ManageNamesPage::changedAddressFilter(const QString &filter)
{
    if (!proxyModel)
        return;
    proxyModel->setAddressSearch(filter);
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

    // TODO: name needs more exhaustive syntax checking, Unicode characters etc.
    // TODO: maybe it should be done while the user is typing (e.g. show/hide a red notice below the input box)
    if (name != name.simplified() || name.contains(" "))
    {
        if (QMessageBox::Yes != QMessageBox::warning(this, tr("Name registration warning"),
              tr("The name you entered contains whitespace characters. It is probably invalid. Are you sure you want to use this name?"),
              QMessageBox::Yes | QMessageBox::Cancel,
              QMessageBox::Cancel))
        {
            return;
        }
    }
    if (!name.contains("/") || name.startsWith("/"))
    {
        if (QMessageBox::Yes != QMessageBox::warning(this, tr("Name registration warning"),
              tr("The name you entered does not start with prefix (such as \"d/\"). It may be invalid for certain tasks. Are you sure you want to use this name?"),
              QMessageBox::Yes | QMessageBox::Cancel,
              QMessageBox::Cancel))
        {
            return;
        }
    }

    QString msg;
    if (name.startsWith("d/"))
        msg = tr("Are you sure you want to register domain name %1, which corresponds to domain %2?").arg(GUIUtil::HtmlEscape(name)).arg(GUIUtil::HtmlEscape(name.mid(2) + ".bit"));
    else
        msg = tr("Are you sure you want to register non-domain name %1?").arg(GUIUtil::HtmlEscape(name));
    msg += "<br><br>" + tr("This will issue both a name_new and a postponed name_firstupdate. Let the program run for three hours to make sure the process can finish.");

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
            model->updateEntry(name, "", res.address, NameTableEntry::NAME_NEW, CT_NEW, &newRowIndex);
            ui->tableView->selectRow(newRowIndex);
            ui->tableView->setFocus();

            ConfigureNameDialog dlg(name, "", res.address, true, this);
            dlg.setModel(walletModel);
            if (dlg.exec() == QDialog::Accepted)
            {
                LOCK(cs_main);
                if (mapMyNameFirstUpdate.count(vchFromString(name.toStdString())) != 0)
                    model->updateEntry(name, dlg.getReturnData(), res.address, NameTableEntry::NAME_NEW, CT_UPDATED);
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
        if (object == ui->registerName || object == ui->submitNameButton)
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

void ManageNamesPage::onCopyAddressAction()
{
    GUIUtil::copyEntryData(ui->tableView, NameTableModel::Address);
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
    QString address = index.sibling(index.row(), NameTableModel::Address).data(Qt::EditRole).toString();

    std::vector<unsigned char> vchName = vchFromString(name.toStdString());
    bool fFirstUpdate = mapMyNameFirstUpdate.count(vchName) != 0;

    ConfigureNameDialog dlg(name, value, address, fFirstUpdate, this);
    dlg.setModel(walletModel);
    if (dlg.exec() == QDialog::Accepted && fFirstUpdate)
    {
        LOCK(cs_main);
        // name_firstupdate could have been sent, while the user was editing the value
        if (mapMyNameFirstUpdate.count(vchName) != 0)
            model->updateEntry(name, dlg.getReturnData(), address, NameTableEntry::NAME_NEW, CT_UPDATED);
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

    writer.setModel(proxyModel);
    // name, column, role
    writer.addColumn("Name", NameTableModel::Name, Qt::EditRole);
    writer.addColumn("Value", NameTableModel::Value, Qt::EditRole);
    writer.addColumn("Address", NameTableModel::Address, Qt::EditRole);
    writer.addColumn("Expires In", NameTableModel::ExpiresIn, Qt::EditRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}
