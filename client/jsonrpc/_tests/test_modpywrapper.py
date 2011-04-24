
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
from types import *

class Service(object):
    @jsonrpc.ServiceMethod
    def echo(self, arg):
        return arg


class ApacheRequestMockup(object):

    def __init__(self, filename, fin, fout):
        self.fin=fin
        self.fout = fout
        self.filename = filename
        
    def write(self,data):
        self.fout.write(data)

    def flush(self):
        pass
    
    def read(self):
        return self.fin.read()

class ModPyMockup(object):
    def __init__(self):
        self.apache=ApacheModuleMockup()

class ApacheModuleMockup(object):
    def __getattr__(self, name):
        return name
    
    def import_module(self, moduleName, log=1):
        return Service()


    
class  TestModPyWrapper(unittest.TestCase):

    def setUp(self):
        import sys
        sys.modules['mod_python']  =ModPyMockup()
        
    def tearDown(self):
        pass

    def test_runHandler(self):
        from StringIO import StringIO
       
        json=u'{"method":"echo","params":["foobar"], "id":""}'
        fin=StringIO(json)
        fout=StringIO()
        req = ApacheRequestMockup(__file__ , fin, fout)

        jsonrpc.handler(req)

        data = fout.getvalue()

        self.assertEquals(jsonrpc.loads(data), {"result":"foobar", "error":None, "id":""})

    def test_ServiceImplementationNotFound(self):
        from StringIO import StringIO
       
        json=u'{"method":"echo","params":["foobar"], "id":""}'
        fin=StringIO(json)
        fout=StringIO()
        req = ApacheRequestMockup("foobar" , fin, fout)

        rslt = jsonrpc.handler(req)
        self.assertEquals(rslt, "OK")
        data = fout.getvalue()

        self.assertEquals(jsonrpc.loads(data), {u'id': '', u'result': None, u'error': {u'message': '', u'name': u'ServiceImplementaionNotFound'}} )

        

