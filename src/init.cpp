// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#include "headers.h"
#include "db.h"
#include "bitcoinrpc.h"
#include "net.h"
#include "init.h"
#include "strlcpy.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>

using namespace std;
using namespace boost;

void rescanfornames();

CWallet* pwalletMain;
char *walletPath = "";

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

void ExitTimeout(void* parg)
{
#ifdef __WXMSW__
    MilliSleep(5000);
    ExitProcess(0);
#endif
}

void StartShutdown()
{
#ifdef GUI
    // ensure we leave the Qt main loop for a clean GUI exit (Shutdown() is called in bitcoin.cpp afterwards)
    uiInterface.QueueShutdown();
#else
    // Without UI, Shutdown() can simply be started in a new thread
    CreateThread(Shutdown, NULL);
#endif
}

void Shutdown(void* parg)
{
    static CCriticalSection cs_Shutdown;
    static bool fTaken;
    bool fFirstThread;
    CRITICAL_BLOCK(cs_Shutdown)
    {
        fFirstThread = !fTaken;
        fTaken = true;
    }
    static bool fExit;
    if (fFirstThread)
    {
        fShutdown = true;
        nTransactionsUpdated++;
        DBFlush(false);
        StopNode();
        DBFlush(true);
        boost::filesystem::remove(GetPidFile());
        UnregisterWallet(pwalletMain);
        delete pwalletMain;
        CreateThread(ExitTimeout, NULL);
        MilliSleep(50);
        printf("namecoin exiting\n\n");
        fExit = true;
#ifndef GUI
        // ensure non-UI client gets exited here, but let Bitcoin-Qt reach 'return 0;' in bitcoin.cpp
        exit(0);
#endif
    }
    else
    {
        while (!fExit)
            MilliSleep(500);
        MilliSleep(100);
        ExitThread(0);
    }
}

void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}






//////////////////////////////////////////////////////////////////////////////
//
// Start
//
#ifndef GUI
int main(int argc, char* argv[])
{
    bool fRet = false;
    fRet = AppInit(argc, argv);

    if (fRet && fDaemon)
        return 0;

    return 1;
}
#endif

bool AppInit(int argc, char* argv[])
{
    bool fRet = false;
    try
    {
        fRet = AppInit2(argc, argv);
    }
    catch (std::exception& e) {
        PrintException(&e, "AppInit()");
    } catch (...) {
        PrintException(NULL, "AppInit()");
    }
    if (!fRet)
        StartShutdown();
    return fRet;
}

