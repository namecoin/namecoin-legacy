Merged Mining
===================

Background
==========

Merged mining works by attaching additional information to the auxiliary chain block
to prove that work was done on the parent chain.  The class CAuxPow includes:

* a merkle branch to the root of the auxiliary chain tree (currently empty, only one aux chain is supported)
* an index of this chain in the aux chain list (currently should be zero)
* the coinbase transaction in the parent block that includes the merkle root hash of the auxiliary chains in the coinbase script
* the merkle branch of the coinbase transaction in the parent block
* the parent block header

When a miner finds a parent block that is under an aux chain target, it submits the aux proof of work to the aux chain.  Of course, this required a modification to CBlock::CheckWork in the aux chain to validate.

The validation proceeds as follows:

* compute the merkle root of the aux chain tree
* verify that the merkle root is in the parent coinbase
* verify that the parent coinbase tx is properly attached to the merkle tree of the parent block
* verify that the parent block hash is under the aux target

Mining
======

A python script is provided in contrib/merged-mine-proxy that asks the parent chain and aux chain for work.  The proxy translates these to a backward compatible work that is understood by existing miners and pool software.

When a miner submits a solution, the solution is submitted to both chains.  If the parent block hash is under the parent target, it is accepted in the usual way.  If it is under the aux target, it is accepted as an aux proof of work on that chain.

To deploy, run the satoshi client for each chain, point the proxy at them, then point the mining software at the proxy.

Status
======

Currently acceptance of aux proof of work is disabled on the production bitcoin network and is enabled on the testnet.  Of course, aux POW will be rejected by unpatched clients.

To enable a synchronized upgrade to a chain, a starting block number can be configured in GetAuxPowStartBlock().

Issues
======

* The code as it stands allows multiple aux blocks for the same aux chain to be generated at once if the blocks are put at different positions in the merkle tree.  Unless another solution is found, each aux chain must only accept blocks at a fixed position in the chain list.
