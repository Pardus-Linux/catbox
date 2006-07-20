#!/usr/bin/python

import os
import catbox
import sys

def tryWrite(who, path):
    try:
        f = file(path, "w")
        f.write("hello world\n")
        f.close()
        print "%s wrote '%s'" % (who, path)
    except Exception, e:
        tmp = str(e.__class__)
        if '.' in tmp:
            tmp = tmp.split('.')[-1]
        print "%s cannot write '%s'\n  %s: %s" % (who, path, tmp, e)

def test():
    pid = os.fork()
    if pid == -1:
        print "fork failed"
        sys.exit(0)

    if pid == 0:
        tryWrite("Child: ", "/tmp/catboxtest.txt")
    else:
        tryWrite("Parent: ", "/tmp/catboxtest.txt")

catbox.run(test, writable_paths=[os.getcwd()])

