# Namecoin Design

## Key Operations

Key operations are performed by transactions with version 0x9900 - 0x99ff

* name\_new(hash(rand, name), value)
* name\_firstupdate(name, rand, value)
* name\_update(name, value)

The last is a normal bitcoin-like transaction that does not affect the name.

## Method

A transaction can have a name operation associated with it.  An operation can reserve (name\_new), initialize (name\_firstupdate) or update (name\_update) a name/value pair.

The name\_firstupdate transaction has a network fee.  The network fees represents namecoins (NC) that are destroyed.  This is in addition to the bitcoin-like miner transaction fees.

## Key Operations Detail

* The pubkey script starts with the following constants (OP\_N or OP\_PUSHDATA), followed by a OP_DROP, OP_2DROP or OP_NOP:
  * name\_new: [NAME\_NEW, hash(rand, name), value]
  * name\_firstupdate: [NAME\_FIRSTUPDATE, name, [newtx\_hash,] rand, value]
  * name\_update: [NAME\_UPDATE, name, value]
* name\_firstupdate will be accepted at or after 12 blocks of the matching name\_new
* in case of a collision, the first non-ignore name\_new wins
* new names are valid after name\_firstupdate
* name is a byte array of max length 255
* value is a byte array of max length 1023
* The transaction can only be inserted into a block where the current network fee is less than the network fee output of the transaction
* a name expires 36000 blocks from the last operation

## Network fees

The purpose of the network fees is to slow down the initial gold-rush.

* Network fees start out at 50 NC per operation at the genesis block
* Every block, the network fees decreases based on this algorithm, in 1e-8 NC:
  * res = 500000000 >> floor(nBlock / 8192)
  * res = res - (res >> 14)*(nBlock % 8192)
* nBlock is zero at the genesis block
* This is a decrease of 50% every 8192 blocks (about two months)
* As 50 NC are generated per block, the maximum number of registrations in the first 8192 blocks is therefore 2/3 of 8192, which is 5461
* Difficulty starts at 512

## Validation

A name operation transaction can be inserted in a block if the following validations pass:

* normal bitcoin validations pass
* if the transaction version does not indicate namecoin, no inputs can be namecoin outputs (i.e. have namecoin transaction id and have a namecoin script)
* if the transaction version does not indicate namecoin, terminate with success
* one of the outputs is >= the network fee of the block and has the script: OP\_RETURN (i.e. cannot be used as input, coins are lost)
* if this is an name\_update, exactly one of the inputs is an name\_update or name\_firstupdate on this name and the difference in block numbers is at most 12000.  Also, no other inputs are name operations.
* if this is a name\_firstupdate, exactly one of the inputs is a name\_new with block difference at least 12 but not more than 12000. No other inputs are name operations.
* if this is a name\_new, none of the inputs is a name operation.
* a name\_firstupdate must be on a name never seen or that has expired

## Payment of network fee

One of the outputs of a name\_firstupdate or name\_update transaction is lost (cannot be an input to a further transaction and has a script of exactly "OP_RETURN")

## Applications

The name is normally interpreted as a UTF-8 string composed of several slash-separated substrings.  The first element is a application specifier.

For DNS, the first element shall be "d" and there are exactly two elements.  Mapping into the .bit domain is simply: d/xyz => xyz.bit .  The value is interpreted as a zone specification.  It is recommended that names stop serving 1000 blocks before expiration, as a signal to prevent a forgetful registrant from losing the domain immediately after it expires.

For personal public name, the first element shall be "p" and there are exactly two elements.  The value is interpreted as a json encoded hash.  The "key" element of the cash contains PGP encoded keys.
