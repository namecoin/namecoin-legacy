include $(top_srcdir)/src/Makefile.include

AM_CPPFLAGS += -I$(top_srcdir)/src \
  -I$(top_builddir)/src/qt \
  -I$(top_builddir)/src/qt/forms \
  $(PROTOBUF_CFLAGS) \
  $(QR_CFLAGS)
bin_PROGRAMS = namecoin-qt
noinst_LIBRARIES = libnamecoinqt.a
SUBDIRS = $(BUILD_TEST_QT)
DIST_SUBDIRS = test

# namecoin qt core #
QT_TS = locale/namecoin_ach.ts \
  locale/namecoin_af_ZA.ts \
  locale/namecoin_ar.ts \
  locale/namecoin_be_BY.ts \
  locale/namecoin_bg.ts \
  locale/namecoin_bs.ts \
  locale/namecoin_ca_ES.ts \
  locale/namecoin_ca.ts \
  locale/namecoin_cs.ts \
  locale/namecoin_cy.ts \
  locale/namecoin_da.ts \
  locale/namecoin_de.ts \
  locale/namecoin_de_AT.ts \
  locale/namecoin_el_GR.ts \
  locale/namecoin_en.ts \
  locale/namecoin_eo.ts \
  locale/namecoin_es_CL.ts \
  locale/namecoin_es.ts \
  locale/namecoin_et.ts \
  locale/namecoin_eu_ES.ts \
  locale/namecoin_fa_IR.ts \
  locale/namecoin_fa.ts \
  locale/namecoin_fi.ts \
  locale/namecoin_fr_CA.ts \
  locale/namecoin_fr.ts \
  locale/namecoin_gl.ts \
  locale/namecoin_gu_IN.ts \
  locale/namecoin_he.ts \
  locale/namecoin_hi_IN.ts \
  locale/namecoin_hr.ts \
  locale/namecoin_hu.ts \
  locale/namecoin_id_ID.ts \
  locale/namecoin_it.ts \
  locale/namecoin_ja.ts \
  locale/namecoin_kk_KZ.ts \
  locale/namecoin_ko_KR.ts \
  locale/namecoin_la.ts \
  locale/namecoin_lt.ts \
  locale/namecoin_lv_LV.ts \
  locale/namecoin_ms_MY.ts \
  locale/namecoin_nb.ts \
  locale/namecoin_nl.ts \
  locale/namecoin_pl.ts \
  locale/namecoin_pt_BR.ts \
  locale/namecoin_pt_PT.ts \
  locale/namecoin_ro_RO.ts \
  locale/namecoin_ru.ts \
  locale/namecoin_sk.ts \
  locale/namecoin_sl_SI.ts \
  locale/namecoin_sq.ts \
  locale/namecoin_sr.ts \
  locale/namecoin_sv.ts \
  locale/namecoin_th_TH.ts \
  locale/namecoin_tr.ts \
  locale/namecoin_uk.ts \
  locale/namecoin_vi.ts \
  locale/namecoin_vi_VN.ts \
  locale/namecoin_zh_CN.ts \
  locale/namecoin_zh_TW.ts

QT_FORMS_UI = forms/aboutdialog.ui forms/addressbookpage.ui \
  forms/askpassphrasedialog.ui \
  forms/coincontroldialog.ui \
  forms/editaddressdialog.ui \
  forms/intro.ui \
  forms/openuridialog.ui \
  forms/optionsdialog.ui forms/overviewpage.ui forms/receiverequestdialog.ui \
  forms/receivecoinsdialog.ui \
  forms/rpcconsole.ui forms/sendcoinsdialog.ui forms/sendcoinsentry.ui \
  forms/signverifymessagedialog.ui forms/transactiondescdialog.ui

