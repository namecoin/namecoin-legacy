include $(top_srcdir)/src/Makefile.include

AM_CPPFLAGS += -I$(top_srcdir)/src \
  -I$(top_builddir)/src/qt \
  -I$(top_builddir)/src/qt/forms \
  $(QR_CFLAGS)
bin_PROGRAMS = $(NAMECOIN_QT)
EXTRA_PROGRAMS = namecoin-qt
noinst_LIBRARIES = libnamecoinqt.a
SUBDIRS = $(BUILD_TEST_QT)
DIST_SUBDIRS = test

# namecoin qt core #
QT_TS = locale/bitcoin_af_ZA.ts \
  locale/bitcoin_ar.ts \
  locale/bitcoin_bg.ts \
  locale/bitcoin_ca_ES.ts \
  locale/bitcoin_cs.ts \
  locale/bitcoin_da.ts \
  locale/bitcoin_de.ts \
  locale/bitcoin_el_GR.ts \
  locale/bitcoin_en.ts \
  locale/bitcoin_es_CL.ts \
  locale/bitcoin_es.ts \
  locale/bitcoin_et.ts \
  locale/bitcoin_eu_ES.ts \
  locale/bitcoin_fa_IR.ts \
  locale/bitcoin_fa.ts \
  locale/bitcoin_fi.ts \
  locale/bitcoin_fr_CA.ts \
  locale/bitcoin_fr.ts \
  locale/bitcoin_gu_IN.ts \
  locale/bitcoin_he.ts \
  locale/bitcoin_hi_IN.ts \
  locale/bitcoin_hr.ts \
  locale/bitcoin_hu.ts \
  locale/bitcoin_it.ts \
  locale/bitcoin_ja.ts \
  locale/bitcoin_lt.ts \
  locale/bitcoin_nb.ts \
  locale/bitcoin_nl.ts \
  locale/bitcoin_pl.ts \
  locale/bitcoin_pt_BR.ts \
  locale/bitcoin_pt_PT.ts \
  locale/bitcoin_ro_RO.ts \
  locale/bitcoin_ru.ts \
  locale/bitcoin_sk.ts \
  locale/bitcoin_sr.ts \
  locale/bitcoin_sv.ts \
  locale/bitcoin_th_TH.ts \
  locale/bitcoin_tr.ts \
  locale/bitcoin_uk.ts \
  locale/bitcoin_zh_CN.ts \
  locale/bitcoin_zh_TW.ts

 QT_FORMS_UI = forms/aboutdialog.ui \
  forms/addressbookpage.ui \
  forms/askpassphrasedialog.ui \
  forms/configurenamedialog.ui \
  forms/editaddressdialog.ui \
  forms/managenamespage.ui \
  forms/optionsdialog.ui \
  forms/overviewpage.ui \
  forms/qrcodedialog.ui \
  forms/rpcconsole.ui \
  forms/sendcoinsdialog.ui \
  forms/sendcoinsentry.ui \
  forms/signverifymessagedialog.ui \
  forms/transactiondescdialog.ui

QT_MOC_CPP = moc_aboutdialog.cpp moc_addressbookpage.cpp \
  moc_addresstablemodel.cpp moc_askpassphrasedialog.cpp \
  moc_bitcoinaddressvalidator.cpp moc_bitcoinamountfield.cpp \
  moc_bitcoingui.cpp moc_bitcoinunits.cpp moc_clientmodel.cpp \
  moc_csvmodelwriter.cpp moc_editaddressdialog.cpp moc_guiutil.cpp \
  moc_macdockiconhandler.cpp \
  moc_monitoreddatamapper.cpp moc_notificator.cpp \
  moc_optionsdialog.cpp \
  moc_optionsmodel.cpp moc_overviewpage.cpp moc_paymentserver.cpp \
  moc_qvalidatedlineedit.cpp moc_qvaluecombobox.cpp \
  moc_rpcconsole.cpp moc_sendcoinsdialog.cpp moc_sendcoinsentry.cpp \
  moc_signverifymessagedialog.cpp moc_transactiondesc.cpp \
  moc_transactiondescdialog.cpp moc_transactionfilterproxy.cpp \
  moc_transactiontablemodel.cpp moc_transactionview.cpp \
  moc_walletmodel.cpp moc_nametablemodel.cpp moc_managenamespage.cpp \
  moc_configurenamedialog.cpp

NAMECOIN_MM = macdockiconhandler.mm

QT_MOC = overviewpage.moc rpcconsole.moc

QT_QRC_CPP = qrc_bitcoin.cpp
QT_QRC = bitcoin.qrc

NAMECOIN_QT_H  = aboutdialog.h addressbookpage.h addresstablemodel.h \
  askpassphrasedialog.h bitcoinaddressvalidator.h bitcoinamountfield.h \
  bitcoingui.h bitcoinunits.h clientmodel.h csvmodelwriter.h \
  editaddressdialog.h guiconstants.h guiutil.h macdockiconhandler.h \
  monitoreddatamapper.h notificator.h \
  optionsdialog.h \
  optionsmodel.h overviewpage.h paymentserver.h \
  qvalidatedlineedit.h qvaluecombobox.h rpcconsole.h \
  sendcoinsdialog.h sendcoinsentry.h signverifymessagedialog.h \
  transactiondescdialog.h transactiondesc.h transactionfilterproxy.h \
  transactionrecord.h transactiontablemodel.h transactionview.h \
  walletmodel.h nametablemodel.h managenamespage.h \
  configurenamedialog.h

