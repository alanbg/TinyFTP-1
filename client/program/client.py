import socket
import re
import os
import random
import time
import ctypes
import getpass

class Client(object):
  """ftp client"""
  def __init__(self):
    self.hip = None # server ip
    self.hport = None
    self.sock = None
    self.buf_size = 8192
    self.logged = False
    self.lip = None # local ip
    self.mode = 'pasv'
    self.append = False
    self.encrypt = False
    self.rsalib = ctypes.CDLL('./librsa.so')
    self.rsalib.decodeStringChar.restype = ctypes.c_char_p
    self.rsalib.encodeStringChar.restype = ctypes.c_char_p
    self.pub_exp = None
    self.pub_mod = None
    self.bts = None
    self.uname = None
    self.pwd = None

  def decode(self, msg):
    ret = self.rsalib.decodeStringChar(bytes(msg, encoding='ascii'), bytes(self.pub_exp, encoding='ascii'), bytes(self.pub_mod, encoding='ascii'))
    ret = ret.decode('ascii')
    return ret

  def encode(self, msg):
    ret = self.rsalib.encodeStringChar(bytes(msg, encoding='ascii'), bytes(self.pub_exp, encoding='ascii'), bytes(self.pub_mod, encoding='ascii'))
    ret = ret.decode('ascii')
    return ret;

  def extract_addr(self, string):
    ip = None
    port = None

    result = re.findall(r"\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\,){5}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b", string)
    if len(result) != 1:
      print('client failed to find valid address in %s', string)
      return ip, port

    addr = result[0].replace(',', '.')

    # split ip addr and port number
    idx = -1
    for i in range(4):
      idx = addr.find('.', idx + 1)

    ip = addr[:idx]
    port = addr[idx + 1:]

    idx = port.find('.')
    p1 = int(port[:idx])
    p2 = int(port[idx + 1:])
    port = int(p1 * 256 + p2)

    return ip, port

  def send(self, msg):
    msg += '\r\n';
    if self.encrypt:
      msg = self.encode(msg)
    self.sock.send(bytes(msg, encoding='ascii'))

  def recv(self):
    res = self.sock.recv(self.buf_size).decode('ascii').strip()
    if self.encrypt:
      res = self.decode(res)
    code = int(res.split()[0])
    return code, res

  def xchg(self, msg):
    """exchange message: send server the msg and return response"""
    # try:
    self.send(msg)
    code, res = self.recv()
    # except Exception as e:
    #   print('Error in Client.xchg' + str(e))
    return code, res

  def pasv(self):
    code, res = self.xchg('PASV')
    print(res.strip())
    ip = None
    port = None
    if code == 227:
      ip, port = self.extract_addr(res)
    return ip, port

  def port(self):
    lport = random.randint(20000, 65535)
    p1 = lport // 256
    p2 = lport % 256
    ip = self.lip.replace('.', ',')
    code, res = self.xchg('PORT %s,%d,%d' % (ip, p1, p2))
    print(res.strip())
    if code // 100 == 2:
      lstn_sock = socket.socket()
      lstn_sock.bind(('', lport))
      lstn_sock.listen(10)
      return lstn_sock
    else:
      return None

  def data_connect(self, msg):
    data_sock = None
    if self.mode == 'pasv':
      ip, port = self.pasv()
      self.send(msg)
      if ip and port:
        data_sock = socket.socket()
        data_sock.connect((ip, port))
        code, res = self.recv()
        print(res.strip())
        if (code != 150):
          data_sock.close()
          data_sock = None;
      else:
        print('Error in Client.data_connect: no ip or port')
    elif self.mode == 'port':
      lstn_sock = self.port()
      self.send(msg)
      if lstn_sock:
        data_sock, _ = lstn_sock.accept()
        lstn_sock.close()
        code, res = self.recv()
        print(res.strip())
      else:
        print('Error in Client.data_connect: no lstn_sock')
    else:
      print('Error in Client.data_connect: illegal mode')
    return data_sock

  def command_open(self, arg):
    self.hip = arg[0]
    self.hport = 21
    if len(arg) > 1:
      self.hport = int(arg[1])
    if self.logged:
      print('Error: you are connected, please close first.')
      return
    self.sock = socket.socket()
    self.sock.connect((self.hip, self.hport))
    code, res = self.recv()
    print(res.strip())

    # self.send('SYST')
    # res = self.recv()
    # print('Server system: %s' % res)

    if code == 220: # success connect
      self.uname = input('username: ')
      code, res = self.xchg('USER ' + self.uname)
      if code == 331: # ask for password
        self.pwd = getpass.getpass('password: ')
        code, res = self.xchg('PASS ' + self.pwd)
        if code // 100 == 2: # login success
          print('login successful as %s' % self.uname)
          self.logged = True
          code, res = self.xchg('TYPE I')
          if code == 200: # use binay
            print('using binary.')
          else:
            print('server refused using binary.')
        else:
          print(res.strip())
          print('login failed')
      else:
        print(res.strip())
        print('login failed')
    else:
      print('connection fail due to server')

  def command_recv(self, arg):
    arg = ''.join(arg)
    data_sock = self.data_connect('RETR ' + arg)
    if data_sock:
      f = None
      if self.append:
        f = open(arg, 'ab')
        self.append = False
        print('resuming transfer...')
      else:
        f = open(arg, 'wb')

      t = time.time()
      data = data_sock.recv(self.buf_size)
      total = len(data)
      while data:
        f.write(data)
        data = data_sock.recv(self.buf_size)
        total += len(data)
      f.close()
      data_sock.close()
      code, res = self.recv()
      t = time.time() - t
      print(res.strip())
      print('%dkb in %f seconds, %fkb/s in avg' % (total, t, total / t / 1e3))
    else:
      print('Error in Client.command_recv: no data_sock')

  def command_send(self, arg):
    arg = ''.join(arg)
    data_sock = self.data_connect('STOR ' + arg)
    if data_sock:
      t = time.time()
      with open(arg, 'rb') as f:
        data_sock.send(f.read())
      data_sock.close()
      code, res = self.recv()
      t = time.time() - t
      print(res.strip())
      total = os.path.getsize(arg)
      print('%dkb in %f seconds, %fkb/s in avg' % \
        (total, t, total / (t+1e-4) / 1e3))
    else:
      print('Error in Client.command_send: no data_sock')

  def command_ls(self, arg):
    arg = ''.join(arg)
    if len(arg) == 0:
      arg = './'
    else:
      arg = arg[0]
    data_sock = self.data_connect('LIST ' + arg)
    if data_sock:
      data = ""
      packet = data_sock.recv(self.buf_size)
      while packet:
        data += packet.decode('ascii').strip()
        packet = data_sock.recv(self.buf_size)
      print(data)
      code, res = self.recv()
      print(res.strip())
      data_sock.close()
    else:
      print('Error in Client.command_ls: no data_sock')

  def command_help(self, arg):
    print('supported commands:')
    for attr in dir(self):
      if 'command_' in attr:
        print(attr[len('command_'): ])

  def command_close(self, arg):
    code, res = self.xchg('QUIT')
    print(res.strip())
    self.sock.close()
    self.__init__()

  def command_bye(self, arg):
    if self.logged:
      self.command_close('')
    print('good luck')
    return True

  def command_nlist(self, arg):
    arg = ''.join(arg)
    if len(arg) == 0:
      arg = './'
    else:
      arg = arg[0]
    data_sock = self.data_connect('NLST ' + arg)
    if data_sock:
      data = ""
      packet = data_sock.recv(self.buf_size)
      while packet:
        data += packet.decode('ascii').strip()
        packet = data_sock.recv(self.buf_size)
      print(data)
      code, res = self.recv()
      print(res.strip())
      data_sock.close()
    else:
      print('Error in Client.command_ls: no data_sock')

  def command_mkdir(self, arg):
    arg = ''.join(arg)
    code, res = self.xchg('MKD ' + arg)
    print(res.strip())

  def command_rm(self, arg):
    arg = ''.join(arg)
    code, res = self.xchg('RMD ' + arg)
    print(res.strip())

  def command_cd(self, arg):
    arg = ''.join(arg)
    code, res = self.xchg('CWD ' + arg)
    print(res.strip())

  def command_resume(self, arg):
    try:
      offset = os.path.getsize(''.join(arg))
      code, res = self.xchg('REST %d' % offset)
      if code // 100 == 2 or code // 100 == 3:
        self.append = True
      else:
        print('server rejected resume')
      self.command_recv(arg)
    except Exception as e:
      print(str(e))

  def command_pasv(self, arg):
    self.mode = 'pasv'
    print('switch to pasv mode')

  def command_port(self, arg):
    self.mode = 'port'
    print('switch to part mode')
    print('ip address %s' % self.lip)

  def command_mult(self, arg):
    code, res = self.xchg('MULT')
    print(res.strip())

  def command_encry(self, arg):
    if self.encrypt:
      self.send('ENCR')
      self.encrypt = False
      code, res = self.recv()
      print(res.strip())
    else:
      code, res = self.xchg('ENCR')
      self.encrypt = True
      self.pub_exp, self.pub_mod, self.bts = res.split()[1].split(',')
      self.bts = int(self.bts)
      code, res = self.recv()
      print(res.strip())

  def run(self):
    self.lip = socket.gethostbyname(socket.gethostname())
    print('ftp client start, ip addr %s' % self.lip)
    flag = None
    while not flag:
      cmd = input('ftp > ').split()
      arg = cmd[1:]
      cmd = cmd[0]
      try:
        flag = getattr(self, "command_%s" % cmd)(arg)
      except Exception as e:
        print(str(e))
        print('invalid command')


    
if __name__ == '__main__':
  client = Client()
  client.run()



