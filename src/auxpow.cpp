// Copyright (c) 2011 Vince Durham
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
#include "headers.h"
#include "script.h"
#include "auxpow.h"
#include "init.h"

using namespace std;
using namespace boost;

bool CAuxPow::Check(uint256 hashAuxBlock)
{
    // Check that the chain merkle root is in the coinbase
    uint256 nRootHash = CBlock::CheckMerkleBranch(hashAuxBlock, vChainMerkleBranch, nChainIndex);
    vector<unsigned char> vchRootHash(nRootHash.begin(), nRootHash.end());
    std::reverse(vchRootHash.begin(), vchRootHash.end()); // correct endian

    const CScript script = vin[0].scriptSig;

    if (std::search(script.begin(), script.end(), vchRootHash.begin(), vchRootHash.end()) ==
            script.end()
            )
        return false;

    // Check that we are in the parent block merkle tree
    if (CBlock::CheckMerkleBranch(GetHash(), vMerkleBranch, nIndex) != parentBlock.hashMerkleRoot)
        return false;

    return true;
}
