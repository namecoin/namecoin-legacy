include ../extra.mk

PROG = ${NAMECOIN}${PROG_SUFFIX}

SRCS = auxpow.cc	\
       bitcoinrpc.cc	\
       crypter.cc	\
       cryptopp/cpu.cc	\
       cryptopp/sha.cc	\
       db.cc		\
       init.cc		\
       irc.cc		\
       key.cc		\
       keystore.cc	\
       main.cc		\
       namecoin.cc	\
       net.cc		\
       script.cc	\
       util.cc		\
       wallet.cc	\
       walletdb.cc

SRCS_QT = qt/aboutdialog.cc		\
	  qt/addressbookpage.cc		\
	  qt/addresstablemodel.cc	\
	  qt/askpassphrasedialog.cc	\
	  qt/bitcoin.cc			\
	  qt/bitcoinaddressvalidator.cc	\
	  qt/bitcoinamountfield.cc	\
	  qt/bitcoingui.cc		\
	  qt/bitcoinstrings.cc		\
	  qt/bitcoinunits.cc		\
	  qt/clientmodel.cc		\
	  qt/configurenamedialog.cc	\
	  qt/csvmodelwriter.cc		\
	  qt/editaddressdialog.cc	\
	  qt/guiutil.cc			\
	  ${QT_MACDOCKICONHANDLER_MM}	\
	  qt/managenamespage.cc		\
	  qt/monitoreddatamapper.cc	\
	  qt/nametablemodel.cc		\
	  qt/netbase.cc			\
	  qt/notificator.cc		\
	  qt/optionsdialog.cc		\
	  qt/optionsmodel.cc		\
	  qt/overviewpage.cc		\
	  qt/paymentserver.cc		\
	  qt/qvalidatedlineedit.cc	\
	  qt/qvaluecombobox.cc		\
	  qt/rpcconsole.cc		\
	  qt/sendcoinsdialog.cc		\
	  qt/sendcoinsentry.cc		\
	  qt/signverifymessagedialog.cc	\
	  qt/transactiondesc.cc		\
	  qt/transactiondescdialog.cc	\
	  qt/transactionfilterproxy.cc	\
	  qt/transactionrecord.cc	\
	  qt/transactiontablemodel.cc	\
	  qt/transactionview.cc		\
	  qt/walletmodel.cc		\
	  ${SRCS_QT_MOC}		\
	  qt/bitcoin.qrc.cc
SRCS_QT_MOC = qt/aboutdialog.moc.cc		\
	      qt/addressbookpage.moc.cc		\
	      qt/addresstablemodel.moc.cc	\
	      qt/askpassphrasedialog.moc.cc	\
	      qt/bitcoinaddressvalidator.moc.cc	\
	      qt/bitcoinamountfield.moc.cc	\
	      qt/bitcoingui.moc.cc		\
	      qt/bitcoinunits.moc.cc		\
	      qt/clientmodel.moc.cc		\
	      qt/configurenamedialog.moc.cc	\
	      qt/csvmodelwriter.moc.cc		\
	      qt/editaddressdialog.moc.cc	\
	      qt/guiutil.moc.cc			\
	      ${QT_MACDOCKICONHANDLER_MOC_CC}	\
	      qt/managenamespage.moc.cc		\
	      qt/monitoreddatamapper.moc.cc	\
	      qt/nametablemodel.moc.cc		\
	      qt/notificator.moc.cc		\
	      qt/optionsdialog.moc.cc		\
	      qt/optionsmodel.moc.cc		\
	      qt/overviewpage.moc.cc		\
	      qt/paymentserver.moc.cc		\
	      qt/qvalidatedlineedit.moc.cc	\
	      qt/qvaluecombobox.moc.cc		\
	      qt/rpcconsole.moc.cc		\
	      qt/sendcoinsdialog.moc.cc		\
	      qt/sendcoinsentry.moc.cc		\
	      qt/signverifymessagedialog.moc.cc	\
	      qt/transactiondesc.moc.cc		\
	      qt/transactiondescdialog.moc.cc	\
	      qt/transactionfilterproxy.moc.cc	\
	      qt/transactiontablemodel.moc.cc	\
	      qt/transactionview.moc.cc		\
	      qt/walletmodel.moc.cc
