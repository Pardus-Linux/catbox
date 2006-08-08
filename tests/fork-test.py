#!/usr/bin/python

import os
import catbox
import sys

def tryWrite(who, path):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
    except IOError, e:
        if e.errno == 13:
            return 0
        print "Sandbox error in %s: cannot write '%s': %s" % (who, path, e)
        return 1
    
    print "Sandbox violation in %s: wrote '%s'" % (who, path)
    return 1

def test():
    pid = os.fork()
    if pid == -1:
        print "fork failed"
        sys.exit(0)

    if pid == 0:
        tryWrite("child", "/tmp/catboxtest.txt")
    else:
        tryWrite("parent", "/tmp/catboxtest.txt")

catbox.run(test, writable_paths=[os.getcwd()])
