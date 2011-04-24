
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



class  TestDumps(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass

    
    def assertJSON(self, json, expectedJSON):
        self.assert_(type(json) is UnicodeType)
        self.assertEqual(json, expectedJSON)
           
    def test_Number(self):
        json = jsonrpc.dumps(1)
        self.assertJSON(json, u'1')
        
        json = jsonrpc.dumps(0xffffffffffffffffffffffff)
        self.assertJSON(json, u'79228162514264337593543950335')

    def test_None(self):
        json = jsonrpc.dumps(None)
        self.assertJSON(json, u'null')
        
    def test_Boolean(self):
        json = jsonrpc.dumps(False)
        self.assertJSON(json, u'false')
        json = jsonrpc.dumps(True)
        self.assertJSON(json, u'true')

    def test_Float(self):
        json = jsonrpc.dumps(1.2345)
        self.assertJSON(json, u'1.2345')

        json =jsonrpc.dumps(1.2345e67)
        self.assertJSON(json, u'1.2345e+67')

        json =jsonrpc.dumps(1.2345e-67)
        self.assertJSON(json, u'1.2345e-67')

    def test_String(self):
        json = jsonrpc.dumps('foobar')
        self.assertJSON(json, u'"foobar"')

        json = jsonrpc.dumps('foobar')
        self.assertJSON(json, u'"foobar"')

    def test_StringEscapedChars(self):
        json = jsonrpc.dumps('\n \f \t \b \r \\ " /')
        self.assertJSON(json, u'"\\n \\f \\t \\b \\r \\\\ \\" \\/"')

    def test_StringEscapedUnicodeChars(self):
        json = jsonrpc.dumps(u'\0 \x19 \x20\u0130')
        self.assertJSON(json, u'"\\u0000 \\u0019  \u0130"')

    def test_Array(self):
        json = jsonrpc.dumps([1, 2.3e45, 'foobar'])
        self.assertJSON(json, u'[1,2.3e+45,"foobar"]')

    def test_Dictionary(self):
        json = jsonrpc.dumps({'foobar':'spam', 'a':[1,2,3]})
        self.assertJSON(json, u'{"a":[1,2,3],"foobar":"spam"}')

    def test_FailOther(self):
        self.failUnlessRaises(jsonrpc.JSONEncodeException, lambda:jsonrpc.dumps(self))

        
        

class  TestLoads(unittest.TestCase):
    def setUp(self):
        pass

    def tearDown(self):
        pass


    def test_String(self):

        json = jsonrpc.dumps("foobar")
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, u"foobar")
    
    def test_StringEscapedChars(self):
        json = '"\\n \\t \\r \\b \\f \\\\ \\/ /"'
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, u'\n \t \r \b \f \\ / /')
        
    def test_StringEscapedUnicodeChars(self):
        json = jsonrpc.dumps(u'\u0000 \u0019')
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, u'\0 \x19')
        
    def test_Array(self):
        json = jsonrpc.dumps(['1', ['2','3']])
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, ['1', ['2','3']])

    def test_Dictionary(self):
        json = jsonrpc.dumps({'foobar':'spam', 'nested':{'a':'b'}})
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, {'foobar':'spam', 'nested':{'a':'b'}})


    def test_Int(self):
        json = jsonrpc.dumps(1234)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, 1234)


    def test_NegativeInt(self):
        json = jsonrpc.dumps(-1234)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, -1234)

    def test_NumberAtEndOfArray(self):
        json = jsonrpc.dumps([-1234])
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, [-1234])

    def test_StrAtEndOfArray(self):
        json = jsonrpc.dumps(['foobar'])
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, ['foobar'])
            
    def test_Float(self):
        json = jsonrpc.dumps(1234.567)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, 1234.567)

    def test_Exponential(self):
        json = jsonrpc.dumps(1234.567e89)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, 1234.567e89)

    def test_True(self):
        json = jsonrpc.dumps(True)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, True)

    def test_False(self):
        json = jsonrpc.dumps(False)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, False)

    def test_None(self):
        json = jsonrpc.dumps(None)
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, None)

    def test_NestedDictAllTypes(self):
        json = jsonrpc.dumps({'s':'foobar', 'int':1234, 'float':1234.567, 'exp':1234.56e78,
                                            'negInt':-1234, 'None':None,'True':True, 'False':False,
                                            'list':[1,2,4,{}], 'dict':{'a':'b'}})
        obj = jsonrpc.loads(json)
        self.assertEquals(obj, {'s':'foobar', 'int':1234, 'float':1234.567, 'exp':1234.56e78,
                                            'negInt':-1234, 'None':None,'True':True, 'False':False,
                                            'list':[1,2,4,{}], 'dict':{'a':'b'}})
