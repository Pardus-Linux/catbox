#!/usr/bin/python

import sys
import catbox.catbox as catbox

def logger(event, data):
    print event, data
    
class Test:
    def __init__(self, path):
        self.path = path
    
    def write(self):
        try:
            file(self.path, "w").write("lala\n")
        except IOError, e:
            if e.errno == 13:
                sys.exit(0)
        sys.exit(2)
    
    def boxedWrite(self):
        ret = catbox.run(self.write, logger=logger)
        if ret:
            print "Sandbox error"
            sys.exit(2)

test = Test("catboxtest.deleteme")
test.boxedWrite()