bool AppInit2(int argc, char* argv[])
{
#ifdef _MSC_VER
    // Turn off microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, ctrl-c
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifndef __WXMSW__
    umask(077);
#endif
#ifndef __WXMSW__
    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
#endif

    //
    // Parameters
    //
    if (argc >= 0) // GUI sets argc to -1, because it parses the parameters itself
    {
        ParseParameters(argc, argv);

        if (mapArgs.count("-datadir"))
        {
            if (filesystem::is_directory(filesystem::system_complete(mapArgs["-datadir"])))
            {
                filesystem::path pathDataDir = filesystem::system_complete(mapArgs["-datadir"]);
                strlcpy(pszSetDataDir, pathDataDir.string().c_str(), sizeof(pszSetDataDir));
            }
            else
            {
                fprintf(stderr, "Error: Specified directory does not exist\n");
                Shutdown(NULL);
            }
        }
    }

    // Set testnet flag first to determine the default datadir correctly
    fTestNet = GetBoolArg("-testnet");

    ReadConfigFile(mapArgs, mapMultiArgs); // Must be done after processing datadir

    // Note: at this point the default datadir may change, so the user either must not provide -testnet in the .conf file
    // or provide -datadir explicitly on the command line
    fTestNet = GetBoolArg("-testnet");

    if (mapArgs.count("-?") || mapArgs.count("--help"))
    {
        string strUsage = string() +
          _("namecoin version") + " " + FormatFullVersion() + "\n\n" +
          _("Usage:") + "\t\t\t\t\t\t\t\t\t\t\n" +
            "  namecoin [options]                   \t  " + "\n" +
            "  namecoin [options] <command> [params]\t  " + _("Send command to -server or namecoind") + "\n" +
            "  namecoin [options] help              \t\t  " + _("List commands") + "\n" +
            "  namecoin [options] help <command>    \t\t  " + _("Get help for a command") + "\n";

        strUsage += "\n" + HelpMessage();

#if defined(__WXMSW__) && defined(GUI)
        // Tabs make the columns line up in the message box
        wxMessageBox(strUsage, "Namecoin", wxOK);
#else
        // Remove tabs
        strUsage.erase(std::remove(strUsage.begin(), strUsage.end(), '\t'), strUsage.end());
        fprintf(stderr, "%s", strUsage.c_str());
#endif
        return false;
    }

    fDebug = GetBoolArg("-debug");
    fAllowDNS = GetBoolArg("-dns");

#if !defined(WIN32) && !defined(QT_GUI)
    fDaemon = GetBoolArg("-daemon");
#else
    fDaemon = false;
#endif

    if (fDaemon)
        fServer = true;
    else
        fServer = GetBoolArg("-server");

    /* force fServer when running without GUI */
#ifndef GUI
    fServer = true;
#endif

    fPrintToConsole = GetBoolArg("-printtoconsole");
    fPrintToDebugger = GetBoolArg("-printtodebugger");

    fNoListen = GetBoolArg("-nolisten");
    fLogTimestamps = GetBoolArg("-logtimestamps");

    for (int i = 1; i < argc; i++)
        if (!IsSwitchChar(argv[i][0]))
            fCommandLine = true;

    if (fCommandLine)
    {
        int ret = CommandLineRPC(argc, argv);
        exit(ret);
    }

#ifndef __WXMSW__
    if (fDaemon)
    {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0)
        {
            CreatePidFile(GetPidFile(), pid);
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0)
            fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
    }
#endif

    if (!fDebug && !pszSetDataDir[0])
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("namecoin version %s\n", FormatFullVersion().c_str());
//#ifdef GUI
#if 0
    printf("OS version %s\n", ((string)wxGetOsDescription()).c_str());
    printf("System default language is %d %s\n", g_locale.GetSystemLanguage(), ((string)g_locale.GetSysName()).c_str());
    printf("Language file %s (%s)\n", (string("locale/") + (string)g_locale.GetCanonicalName() + "/LC_MESSAGES/bitcoin.mo").c_str(), ((string)g_locale.GetLocale()).c_str());
