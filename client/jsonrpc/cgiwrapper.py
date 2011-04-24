import sys, os
from jsonrpc import ServiceHandler

class CGIServiceHandler(ServiceHandler):
    def __init__(self, service):
        if service == None:
            import __main__ as service

        ServiceHandler.__init__(self, service)

    def handleRequest(self, fin=None, fout=None, env=None):
        if fin==None:
            fin = sys.stdin
        if fout==None:
            fout = sys.stdout
        if env == None:
            env = os.environ
        
        try:
            contLen=int(env['CONTENT_LENGTH'])
            data = fin.read(contLen)
        except Exception, e:
            data = ""

        resultData = ServiceHandler.handleRequest(self, data)
        
        response = "Content-Type: text/plain\n"
        response += "Content-Length: %d\n\n" % len(resultData)
        response += resultData
        
        #on windows all \n are converted to \r\n if stdout is a terminal and  is not set to binary mode :(
        #this will then cause an incorrect Content-length.
        #I have only experienced this problem with apache on Win so far.
        if sys.platform == "win32":
            try:
                import  msvcrt
                msvcrt.setmode(fout.fileno(), os.O_BINARY)
            except:
                pass
        #put out the response
        fout.write(response)
        fout.flush()

def handleCGI(service=None, fin=None, fout=None, env=None):
    CGIServiceHandler(service).handleRequest(fin, fout, env)