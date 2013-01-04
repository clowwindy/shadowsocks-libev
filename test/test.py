#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys
import select
import struct
import hashlib
import string
from subprocess import Popen, PIPE

p1 = Popen(['./server'], shell=False, bufsize=0, stdin=PIPE, 
    stdout=PIPE, stderr=PIPE, close_fds=True)
p2 = Popen(['./local'], shell=False, bufsize=0, stdin=PIPE,
    stdout=PIPE, stderr=PIPE, close_fds=True)
p3 = None

try:
    ready_count = 0
    fdset = [p1.stdout, p2.stdout, p1.stderr, p2.stderr]
    while True:
        r, w, e = select.select(fdset, [], fdset)
        if e:
            break
            
        for fd in r:
            line = fd.readline()
            sys.stderr.write(line)
            if line.find('at port') >= 0:
                ready_count += 1
        
        if ready_count == 2 and p3 is None:
            p3 = Popen(['curl', 'http://www.google.com/', '-v', '-L',
                        '--socks5-hostname', '127.0.0.1:1080'], shell=False,
                        bufsize=0,  close_fds=True)
            break
            
    if p3 is not None:
        r = p3.wait()
        if r == 0:
            print 'test passed'
        sys.exit(r)
    
finally:
    for p in [p1, p2]:
        try:
            p.kill()
        except OSError:
            pass
   
sys.exit(-1)