#endif
    printf("Default data directory %s\n", GetDefaultDataDir().c_str());

    if (GetBoolArg("-loadblockindextest"))
    {
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    /* Debugging feature:  Read a BDB database file and print out some
       statistics about which keys it contains and how much data they
       use up in the file.  */
    if (GetBoolArg ("-dbstats"))
      {
        const std::string dbfile = GetArg ("-dbstatsfile", "blkindex.dat");
        printf ("Database storage stats for '%s' requested.\n",
                dbfile.c_str ());
        CDB::PrintStorageStats (dbfile);
        return true;
      }

    //
    // Limit to single instance per user
    // Required to protect the database files if we're going to keep deleting log.*
    //
//#if defined(__WXMSW__) && defined(GUI)
#if 0
    // wxSingleInstanceChecker doesn't work on Linux
    wxString strMutexName = wxString("bitcoin_running.") + getenv("HOMEPATH");
    for (int i = 0; i < strMutexName.size(); i++)
        if (!isalnum(strMutexName[i]))
            strMutexName[i] = '.';
    wxSingleInstanceChecker* psingleinstancechecker = new wxSingleInstanceChecker(strMutexName);
    if (psingleinstancechecker->IsAnotherRunning())
    {
        printf("Existing instance found\n");
        unsigned int nStart = GetTime();
        loop
        {
            // Show the previous instance and exit
            HWND hwndPrev = FindWindowA("wxWindowClassNR", "Namecoin");
            if (hwndPrev)
            {
                if (IsIconic(hwndPrev))
                    ShowWindow(hwndPrev, SW_RESTORE);
                SetForegroundWindow(hwndPrev);
                return false;
            }

            if (GetTime() > nStart + 60)
                return false;

            // Resume this instance if the other exits
            delete psingleinstancechecker;
            MilliSleep(1000);
            psingleinstancechecker = new wxSingleInstanceChecker(strMutexName);
            if (!psingleinstancechecker->IsAnotherRunning())
                break;
        }
    }
#endif

    // Make sure only a single bitcoin process is using the data directory.
    string strLockFile = GetDataDir() + "/.lock";
    FILE* file = fopen(strLockFile.c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(strLockFile.c_str());
    if (!lock.try_lock())
    {
        wxMessageBox(strprintf(_("Cannot obtain a lock on data directory %s.  namecoin is probably already running."), GetDataDir().c_str()), "Namecoin");
        return false;
    }

    // Bind to the port early so we can tell if another instance is already running.
    string strErrors;
    if (!fNoListen)
    {
        if (!BindListenPort(strErrors))
        {
            wxMessageBox(strErrors, "Namecoin");
            return false;
        }
    }

    hooks = InitHook();

    //
    // Load data files
    //
    if (fDaemon)
        fprintf(stdout, "namecoin server starting\n");
    strErrors = "";
    int64 nStart;

    printf("Loading addresses...\n");
    nStart = GetTimeMillis();
    if (!LoadAddresses())
        strErrors += _("Error loading addr.dat      \n");
    printf(" addresses   %15"PRI64d"ms\n", GetTimeMillis() - nStart);

    /* See if the name index exists and create at least the database file
       if not.  This is necessary so that DatabaseSet can be used without
       failing due to a missing file in LoadBlockIndex.  */
    bool needNameRescan = false;
    {
      filesystem::path nmindex, nmindex_old;
      nmindex_old = filesystem::path (GetDataDir ()) / "nameindexfull.dat";
      nmindex = filesystem::path (GetDataDir ()) / "nameindex.dat";

      if (filesystem::exists (nmindex_old))
        {
          /* If the old file exists, delete it and rescan.  Also delete the
             new file in this case if it exists, as it could be the one from
             a much older version.  */
          filesystem::remove (nmindex_old);
          if (filesystem::exists (nmindex))
            filesystem::remove (nmindex);
          needNameRescan = true;
        }
      else if (!filesystem::exists (nmindex))
        needNameRescan = true;

      CNameDB dbName("cr+");
    }

    printf("Loading block index...\n");
    nStart = GetTimeMillis();
    if (!LoadBlockIndex())
        strErrors += _("Error loading blkindex.dat      \n");
    printf(" block index %15"PRI64d"ms\n", GetTimeMillis() - nStart);

    printf("Loading wallet...\n");
    nStart = GetTimeMillis();
    bool fFirstRun;
    string argWalletPath = GetArg("-walletpath", "wallet.dat");
    boost::filesystem::path pathWalletFile(argWalletPath);
    if (!pathWalletFile.is_complete())pathWalletFile = boost::filesystem::path(GetDataDir()) / pathWalletFile;
    walletPath = new char[pathWalletFile.string().size() + 1];
    strcpy(walletPath, pathWalletFile.string().c_str());
    
    pwalletMain = new CWallet(walletPath);
    if (!pwalletMain->LoadWallet(fFirstRun))
    {
        if(argWalletPath=="wallet.dat")
            strErrors += _("Error loading wallet.dat      \n");
        else
            strErrors += "Error loading " + argWalletPath + "      \n";
    }
    printf(" wallet      %15"PRI64d"ms\n", GetTimeMillis() - nStart);

    RegisterWallet(pwalletMain);

    /* Rescan for name index now if we need to do it.  */
    if (needNameRescan)
      rescanfornames ();

    // Read -mininput before -rescan, otherwise rescan will skip transactions
    // lower than the default mininput
    if (mapArgs.count("-mininput"))
    {
        if (!ParseMoney(mapArgs["-mininput"], nMinimumInputValue))
        {
            wxMessageBox(_("Invalid amount for -mininput=<amount>"), "Namecoin");
            return false;
        }
    }

    CBlockIndex *pindexRescan = pindexBest;
    if (GetBoolArg("-rescan"))
        pindexRescan = pindexGenesisBlock;
    else
    {
        CWalletDB walletdb(walletPath);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex();
    }
    if (pindexBest != pindexRescan)
    {
        printf("Rescanning last %i blocks (from block %i)...\n", pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        pwalletMain->ScanForWalletTransactions(pindexRescan, true);
        printf(" rescan      %15"PRI64d"ms\n", GetTimeMillis() - nStart);
    }

    printf("Done loading\n");

    //// debug print
    printf("mapBlockIndex.size() = %d\n",   mapBlockIndex.size());
    printf("nBestHeight = %d\n",            nBestHeight);
    pwalletMain->DebugPrint();
    printf("setKeyPool.size() = %d\n",      pwalletMain->setKeyPool.size());
    printf("mapPubKeys.size() = %d\n",      pwalletMain->mapPubKeys.size());
    printf("mapWallet.size() = %d\n",       pwalletMain->mapWallet.size());
    printf("mapAddressBook.size() = %d\n",  pwalletMain->mapAddressBook.size());

    if (!strErrors.empty())
    {
        wxMessageBox(strErrors, "Namecoin", wxOK | wxICON_ERROR);
        return false;
    }

    // Add wallet transactions that aren't already in a block to mapTransactions
    pwalletMain->ReacceptWalletTransactions();

    //
    // Parameters
    //
    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (mapArgs.count("-timeout"))
    {
        int nNewTimeout = GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                block.ReadFromDisk(pindex);
                block.BuildMerkleTree();
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    fGenerateBitcoins = GetBoolArg("-gen");

    if (mapArgs.count("-proxy"))
    {
        fUseProxy = true;
        addrProxy = CAddress(mapArgs["-proxy"]);
        if (!addrProxy.IsValid())
        {
            wxMessageBox(_("Invalid -proxy address"), "Namecoin");
            return false;
        }
    }

    if (mapArgs.count("-addnode"))
    {
        BOOST_FOREACH(string strAddr, mapMultiArgs["-addnode"])
        {
            CAddress addr(strAddr, fAllowDNS);
            addr.nTime = 0; // so it won't relay unless successfully connected
            if (addr.IsValid())
                AddAddress(addr);
        }
    }

    if (GetBoolArg("-nodnsseed"))
        printf("DNS seeding disabled\n");
    else
        DNSAddressSeed();

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee))
        {
            wxMessageBox(_("Invalid amount for -paytxfee=<amount>"), "Namecoin");
            return false;
        }
        if (nTransactionFee > 0.25 * COIN)
            wxMessageBox(_("Warning: -paytxfee is set very high.  This is the transaction fee you will pay if you send a transaction."), "Namecoin", wxOK | wxICON_EXCLAMATION);
    }

    if (fHaveUPnP)
    {
#if USE_UPNP
    if (GetBoolArg("-noupnp"))
        fUseUPnP = false;
#else
    if (GetBoolArg("-upnp"))
        fUseUPnP = true;
#endif
    }

    //
    // Create the main window and start the node
    //
#ifdef GUI
    if (!fDaemon)
        CreateMainWindow();
#endif

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    if (!CreateThread(StartNode, NULL))
        wxMessageBox("Error: CreateThread(StartNode) failed", "Namecoin");

    if (fServer)
        CreateThread(ThreadRPCServer, NULL);

#if defined(__WXMSW__) && defined(GUI)
    if (fFirstRun)
        SetStartOnSystemStartup(true);
#endif

#ifndef GUI
    while (1)
        MilliSleep(5000);
#endif

    return true;
}

