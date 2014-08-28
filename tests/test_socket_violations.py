#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version. Please read the COPYING file.
#
import os
import socket

import testify as T

import catbox

def _catbox_it(f):
    result = catbox.run(f, collect_only=True, network=False, writable_paths=[])
    return result.code, result.violations

class TestSocketViolations(T.TestCase):
    def assert_violations(self, violations, kind, args):
        matching = [v_arg for (v_kind, _, v_arg) in violations if v_kind == kind]
        if type(args) is int:
            self.assertEquals(args, len(matching))
        else:
            self.assertEquals(args, matching)

    def test_sock_tcp_ipv4(self):
        s1 = socket.socket()
        s1.listen(1)
        host, port = s1.getsockname()
        def run():
            s2 = socket.socket()
            s2.connect((host, port))

            s2.close()
        code, v = _catbox_it(run)
        self.assertEquals(0, code)
        self.assertEquals(2, len(v))
        self.assert_violations(v, 'connect', ['%s:%d' % (host, port)])
        self.assert_violations(v, 'socketcall', 1)
        s1.close()

    def test_sock_tcp_ipv6(self):
        s1 = socket.socket(socket.AF_INET6)
        s1.listen(1)
        host, port, _, _ = s1.getsockname()
        def run():
            s2 = socket.socket(socket.AF_INET6)
            s2.connect((host, port))

            s2.close()
        code, v = _catbox_it(run)
        self.assertEquals(0, code)
        self.assert_violations(v, 'connect', ['%s:%d' % (host, port)])

        ## XXX: On some systems with more than 1 interface, ipv6 connect
        ##        may cause an additional socket of type AF_NETLINK to be
        ##        created. This is why we allow one or two socket calls
        ##        in this test
        self.assertTrue(len(v) in [2,3])
        self.assert_violations(v, 'socketcall', len(v)-1)
        s1.close()

    def test_sock_unix(self):
        path = os.path.abspath('./socket.sock')
        # make sure the socket doesn't exist
        try:
            os.unlink(path)
        except OSError:
            if os.path.exists(path):
                raise
        s1 = socket.socket(socket.AF_UNIX)
        s1.bind(path)
        s1.listen(1)
        def run():
            s2 = socket.socket(socket.AF_UNIX)
            s2.connect(path)

            s2.close()
        code, v = _catbox_it(run)
        self.assertEquals(0, code)
        self.assertEquals(2, len(v))
        self.assert_violations(v, 'connect', [path])
        self.assert_violations(v, 'socketcall', 1)
        s1.close()

    def test_nosyscall(self):
        def run():
            pass
        code, v = _catbox_it(run)
        self.assertEquals(0, code)
        self.assertEquals([], v)

