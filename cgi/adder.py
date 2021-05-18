import os
import sys
import string

method = os.environ['REQUEST_METHOD']
server_name = os.environ['SERVER_SOFTWARE']

def response500():
  print 'HTTP/1.1 500 Internal Error\r\nServer: %s\r\nConnection: Close\r\n\r\n'%(server_name)

def compute(input):
  params = input.split('&')
  if(len(params) != 2):
    response500()
  else:
    lhs = string.atoi(params[0].split('=')[1])
    rhs = string.atoi(params[1].split('=')[1])
    res = str(lhs + rhs)
    print 'HTTP/1.1 200 OK\r\nServer: %s\r\nContent-length: %d\r\n\r\n%s'%(server_name, len(res), res)

if method == 'GET':
  query_string = os.environ['QUERY_STRING']
  if query_string is None:
    response500()
  else:
    compute(query_string)

if method == 'POST':
  content_length = os.environ['CONTENT_LENGTH']
  if content_length <= 0:
    response500()
  else:
    body = sys.stdin.read(string.atoi(content_length))
    compute(body)