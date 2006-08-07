#!/usr/bin/python

import sys
import os
import catbox

def test1():
    # correct op, implicit end
    a = 5

def test2():
    # incorrect op, implicit end
    file("/tmp/catboxtest.txt", "w").write("lala")

def test3():
    # correct op, explicit end
    sys.exit(0)

def test4():
    # incorrect op, explicit end
    sys.exit(3)

tests = (test1, test2, test3, test4)
ret = map(catbox.run, tests)
print ret

