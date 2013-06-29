#ifndef CONFIGURENAMEDIALOG_H
#define CONFIGURENAMEDIALOG_H

#include <QDialog>

namespace Ui {
    class ConfigureNameDialog;
}

class WalletModel;

/** Dialog for editing an address and associated information.
 */
class ConfigureNameDialog : public QDialog
{
    Q_OBJECT

public:

    explicit ConfigureNameDialog(const QString &_name, const QString &data, bool _firstUpdate, QWidget *parent = 0);
    ~ConfigureNameDialog();

    void setModel(WalletModel *walletModel);
    const QString &getReturnData() const { return returnData; }

public slots:
    void accept();
    void reject();
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void on_nsEdit_textChanged()                              { if (initialized) SetDNS(); }
    void on_nsTranslateEdit_textChanged(const QString &text)  { if (initialized) SetDNS(); }
    void on_nsFingerprintEdit_textChanged()                   { if (initialized) SetDNS(); }
    void on_ipEdit_textChanged(const QString &text)           { if (initialized) SetIP();  }
    void on_ipFingerprintEdit_textChanged()                   { if (initialized) SetIP(); }
    void on_dataEdit_textChanged(const QString &text);

private:
    QString returnData;
    Ui::ConfigureNameDialog *ui;
    WalletModel *walletModel;
    QString name;
    bool firstUpdate;
    bool initialized;

    void SetDNS();
    void SetIP();
};

#endif // CONFIGURENAMEDIALOG_H
