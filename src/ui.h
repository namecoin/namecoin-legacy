// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_UI_H
#define BITCOIN_UI_H

#ifdef GUI

#include <string>
#include <boost/function.hpp>
#include "wallet.h"
#include "qt/ui_interface.h"

#define wxTheApp true

typedef void wxWindow;
#define wxYES                   CClientUIInterface::BTN_YES
#define wxOK                    CClientUIInterface::BTN_OK
#define wxNO                    CClientUIInterface::BTN_NO
#define wxYES_NO                (wxYES|wxNO)
#define wxCANCEL                CClientUIInterface::BTN_CANCEL
#define wxAPPLY                 CClientUIInterface::BTN_APPLY
#define wxCLOSE                 CClientUIInterface::BTN_CLOSE
#define wxOK_DEFAULT            0
#define wxYES_DEFAULT           0
#define wxNO_DEFAULT            0
#define wxCANCEL_DEFAULT        0
#define wxICON_EXCLAMATION      0
#define wxICON_HAND             CClientUIInterface::MSG_ERROR
#define wxICON_WARNING          wxICON_EXCLAMATION
#define wxICON_ERROR            wxICON_HAND
#define wxICON_QUESTION         0
#define wxICON_INFORMATION      CClientUIInterface::ICON_INFORMATION
#define wxICON_STOP             wxICON_HAND
#define wxICON_ASTERISK         wxICON_INFORMATION
#define wxICON_MASK             CClientUIInterface::ICON_MASK
#define wxFORWARD               0
#define wxBACKWARD              0
#define wxRESET                 0
#define wxHELP                  0
#define wxMORE                  0
#define wxSETUP                 0

inline int MyMessageBox(const std::string& message, const std::string& caption="Message", int style=wxOK, wxWindow* parent=NULL, int x=-1, int y=-1)
{
    return uiInterface.ThreadSafeMessageBox(message, caption, style);
}

#define wxMessageBox MyMessageBox

inline void CalledSetStatusBar(const std::string& strText, int nField)
{
}

inline void UIThreadCall(boost::function0<void> fn)
{
}

inline void CreateMainWindow()
{
}

inline void SetStartOnSystemStartup(bool dummy)
{
}

#endif 
#endif 
