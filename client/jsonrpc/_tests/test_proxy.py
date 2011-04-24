
"""
  Copyright (c) 2007 Jan-Klaas Kollhof

  This file is part of jsonrpc.

  jsonrpc is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this software; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

import unittest
import jsonrpc

import urllib

from StringIO import StringIO

class  TestProxy(unittest.TestCase):

    def urlopen(self, url, data):
        self.postdata = data
        return StringIO(self.respdata) 
    
    def setUp(self):
        self.postdata=""
        self.urllib_openurl = urllib.urlopen
        urllib.urlopen = self.urlopen
        
    def tearDown(self):
        urllib.urlopen = self.urllib_openurl

    def test_ProvidesProxyMethod(self):
        s = jsonrpc.ServiceProxy("http://localhost/")
        self.assert_(callable(s.echo))

    def test_MethodCallCallsService(self):
        
        s = jsonrpc.ServiceProxy("http://localhost/")

        self.respdata='{"result":"foobar","error":null,"id":""}'
        echo = s.echo("foobar")
        self.assertEquals(self.postdata, jsonrpc.dumps({"method":"echo", 'params':['foobar'], 'id':'jsonrpc'}))
        self.assertEquals(echo, 'foobar')

        self.respdata='{"result":null,"error":"MethodNotFound","id":""}'
        try:
            s.echo("foobar")
        except jsonrpc.JSONRPCException,e:
            self.assertEquals(e.error, "MethodNotFound")
            