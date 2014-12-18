v0.3.80
======
* Various stricter checks, some small modifications and cleanup to harmonize
behavior in preparation for the rebase of Namecoin on the latest Bitcoin version.
Changes will take full effect by block height 212500. This is a softfork, miners
must update, regular users should update! (domob)
* fix locale bugs (itoffshore)

v0.3.76rc1
========
* Set default fee per kb to 0.005NMC (phelix)
* Several optimizations to improve handling of large/many network transactions (Domob)
* Only accept finished transactions (phelix)
* Relay transaction size limited to 20kb (consensus/phelix)
* Increased network relay fee MIN_RELAY_TX_FEE to 100000 (RyanC/Indolering/phelix)
* More restrictive filtering of transactions
* Update to OpenSSL1.0.1i (phelix)
* Better drive performance on disk based systems through less fragmentation (Domob)
* "Renew" GUI Button (Domob)
* contrib/easywinbuilder: Qt5/MinGW4.8.2/cleanup (phelix)
* Qt5 compatibility (Canercandan/phelix)
* New command line / .conf file option: -walletpath=customwalletfilename.dat (digital-dreamer)
* "Pay To" in the Qt can be used to send coins also to a name, like "sendtoname" (Domob)
* New RPC block info: height, confirmations, chainwork, nextblockhash. Change: No previousblockhash for block 0 (RyanC)
* The RPC interface now returns an error while initialising, instead of not accepting connections at all (Domob)

v0.3.75
=======
* Add difficulty to RPC block output JSON (Domob)
* Bitcoin port: skip signature verification on blocks before last checkpoint (phelix)
* New checkpoint at 182000
* Update to OpenSSL1.0.1h (security fix for SSL http RPC)
* Czech localization (digital-dreamer)
* Windows installer script for Innsetup (phelix)
* Enforce value length of 520 characters in RPC and Qt (Domob)
* New command line argument -dbstats runs a DB file storage statistics analysis and prints it to the debug log. (Domob)
* Atomic handling of TxDB/NameDB operations, DB code cleanup and optimization. (Domob)
* Even smaller blkindex.dat. Not backward compatible. It will take a while for the rewrite on the first start. (Domob)

v0.3.74-rc1 (never officially released)
=======================================
* allow for atomic name transactions via rpc commands (Domob)
* new rpc commands: name_pending, getchains (Domob)
* Simplified blkindex.dat for smaller file size and faster startup - not backward compatible ("remove auxpow", Domob) - IT WILL TAKE A WHILE FOR THE REWRITE ON THE FIRST START
* Implement name_update in createrawtransaction (Domob)
* More detailed JSON outputs for decoderawtransaction and getrawtransaction (Domob)
* Async RPC calls (Domob: ported from Huntercoin)
* Add toaddress argument to name_firstupdate (Domob)
* fixed memory leaks (Domob)
* listsinceblock (Olgasanko)
* Several performance optimizations (Domob)
* valgrind script (Domob)
* Small updates and fixes, code cleanup

v0.3.73-rc1 (never officially released)
=======================================
* Modified testnet difficulty calculation (Khal)
* GUI: ID-tab (Domob)
* improve name_filter speed (Khal)
* Small updates and fixes