QT_MOC_CPP = moc_aboutdialog.cpp moc_addressbookpage.cpp \
  moc_addresstablemodel.cpp moc_askpassphrasedialog.cpp \
  moc_namecoinaddressvalidator.cpp moc_namecoinamountfield.cpp \
  moc_namecoingui.cpp moc_namecoinunits.cpp moc_clientmodel.cpp \
  moc_coincontroldialog.cpp \
  moc_coincontroltreewidget.cpp \
  moc_csvmodelwriter.cpp moc_editaddressdialog.cpp moc_guiutil.cpp \
  moc_intro.cpp moc_macdockiconhandler.cpp moc_macnotificationhandler.cpp \
  moc_monitoreddatamapper.cpp moc_notificator.cpp \
  moc_openuridialog.cpp \
  moc_optionsdialog.cpp \
  moc_optionsmodel.cpp moc_overviewpage.cpp moc_paymentserver.cpp \
  moc_receiverequestdialog.cpp moc_qvalidatedlineedit.cpp moc_qvaluecombobox.cpp \
  moc_receivecoinsdialog.cpp \
  moc_recentrequeststablemodel.cpp \
  moc_rpcconsole.cpp moc_sendcoinsdialog.cpp moc_sendcoinsentry.cpp \
  moc_signverifymessagedialog.cpp moc_splashscreen.cpp moc_trafficgraphwidget.cpp moc_transactiondesc.cpp \
  moc_transactiondescdialog.cpp moc_transactionfilterproxy.cpp \
  moc_transactiontablemodel.cpp moc_transactionview.cpp moc_walletframe.cpp \
  moc_walletmodel.cpp moc_walletview.cpp

NAMECOIN_MM = macdockiconhandler.mm macnotificationhandler.mm

QT_MOC = intro.moc overviewpage.moc rpcconsole.moc

QT_QRC_CPP = qrc_namecoin.cpp
QT_QRC = namecoin.qrc

PROTOBUF_CC = paymentrequest.pb.cc
PROTOBUF_H = paymentrequest.pb.h
PROTOBUF_PROTO = paymentrequest.proto

NAMECOIN_QT_H  = aboutdialog.h addressbookpage.h addresstablemodel.h \
  askpassphrasedialog.h namecoinaddressvalidator.h namecoinamountfield.h \
  namecoingui.h namecoinunits.h clientmodel.h coincontroldialog.h coincontroltreewidget.h csvmodelwriter.h \
  editaddressdialog.h guiconstants.h guiutil.h intro.h macdockiconhandler.h \
  macnotificationhandler.h monitoreddatamapper.h notificator.h \
  openuridialog.h \
  optionsdialog.h \
  optionsmodel.h overviewpage.h paymentrequestplus.h paymentserver.h \
  receivecoinsdialog.h \
  recentrequeststablemodel.h \
  receiverequestdialog.h qvalidatedlineedit.h qvaluecombobox.h rpcconsole.h \
  sendcoinsdialog.h sendcoinsentry.h signverifymessagedialog.h splashscreen.h \
  trafficgraphwidget.h transactiondescdialog.h transactiondesc.h transactionfilterproxy.h \
  transactionrecord.h transactiontablemodel.h transactionview.h walletframe.h \
  walletmodel.h walletmodeltransaction.h walletview.h

RES_ICONS = res/icons/namecoin.png res/icons/address-book.png \
  res/icons/quit.png res/icons/send.png res/icons/toolbar.png \
  res/icons/connect0_16.png res/icons/connect1_16.png \
  res/icons/connect2_16.png res/icons/connect3_16.png \
  res/icons/connect4_16.png res/icons/transaction0.png \
  res/icons/transaction2.png res/icons/clock1.png res/icons/clock2.png \
  res/icons/clock3.png res/icons/clock4.png res/icons/clock5.png \
  res/icons/configure.png res/icons/receive.png res/icons/editpaste.png \
  res/icons/editcopy.png res/icons/add.png res/icons/namecoin_testnet.png \
  res/icons/toolbar_testnet.png res/icons/edit.png res/icons/history.png \
  res/icons/overview.png res/icons/export.png res/icons/synced.png \
  res/icons/remove.png res/icons/tx_mined.png res/icons/tx_input.png \
  res/icons/tx_output.png res/icons/tx_inout.png res/icons/lock_closed.png \
  res/icons/lock_open.png res/icons/key.png res/icons/filesave.png \
  res/icons/qrcode.png res/icons/debugwindow.png res/icons/namecoin.ico \
  res/icons/namecoin_testnet.ico

