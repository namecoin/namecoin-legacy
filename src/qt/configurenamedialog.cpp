#include "configurenamedialog.h"
#include "ui_configurenamedialog.h"

#include "guiutil.h"
#include "addressbookpage.h"
#include "walletmodel.h"
#include "../main.h"
#include "../hook.h"
#include "../wallet.h"
#include "../namecoin.h"

#include <QMessageBox>
#include <QClipboard>

ConfigureNameDialog::ConfigureNameDialog(const QString &_name, const QString &data, bool _firstUpdate, QWidget *parent) :
    QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint),
    name(_name),
    firstUpdate(_firstUpdate),
    ui(new Ui::ConfigureNameDialog)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
    ui->transferToLayout->setSpacing(4);
#endif

    GUIUtil::setupAddressWidget(ui->transferTo, this, true);

    ui->labelName->setText(name);
    ui->dataEdit->setText(data);

    ui->dataEdit->setMaxLength(MAX_VALUE_LENGTH);

    returnData = data;

    if (name.startsWith("d/"))
        ui->labelDomain->setText(name.mid(2) + ".bit");
    else
        ui->labelDomain->setText(tr("(not a domain name)"));

    if (firstUpdate)
    {
        ui->labelTransferTo->hide();
        ui->labelTransferToHint->hide();
        ui->transferTo->hide();
        ui->addressBookButton->hide();
        ui->pasteButton->hide();
        ui->labelSubmitHint->setText(tr("Name_firstupdate transaction will be queued and broadcasted when corresponding name_new is %1 blocks old").arg(MIN_FIRSTUPDATE_DEPTH));
    }
    else
    {
        ui->labelSubmitHint->setText(tr("Name_update transaction will be issued immediately"));
        setWindowTitle(tr("Update Name"));
    }
}

ConfigureNameDialog::~ConfigureNameDialog()
{
    delete ui;
}

void ConfigureNameDialog::accept()
{
    if (!walletModel)
        return;

    QString addr;
    if (!firstUpdate)
    {
        if (!ui->transferTo->hasAcceptableInput())
        {
            ui->transferTo->setValid(false);
            return;
        }

        addr = ui->transferTo->text();

        if (addr != "" && !walletModel->validateAddress(addr))
        {
            ui->transferTo->setValid(false);
            return;
        }
    }
    
    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())
        return;

    returnData = ui->dataEdit->text();
    // TODO: JSON syntax checking; maybe removing leading/trailing whitespace

    QString err_msg;
    try
    {
        if (firstUpdate)
            err_msg = walletModel->nameFirstUpdatePrepare(name, returnData);
        else
            err_msg = walletModel->nameUpdate(name, returnData, addr);
    }
    catch (std::exception& e) 
    {
        err_msg = e.what();
    }

    if (!err_msg.isEmpty())
    {
        if (err_msg == "ABORTED")
            return;

        QMessageBox::critical(this, tr("Name update error"), err_msg);
        return;
    }

    QDialog::accept();
}

void ConfigureNameDialog::reject()
{
    QDialog::reject();
}

void ConfigureNameDialog::setModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
}

void ConfigureNameDialog::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->transferTo->setText(QApplication::clipboard()->text());
}

void ConfigureNameDialog::on_addressBookButton_clicked()
{
    if (!walletModel)
        return;
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
    dlg.setModel(walletModel->getAddressTableModel());
    if (dlg.exec())
        ui->transferTo->setText(dlg.getReturnValue());
}