MOC_FILES = qt/overviewpage.moc	\
	    qt/rpcconsole.moc
FORMS_HEADERS = qt/forms/aboutdialog.ui.h		\
		qt/forms/addressbookpage.ui.h		\
		qt/forms/askpassphrasedialog.ui.h	\
		qt/forms/configurenamedialog.ui.h	\
		qt/forms/editaddressdialog.ui.h		\
		qt/forms/managenamespage.ui.h		\
		qt/forms/optionsdialog.ui.h		\
		qt/forms/overviewpage.ui.h		\
		qt/forms/rpcconsole.ui.h		\
		qt/forms/sendcoinsdialog.ui.h		\
		qt/forms/sendcoinsentry.ui.h		\
		qt/forms/signverifymessagedialog.ui.h	\
		qt/forms/transactiondescdialog.ui.h
QM_FILES = qt/locale/bitcoin_af_ZA.qm	\
	   qt/locale/bitcoin_ar.qm	\
	   qt/locale/bitcoin_bg.qm	\
	   qt/locale/bitcoin_ca_ES.qm	\
	   qt/locale/bitcoin_cs.qm	\
	   qt/locale/bitcoin_da.qm	\
	   qt/locale/bitcoin_de.qm	\
	   qt/locale/bitcoin_el_GR.qm	\
	   qt/locale/bitcoin_en.qm	\
	   qt/locale/bitcoin_es.qm	\
	   qt/locale/bitcoin_es_CL.qm	\
	   qt/locale/bitcoin_et.qm	\
	   qt/locale/bitcoin_eu_ES.qm	\
	   qt/locale/bitcoin_fa.qm	\
	   qt/locale/bitcoin_fa_IR.qm	\
	   qt/locale/bitcoin_fi.qm	\
	   qt/locale/bitcoin_fr.qm	\
	   qt/locale/bitcoin_fr_CA.qm	\
	   qt/locale/bitcoin_gu_IN.qm	\
	   qt/locale/bitcoin_he.qm	\
	   qt/locale/bitcoin_hi_IN.qm	\
	   qt/locale/bitcoin_hr.qm	\
	   qt/locale/bitcoin_hu.qm	\
	   qt/locale/bitcoin_it.qm	\
	   qt/locale/bitcoin_ja.qm	\
	   qt/locale/bitcoin_lt.qm	\
	   qt/locale/bitcoin_nb.qm	\
	   qt/locale/bitcoin_nl.qm	\
	   qt/locale/bitcoin_pl.qm	\
	   qt/locale/bitcoin_pt_BR.qm	\
	   qt/locale/bitcoin_pt_PT.qm	\
	   qt/locale/bitcoin_ro_RO.qm	\
	   qt/locale/bitcoin_ru.qm	\
	   qt/locale/bitcoin_sk.qm	\
	   qt/locale/bitcoin_sr.qm	\
	   qt/locale/bitcoin_sv.qm	\
	   qt/locale/bitcoin_th_TH.qm	\
	   qt/locale/bitcoin_tr.qm	\
	   qt/locale/bitcoin_uk.qm	\
	   qt/locale/bitcoin_zh_CN.qm	\
	   qt/locale/bitcoin_zh_TW.qm

SRCS += ${USE_SRCS_QT}

CLEAN = ${SRCS_QT_MOC}		\
	${MOC_FILES}		\
	${FORMS_HEADERS}	\
	${QM_FILES}		\
	qt/bitcoin.qrc.cc

include ../buildsys.mk

.SUFFIXES: .cc .h .moc .moc.cc .qm .qrc .qrc.cc .ts .ui .ui.h

pre-depend: ${USE_MOC_FILES} ${USE_FORMS_HEADERS} ${USE_QM_FILES}

.h.moc.cc:
	${MOC} ${MOC_CPPFLAGS} $< -o $@

.ui.ui.h:
	${UIC} -p $< -o $@

.cc.moc:
	${MOC} ${MOC_CPPFLAGS} $< -o $@

.ts.qm:
	${LRELEASE} $< -qm $@

.qrc.qrc.cc:
	${RCC} -name bitcoin $< -o $@

CPPFLAGS += -I.. -Iqt -Iqt/forms
LD = ${CXX}
