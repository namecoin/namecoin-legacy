Namecoin
===================

Namecoin is a peer to peer naming system based on bitcoin.  It is a secure and cnesorship reistant replacement for DNS.

Ownership of a name is based on ownership of a coin, which is in turn based on public key cryptography.  The namecoin network reaches consensus every few minutes as to which names have been reserved or updated.

It is envisioned that the .bit domain be used for gluing namecoin domain names into the DNS.  This can be done close to the user or even by the user.

See FAQ.md for more general information.

Technical
=====================

The Bitcoin protocol is augmented with namecoin operations, to reserve, register and update names.  In addition to DNS like entries, arbitrary name/value pairs are allowed and multiple namespaces will be available.  This will include a personal handle namespace mapping handles to public keys and personal address data.

The protocol differences from bitcoin include:

* Different blockchain, port, IRC bootstrap and message header
* New transaction types: new, first-update, update
* Validation on the new transaction types
* RPC calls for managing names
* Network fees to slow down the initial rush

Please read DESIGN-namecoind.md before proceeding.

BUILDING
======================

Follow the bitcoin build instructions.  Use "makefile" - it will generate namecoind.  Usage is similar to bitcoind.  There are only RPC calls for the new operations.  A GUI is on the roadmap.

RUNNING
======================

You can acquire namecoins in the usual bitcoin way, by mining or by reciving some from others.  After you have acquired some namecoins, use:

`namecoind name_new d/<name>`

This will reserve a name but not make it visible yet.  Make a note of the short hex number.  You will need it for the next step.  (Do not shut down namecoind or you will also have to supply the longer hex code below.)  Wait about 12 blocks, then issue:

`namecoind name_firstupdate d/<name> <rand> <value>`

`<rand>` is the short hex number from the previous step.  This step will make the name visible to all.

after the first update, you can do more updates with:

`namecoind name_update d/<name> <value>`

and transfer to another person:

`namecoind name_update d/<name> <value> <address>`

dump your list of names:

`namecoind name_list`

dump the global list:

`namecoind name_scan`

VALUES
===================

Values for names in the d/ namespace are JSON encoded.  The simplest value is of this form:

  {'map': {'': '10.0.0.1'}}

which maps all hosts in the domain example.bit to one IP address.  But you can also delegate to your DNS servers:

  {'map': {'': {'ns': ['10.0.0.1', '10.0.0.2']}}}

or even do a translation step before delegation:

  {'map': {'': {'translate': 'bitcoin.org', 'ns': ['10.0.0.1', '10.0.0.2']}}}

in which case, foo.example.bit will be translated to foo.bitcoin.org before it is sent to your DNS servers.

DNS conduits
=============

SOCKS5/Tor name resolver: See `ncproxy` in the client sub-directory.

In the near future gzip encoding of the value will be possible, but ncproxy does not support this yet.

ROADMAP
===================

* DNS zone conduit to allow normal DNS server to serve the .bit domain
* Firefox/chrome/... plugins
* GUI