// Core-specific options shared between UI and daemon
std::string HelpMessage()
{
    std::string strUsage = std::string(_("Options:\n")) +
        "  -conf=<file>     \t\t  " + _("Specify configuration file (default: namecoin.conf)\n") +
        "  -pid=<file>      \t\t  " + _("Specify pid file (default: namecoind.pid)\n") +
        "  -walletpath=<file> \t  " + _("Specify the wallet filename (default: wallet.dat)") + "\n" +
        "  -gen             \t\t  " + _("Generate coins\n") +
        "  -gen=0           \t\t  " + _("Don't generate coins\n") +
        "  -min             \t\t  " + _("Start minimized\n") +
        "  -datadir=<dir>   \t\t  " + _("Specify data directory\n") +
        "  -dbcache=<n>     \t\t  " + _("Set database cache size in megabytes (default: 25)") + "\n" +
        "  -timeout=<n>     \t  "   + _("Specify connection timeout (in milliseconds)\n") +
        "  -proxy=<ip:port> \t  "   + _("Connect through socks4 proxy\n") +
        "  -dns             \t  "   + _("Allow DNS lookups for addnode and connect\n") +
        "  -addnode=<ip>    \t  "   + _("Add a node to connect to\n") +
        "  -connect=<ip>    \t\t  " + _("Connect only to the specified node\n") +
        "  -nolisten        \t  "   + _("Don't accept connections from outside\n") +
#ifdef USE_UPNP
#if USE_UPNP
        "  -noupnp          \t  "   + _("Don't attempt to use UPnP to map the listening port\n") +
#else
        "  -upnp            \t  "   + _("Attempt to use UPnP to map the listening port\n") +
#endif
#endif
        "  -paytxfee=<amt>  \t  "   + _("Fee per KB to add to transactions you send\n") +
        "  -mininput=<amt>  \t  "   + _("When creating transactions, ignore inputs with value less than this (default: 0.0001)\n") +
#ifdef GUI
        "  -server          \t\t  " + _("Accept command line and JSON-RPC commands\n") +
#endif
#if !defined(WIN32) && !defined(QT_GUI)
        "  -daemon          \t\t  " + _("Run in the background as a daemon and accept commands\n") +
#endif
        "  -testnet         \t\t  " + _("Use the test network\n") +
        "  -rpcuser=<user>  \t  "   + _("Username for JSON-RPC connections\n") +
        "  -rpcpassword=<pw>\t  "   + _("Password for JSON-RPC connections\n") +
        "  -rpcport=<port>  \t\t  " + _("Listen for JSON-RPC connections on <port> (default: 8336)\n") +
        "  -rpcallowip=<ip> \t\t  " + _("Allow JSON-RPC connections from specified IP address\n") +
        "  -rpcconnect=<ip> \t  "   + _("Send commands to node running on <ip> (default: 127.0.0.1)\n") +
        "  -keypool=<n>     \t  "   + _("Set key pool size to <n> (default: 100)\n") +
        "  -rescan          \t  "   + _("Rescan the block chain for missing wallet transactions\n");

#ifdef USE_SSL
    strUsage += std::string() +
        _("\nSSL options: (see the namecoin Wiki for SSL setup instructions)\n") +
        "  -rpcssl                                \t  " + _("Use OpenSSL (https) for JSON-RPC connections\n") +
        "  -rpcsslcertificatechainfile=<file.cert>\t  " + _("Server certificate file (default: server.cert)\n") +
        "  -rpcsslprivatekeyfile=<file.pem>       \t  " + _("Server private key (default: server.pem)\n") +
        "  -rpcsslciphers=<ciphers>               \t  " + _("Acceptable ciphers (default: TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!AH:!3DES:@STRENGTH)\n");
#endif

    strUsage += std::string() +
        "  -?               \t\t  " + _("This help message\n");
    return strUsage;
}
