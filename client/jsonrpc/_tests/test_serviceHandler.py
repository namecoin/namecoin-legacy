
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

    def not_a_serviceMethod(self):
        pass
    
    @jsonrpc.ServiceMethod
    def raiseError(self):
        raise Exception("foobar")

class Handler(jsonrpc.ServiceHandler):
    def __init__(self, service):
        self.service=service

    def translateRequest(self, data):
        self._requestTranslated=True
        return jsonrpc.ServiceHandler.translateRequest(self, data)
    
    def findServiceEndpoint(self, name):
        self._foundServiceEndpoint=True
        return jsonrpc.ServiceHandler.findServiceEndpoint(self, name)

    def invokeServiceEndpoint(self, meth, params):
        self._invokedEndpoint=True
        return jsonrpc.ServiceHandler.invokeServiceEndpoint(self, meth, params)

    def translateResult(self, result, error, id_):
        self._resultTranslated=True
        return jsonrpc.ServiceHandler.translateResult(self, result, error,  id_)



class  TestServiceHandler(unittest.TestCase):

    def setUp(self):
        self.service = Service()
        
    def tearDown(self):
        pass

    def test_RequestProcessing(self):
        handler = Handler(self.service)
        json=jsonrpc.dumps({"method":"echo", 'params':['foobar'], 'id':''})
        
        result  = handler.handleRequest(json)
        self.assert_(handler._requestTranslated)
        self.assert_(handler._foundServiceEndpoint)
        self.assert_(handler._invokedEndpoint)
        self.assert_(handler._resultTranslated)

    def test_translateRequest(self):
        handler = Handler(self.service)
        json=jsonrpc.dumps({"method":"echo", 'params':['foobar'], 'id':''})
        req = handler.translateRequest(json)
        self.assertEquals(req['method'], "echo")
        self.assertEquals(req['params'],['foobar'])
        self.assertEquals(req['id'],'')

    def test_findServiceEndpoint(self):
        handler = Handler(self.service)
        self.assertRaises(jsonrpc.ServiceMethodNotFound, handler.findServiceEndpoint, "notfound")
        self.assertRaises(jsonrpc.ServiceMethodNotFound, handler.findServiceEndpoint, "not_a_serviceMethod")
        meth = handler.findServiceEndpoint("echo")
        self.assertEquals(self.service.echo, meth)

    def test_invokeEndpoint(self):
        handler = Handler(self.service)
        meth = handler.findServiceEndpoint("echo")
        rslt = handler.invokeServiceEndpoint(meth, ['spam'])
        self.assertEquals(rslt, 'spam')

    def test_translateResults(self):
        handler=Handler(self.service)
        data=handler.translateResult("foobar", None,  "spam")
        self.assertEquals(jsonrpc.loads(data), {"result":"foobar","id":"spam","error":None})

    def test_translateError(self):
        handler=Handler(self.service)
        exc = Exception()
        data=handler.translateResult(None, exc, "id")
        self.assertEquals(jsonrpc.loads(data), {"result":None,"id":"id","error":{"name":"Exception", "message":""}})

    def test_translateUnencodableResults(self):
        handler=Handler(self.service)
        data=handler.translateResult(self, None, "spam")
        self.assertEquals(jsonrpc.loads(data), {"result":None,"id":"spam","error":{"name":"JSONEncodeException", "message":"Result Object Not Serializable"}})

    def test_handleRequestEcho(self):
        handler=Handler(self.service)
        json=jsonrpc.dumps({"method":"echo", 'params':['foobar'], 'id':''})
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), jsonrpc.loads('{"result":"foobar", "error":null, "id":""}'))

    def test_handleRequestMethodNotFound(self):
        handler=Handler(self.service)
        json=jsonrpc.dumps({"method":"not_found", 'params':['foobar'], 'id':''})
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), {"result":None, "error":{"name":"ServiceMethodNotFound", "message":""}, "id":""})

    def test_handleRequestMethodNotAllowed(self):
        handler=Handler(self.service)
        json=jsonrpc.dumps({"method":"not_a_ServiceMethod", 'params':['foobar'], 'id':''})
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), {"result":None, "error":{"name":"ServiceMethodNotFound", "message":""}, "id":""})

    def test_handleRequestMethodRaiseError(self):
        handler=Handler(self.service)
        json=jsonrpc.dumps({"method":"raiseError", 'params':[], 'id':''})
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), {"result":None, "error":{"name":"Exception", "message":"foobar"}, "id":""})

    def test_handleBadRequestData(self):
        handler=Handler(self.service)
        json = "This is not a JSON-RPC request"
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), {"result":None, "error":{"name":"ServiceRequestNotTranslatable", "message":json}, "id":""})

    def test_handleBadRequestObject(self):
        handler=Handler(self.service)
        json = "{}"
        result = handler.handleRequest(json)
        self.assertEquals(jsonrpc.loads(result), {"result":None, "error":{"name":"BadServiceRequest", "message":json}, "id":""})
