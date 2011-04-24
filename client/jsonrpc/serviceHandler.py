
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

from jsonrpc import loads, dumps, JSONEncodeException


def ServiceMethod(fn):
    fn.IsServiceMethod = True
    return fn

class ServiceException(Exception):
    pass

class ServiceRequestNotTranslatable(ServiceException):
    pass

class BadServiceRequest(ServiceException):
    pass

class ServiceMethodNotFound(ServiceException):
    def __init__(self, name):
        self.methodName=name

class ServiceHandler(object):

    def __init__(self, service):
        self.service=service
    
    def handleRequest(self, json):
        err=None
        result = None
        id_=''
        
        try:
            req = self.translateRequest(json)
        except ServiceRequestNotTranslatable, e:
            err = e
            req={'id':id_}

        if err==None:
            try:
                id_ = req['id']
                methName = req['method']
                args = req['params']
            except:
                err = BadServiceRequest(json)
                
        if err == None:
            try:
                meth = self.findServiceEndpoint(methName)
            except Exception, e:
                err = e

        if err == None:
            try:
                result = self.invokeServiceEndpoint(meth, args)
            except Exception, e:
                err = e

        resultdata = self.translateResult(result, err, id_)

        return resultdata

    def translateRequest(self, data):
        try:
            req = loads(data)
        except:
            raise ServiceRequestNotTranslatable(data)
        return req
     
    def findServiceEndpoint(self, name):
        try:
            meth = getattr(self.service, name)
            if getattr(meth, "IsServiceMethod"):
                return meth
            else:
                raise ServiceMethodNotFound(name)
        except AttributeError:
            raise ServiceMethodNotFound(name)

    def invokeServiceEndpoint(self, meth, args):
        return meth(*args)

    def translateResult(self, rslt, err, id_):
        if err != None:
            err = {"name": err.__class__.__name__, "message":err.message}
            rslt = None

        try:
            data = dumps({"result":rslt,"id":id_,"error":err})
        except JSONEncodeException, e:
            err = {"name": "JSONEncodeException", "message":"Result Object Not Serializable"}
            data = dumps({"result":None, "id":id_,"error":err})
            
        return data