NAMECOIN_QT_CPP = aboutdialog.cpp addressbookpage.cpp \
  addresstablemodel.cpp askpassphrasedialog.cpp namecoinaddressvalidator.cpp \
  namecoinamountfield.cpp namecoin.cpp namecoingui.cpp \
  namecoinunits.cpp clientmodel.cpp \
  coincontroldialog.cpp \
  coincontroltreewidget.cpp \
  csvmodelwriter.cpp editaddressdialog.cpp \
  guiutil.cpp intro.cpp monitoreddatamapper.cpp notificator.cpp \
  openuridialog.cpp \
  optionsdialog.cpp optionsmodel.cpp overviewpage.cpp paymentrequestplus.cpp \
  paymentserver.cpp qvalidatedlineedit.cpp qvaluecombobox.cpp \
  receivecoinsdialog.cpp receiverequestdialog.cpp \
  recentrequeststablemodel.cpp \
  rpcconsole.cpp sendcoinsdialog.cpp sendcoinsentry.cpp \
  signverifymessagedialog.cpp splashscreen.cpp trafficgraphwidget.cpp transactiondesc.cpp \
  transactiondescdialog.cpp transactionfilterproxy.cpp transactionrecord.cpp \
  transactiontablemodel.cpp transactionview.cpp walletframe.cpp \
  walletmodel.cpp walletmodeltransaction.cpp walletview.cpp

RES_IMAGES = res/images/about.png res/images/splash.png \
  res/images/splash_testnet.png

RES_MOVIES = res/movies/update_spinner.mng

NAMECOIN_RC = res/namecoin-qt-res.rc

libnamecoinqt_a_CPPFLAGS = $(AM_CPPFLAGS) $(QT_INCLUDES) \
  -I$(top_srcdir)/src/qt/forms $(QT_DBUS_INCLUDES)
libnamecoinqt_a_SOURCES = $(NAMECOIN_QT_CPP) $(NAMECOIN_QT_H) $(QT_FORMS_UI) \
  $(QT_QRC) $(QT_TS) $(PROTOBUF_PROTO) $(RES_ICONS) $(RES_IMAGES) $(RES_MOVIES)

nodist_libnamecoinqt_a_SOURCES = $(QT_MOC_CPP) $(QT_MOC) $(PROTOBUF_CC) \
  $(PROTOBUF_H) $(QT_QRC_CPP)

BUILT_SOURCES = $(nodist_libnamecoinqt_a_SOURCES)

#Generating these with a half-written protobuf header leads to wacky results.
#This makes sure it's done.
$(QT_MOC): $(PROTOBUF_H)
$(QT_MOC_CPP): $(PROTOBUF_H)

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
namecoin_qt_SOURCES = namecoin.cpp
namecoin_qt_LDADD = libnamecoinqt.a $(LIBNAMECOIN_SERVER) $(LIBNAMECOIN_WALLET) $(LIBNAMECOIN_CLI) $(LIBNAMECOIN_COMMON) $(LIBLEVELDB) $(LIBMEMENV) \
  $(BOOST_LIBS) $(QT_LIBS) $(QT_DBUS_LIBS) $(QR_LIBS) $(PROTOBUF_LIBS) $(BDB_LIBS)

# forms/foo.h -> forms/ui_foo.h
QT_FORMS_H=$(join $(dir $(QT_FORMS_UI)),$(addprefix ui_, $(notdir $(QT_FORMS_UI:.ui=.h))))

#locale/foo.ts -> locale/foo.qm
QT_QM=$(QT_TS:.ts=.qm)

.PHONY: FORCE
.SECONDARY: $(QT_QM)

namecoinstrings.cpp: FORCE
	$(MAKE) -C $(top_srcdir)/src qt/namecoinstrings.cpp

translate: namecoinstrings.cpp $(QT_FORMS_UI) $(QT_FORMS_UI) $(NAMECOIN_QT_CPP) $(NAMECOIN_QT_H) $(NAMECOIN_MM)
	@test -n $(LUPDATE) || echo "lupdate is required for updating translations"
	@$(LUPDATE) $^ -locations relative -no-obsolete -ts locale/namecoin_en.ts

$(QT_QRC_CPP): $(QT_QRC) $(QT_QM) $(QT_FORMS_H) $(RES_ICONS) $(RES_IMAGES) $(RES_MOVIES) $(PROTOBUF_H)
	@cd $(abs_srcdir); test -f $(RCC) && $(RCC) -name namecoin -o $(abs_builddir)/$@ $< || \
	  echo error: could not build $@
	$(SED) -i.bak -e '/^\*\*.*Created:/d' $@ && rm $@.bak
	$(SED) -i.bak -e '/^\*\*.*by:/d' $@ && rm $@.bak

CLEANFILES = $(BUILT_SOURCES) $(QT_QM) $(QT_FORMS_H) *.gcda *.gcno
