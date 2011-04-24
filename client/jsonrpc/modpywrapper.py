import sys, os
from jsonrpc import ServiceHandler, ServiceException


class ServiceImplementaionNotFound(ServiceException):
    pass


class ModPyServiceHandler(ServiceHandler):
    def __init__(self, req):
        self.req = req
        ServiceHandler.__init__(self, None)


    def findServiceEndpoint(self, name):
        req = self.req

        (modulePath, fileName) = os.path.split(req.filename)
        (moduleName, ext) = os.path.splitext(fileName)
        
        if not os.path.exists(os.path.join(modulePath, moduleName + ".py")):
            raise ServiceImplementaionNotFound()
        else:
            if not modulePath in sys.path:
                sys.path.insert(0, modulePath)

            from mod_python import apache
            module = apache.import_module(moduleName, log=1)
            
            if hasattr(module, "service"):
                self.service = module.service
            elif hasattr(module, "Service"):
                self.service = module.Service()
            else:
                self.service = module

        return ServiceHandler.findServiceEndpoint(self, name)
            
    
    def handleRequest(self, data):
        self.req.content_type = "text/plain"
        data = self.req.read()
        resultData = ServiceHandler.handleRequest(self, data)
        self.req.write(resultData)
        self.req.flush()

def handler(req):
    from mod_python import apache
    ModPyServiceHandler(req).handleRequest(req)
    return apache.OK
    