RES_ICONS = res/icons/bitcoin.png res/icons/address-book.png \
  res/icons/quit.png res/icons/send.png res/icons/toolbar.png \
  res/icons/connect0_16.png res/icons/connect1_16.png \
  res/icons/connect2_16.png res/icons/connect3_16.png \
  res/icons/connect4_16.png res/icons/transaction0.png \
  res/icons/transaction2.png res/icons/clock1.png res/icons/clock2.png \
  res/icons/clock3.png res/icons/clock4.png res/icons/clock5.png \
  res/icons/configure.png res/icons/receive.png res/icons/editpaste.png \
  res/icons/editcopy.png res/icons/add.png res/icons/bitcoin_testnet.png \
  res/icons/toolbar_testnet.png res/icons/edit.png res/icons/history.png \
  res/icons/overview.png res/icons/export.png res/icons/synced.png \
  res/icons/remove.png res/icons/tx_mined.png res/icons/tx_input.png \
  res/icons/tx_output.png res/icons/tx_inout.png res/icons/lock_closed.png \
  res/icons/lock_open.png res/icons/key.png res/icons/filesave.png \
  res/icons/qrcode.png res/icons/debugwindow.png res/icons/bitcoin.ico

NAMECOIN_QT_CPP = aboutdialog.cpp addressbookpage.cpp \
  addresstablemodel.cpp askpassphrasedialog.cpp bitcoinaddressvalidator.cpp \
  bitcoinamountfield.cpp bitcoin.cpp bitcoingui.cpp \
  bitcoinunits.cpp clientmodel.cpp \
  csvmodelwriter.cpp editaddressdialog.cpp \
  guiutil.cpp monitoreddatamapper.cpp notificator.cpp \
  optionsdialog.cpp optionsmodel.cpp overviewpage.cpp \
  paymentserver.cpp qvalidatedlineedit.cpp qvaluecombobox.cpp \
  rpcconsole.cpp sendcoinsdialog.cpp sendcoinsentry.cpp \
  signverifymessagedialog.cpp transactiondesc.cpp \
  transactiondescdialog.cpp transactionfilterproxy.cpp transactionrecord.cpp \
  transactiontablemodel.cpp transactionview.cpp \
  walletmodel.cpp netbase.cpp nametablemodel.cpp managenamespage.cpp \
  configurenamedialog.cpp

RES_IMAGES = res/images/about.png

RES_MOVIES = res/movies/update_spinner.mng

NAMECOIN_RC = res/bitcoin-qt-res.rc

libnamecoinqt_a_CPPFLAGS = $(AM_CPPFLAGS) $(QT_INCLUDES) \
  -I$(top_srcdir)/src/qt/forms $(QT_DBUS_INCLUDES)
libnamecoinqt_a_SOURCES = $(NAMECOIN_QT_CPP) $(NAMECOIN_QT_H) $(QT_FORMS_UI) \
  $(QT_QRC) $(QT_TS) $(RES_ICONS) $(RES_IMAGES) $(RES_MOVIES)

nodist_libnamecoinqt_a_SOURCES = $(QT_MOC_CPP) $(QT_MOC) $(QT_QRC_CPP)

BUILT_SOURCES = $(nodist_libnamecoinqt_a_SOURCES)

if TARGET_DARWIN
  libnamecoinqt_a_SOURCES += $(NAMECOIN_MM)
endif
if TARGET_WINDOWS
  libnamecoinqt_a_SOURCES += $(NAMECOIN_RC)
endif
#

# namecoin-qt binary #
namecoin_qt_CPPFLAGS = $(AM_CPPFLAGS) $(QT_INCLUDES) \
  -I$(top_srcdir)/src/qt/forms
namecoin_qt_SOURCES = bitcoin.cpp
namecoin_qt_LDADD = libnamecoinqt.a $(LIBNAMECOIN_SERVER) $(LIBNAMECOIN_WALLET) $(LIBNAMECOIN_CLI) $(LIBNAMECOIN_COMMON) $(LIBLEVELDB) $(LIBMEMENV) \
  $(BOOST_LIBS) $(QT_LIBS) $(QT_DBUS_LIBS) $(QR_LIBS) $(BDB_LIBS)

# forms/foo.h -> forms/ui_foo.h
QT_FORMS_H=$(join $(dir $(QT_FORMS_UI)),$(addprefix ui_, $(notdir $(QT_FORMS_UI:.ui=.h))))

#locale/foo.ts -> locale/foo.qm
QT_QM=$(QT_TS:.ts=.qm)

.PHONY: FORCE
.SECONDARY: $(QT_QM)

namecoinstrings.cpp: FORCE
	$(MAKE) -C $(top_srcdir)/src qt/bitcoinstrings.cpp

translate: bitcoinstrings.cpp $(QT_FORMS_UI) $(QT_FORMS_UI) $(NAMECOIN_QT_CPP) $(NAMECOIN_QT_H) $(NAMECOIN_MM)
	@test -n $(LUPDATE) || echo "lupdate is required for updating translations"
	@$(LUPDATE) $^ -locations relative -no-obsolete -ts locale/bitcoin_en.ts

$(QT_QRC_CPP): $(QT_QRC) $(QT_QM) $(QT_FORMS_H) $(RES_ICONS) $(RES_IMAGES) $(RES_MOVIES)
	@cd $(abs_srcdir); test -f $(RCC) && $(RCC) -name namecoin -o $(abs_builddir)/$@ $< || \
	  echo error: could not build $@
	$(SED) -i.bak -e '/^\*\*.*Created:/d' $@ && rm $@.bak
	$(SED) -i.bak -e '/^\*\*.*by:/d' $@ && rm $@.bak

CLEANFILES = $(BUILT_SOURCES) $(QT_QM) $(QT_FORMS_H) *.gcda *.gcno
