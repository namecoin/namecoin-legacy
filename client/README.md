ncproxy
==============

A SOCKS5 adapter that provides namecoin name resolution.

This program listens for SOCKS5 connections, resolves .bit DNS names if any
and passes the request to a parent SOCKS5 proxy.  It can be used between polipo
and Tor.

HowTo
=============

This assumes namecoin is running on the local machine with the default RPC port, and Tor and polipo are installed.

Run ncproxy with RPC user and password arguments for namecoin:

`./ncproxy --user=USER --pass=PASS`

The ncproxy script listens on port 9055, so configure /etc/polipo/config as follows:

`socksParentProxy = "localhost:9055"`

Make sure that there is just one such statement.  Then do

`sudo service polipo restart`

Check that your browser is already configured to use polipo.  For example, Torbutton does this configuration for you.  .bit should now resolve correctly since polipo will pass all requests through ncproxy.  ncproxy also resolves .b and .n if you prefer the shorthand and the target web site supports it.
