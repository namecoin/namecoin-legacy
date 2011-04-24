import sys
import socket
import select
import threading
import struct
import time
      
class S5Req:
  def __init__(self,buf):
    self.ver, self.cmd, self.rsv, self.atyp = struct.unpack("BBBB", buf[0:4])
    self.dst_addr = None
    self.dst_port = None
    
  def parse_port(self,buf):
    if len(buf) < 2:
      return False
    port = struct.unpack("H",buf[0:2])[0]
    self.dst_port = socket.ntohs(int(port))
    return True
  
  def parse_ipv4(self,buf):
    if len(buf) < 6:
      return False
    self.dst_addr = socket.inet_ntoa(buf[0:4])
    if self.parse_port(buf[4:]):
      return True
    return False
  
  def parse_domain_name(self,buf):
    buf_size = len(buf)
    if buf_size < 1:
      return False
    name_len = struct.unpack("B",buf[0:1])[0]
    if name_len+3 != buf_size:
      return False
    self.dst_addr = buf[1:name_len+1]
    if self.parse_port(buf[1+name_len:]):
      return True
    return False
    
  def parse_netloc(self,buf):
    if self.atyp == 3:
      return self.parse_domain_name(buf)
    if self.atyp == 1:
      return self.parse_ipv4(buf)
    return False

class S5Resp:
  def __init__(self):
    self.ver = 5
    self.rep = 1
    self.rsv = 0
    self.atyp = 1
    self.bnd_addr = None
    self.bnd_port = None
  
  def pack(self):
    addr = 0
    port = 0
    if self.bnd_addr:
      addr = struct.unpack("I",socket.inet_aton(self.bnd_addr))[0]
    if self.bnd_port:
      port = socket.htons(self.bnd_port)
    buf = struct.pack("BBBBIH",self.ver, self.rep, self.rsv, self.atyp,addr,port)
    return buf
  
class Socks5Error(Exception):
  pass

class Socks5Thread(threading.Thread):
  wait = 8.0
  buf_size = 1024*4
  
  def __init__(self,s,ip,port):
    self.s = s
    self.dst_s = None
    self.ip = ip
    self.port = port
    threading.Thread.__init__(self)
      
  def run(self):
    resp = S5Resp()
    try:
      buf = self.s.recv(255)
      if not buf:
        raise socket.error
      
      self.s.send("\x05\x00")
      buf = self.s.recv(4)
      if not buf or len(buf) != 4:
        raise socket.error
      
      req = S5Req(buf)
      if req.ver != 5:
        resp.rep = 1
        raise Socks5Error
      if req.cmd != 1:
        resp.rep = 7
        raise Socks5Error
      if req.atyp != 1 and req.atyp != 3:
        resp.rep = 8
        raise Socks5Error
      
      count = 255
      if req.atyp == 1:
        count = 6
        
      buf = self.s.recv(count)
      if not buf:
        raise socket.error
      
      if not req.parse_netloc(buf):
        resp.rep = 1
        raise Socks5Error
      
      if req.atyp == 3:
        try:
          addr = socket.gethostbyname(req.dst_addr)
        except socket.error:
          resp.rep = 4
          raise Socks5Error
      else:
        addr = req.dst_addr
      
      self.dst_s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
      try:
        self.dst_s.connect((addr,req.dst_port))
      except socket.error:
        resp.rep = 4
        raise Socks5Error
  
      addr,port = self.dst_s.getsockname()
      resp.rep = 0
      resp.dst_addr = addr
      resp.dst_port = port
      self.s.send(resp.pack())
      
      self.forward_loop()

    except Socks5Error:
      self.s.send(resp.pack())
    
    except socket.error:
      pass
      
    finally:
      if self.s:
        self.s.close()
      if self.dst_s:
        self.dst_s.close()
      
  def forward_loop(self):
    while 1:
      r,w,x = select.select([self.s,self.dst_s],[],[],self.wait)
      if not r:
        continue
      
      for s in r:
        if s is self.s:
          buf = self.s.recv(self.buf_size)
          if not buf:
            raise socket.error
          self.dst_s.send(buf)
        if s is self.dst_s:
          buf = self.dst_s.recv(self.buf_size)
          if not buf:
            raise socket.error
          self.s.send(buf)
      time.sleep(0.01)
    
class Socks5(threading.Thread):
  def __init__(self,ip="0.0.0.0",port=8080):
    self.ip = ip
    self.port = port
    self.s = None
    threading.Thread.__init__(self)
    
  def run(self):
    try:
      self.s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
      self.s.bind((self.ip,self.port))
      self.s.listen(5)
      
    except socket.error, msg:
      print msg
      if self.s:
        self.s.close()
        self.s = None
      return False
    while 1:
      try:
        conn, addr = self.s.accept()
      except socket.error, msg:
        print msg
        self.s.close()
        self.s = None
        return False

      thread = Socks5Thread(conn,addr[0],addr[1])
      thread.start()
      
    return True
    
def main():
  ip_addr = "127.0.0.1"
  port = 4444
  try:
    ip_addr = sys.argv[1]
    port = int(sys.argv[2])
  except IndexError:
    pass
  s5 = Socks5(ip_addr,port)
  s5.start()

if __name__=='__main__':
    main()
