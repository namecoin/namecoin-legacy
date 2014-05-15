v0.3.74
======
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
============================
* Modified testnet difficulty calculation (Khal)
* GUI: ID-tab (Domob)
* improve name_filter speed (Khal)
* Small updates and fixes
