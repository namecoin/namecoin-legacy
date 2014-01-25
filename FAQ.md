General
===============

**Q: What is the relationship of this project to Bitcoin?**

**A:** The code is based on Bitcoin.  The block chain is separate, starting from a different genesis block and therefore creating a separate currency - Namecoins.  This approach was recommended during the BitDNS discussion on the forum.

**Q: How much does it cost to register a domain (a.k.a. a name)?**

**A:** The cost includes a network fee and a transaction fee.  The fees are denominated in Namecoins (NC).  Initially, the network fee is 50 NC.  The network fee decreases 2x every 2 months, which means it will be less than one NC in a year.

**Q: How do I obtain Namecoins?**

**A:** You have to get them by mining, in the usual Bitcoin way, or by obtaining them from someone else.  For example, you could find someone willing to sell NC for BTC.

**Q: Who gets the network fee?**

**A:** The network fee Namecoins are destroyed by the transaction.  Nobody gets them.

**Q: Who gets the transaction fee?**

**A:** The miners do, just like in Bitcoin.  Initially a reasonable transaction fee is either 0.00 or 0.01 NC.  You can choose this based on how fast you want your transaction to be processed.

**Q: How long are names good for?**

**A:** You have to execute an update on a name every 12000 blocks (normally about three months), or it expires.  There is no network fee for updates.

**Q: Do I have to pay renewal fees?**

**A:** No, only Bitcoin-like transaction fees, currently 0.00 or 0.01 NC

Usage
===============

**Q: How do I run Namecoin?**

**A:** Currently you have to download and compile the software yourself.  You then run the namecoind executable.  Configuration is in ~/.namecoin/bitcoin.conf .

**Q: Can I run Namecoin and Bitcoin on the same machine?**

**A:** This is possible.  The two systems use different port numbers for client to client communication.  You have to choose a different RPC port for one of them, because they both default to 8332.

**Q: How do I register a name?**

**A:** You must have enough balance to pay the network fee and transaction fee.  Then do:

`namecoind name_new <name>`

This will output two values - a random number (short) and a transaction ID (longer).

**wait 12 blocks**, then:

`namecoind name_firstupdate <name> <random> <value>`

**Q: How do I make my browser recognize the .bit top level domain?**

**A:** _These are not yet implemented, but will be soon!_  Install the relevant plugin for your browser, or use a suitable proxy.

**Q: I mined 50 NC.  Can I register a name now?**

**A:** You have to wait 120 blocks (normally 20 hours) for the NC to mature, in the usual Bitcoin fashion.

**Q: Why is there a separate `name_new` step?**

**A:** This is to prevent others from stealing your new name by registering it quickly themselves when they see your transaction.  The name is not broadcast, only an encrypted version.  There is a mandatory 12 block wait that gives you enough time to broadcast your name with `first_update`, reducing the chance that someone will get in a `first_update` ahead of you.

**Q: How do I list registered names?**

**A:** Use `namecoind name_list` to see your own names, and `namecoind name_scan` for the global list.

**Q: How do I transfer a name?**

**A:** This is not yet in the code, but you would basically do an update with a recipient's wallet address as the destination

**Q: This is complicated, isn't there a simpler interface?**

**A:** A friendly user interface is planned.

Design
========

**Q: Why is there a network fee?**

**A:** The network fee is initially high, but will be negligible after a couple of years.  It is used to slow down the initial registration rate so that plenty of desirable names are left for late adopters.

**Q: How are names represented?**

**A:** Names and values are attached to special coins with a value of 0.01 NC .  Updates are performed by creating a transaction with the name's previous coin as input.

**Q: What if I spend that special coin by mistake?**

**A:** The code prevents those coins from being used for normal payments.

Mining
========

**Q: Can I use existing Bitcoin miners?**

**A:** Yes

**Q: I have to choose whether to mine Namecoin or Bitcoin?**

**A:** There is a plan to create a cross-miner that will be able to mine multiple chains with the same hash power.
