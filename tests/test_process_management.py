import os
import signal
import sys
import time

import testing
import testify as T

class ProcessManagementTestCase(testing.BaseTestCase):

    def test_child(self):
        """catbox.run will fork(). Child process will execute
        self.child_function. This test verifies that child is actually
        running the given function reporting back to parent through a
        Unix pipe.
        """
        self.run_child_function_in_catbox()
        self.verify_message_from_child()

    def test_child_does_not_report_back(self):
        def lazy_child():
            pass
        with T.assert_raises(testing.ChildDidNotReportBackException):
            # lazy_child function will not report anything back and
            # parent should timeout waiting for it.
            self.run_child_function_in_catbox(lazy_child)
            self.verify_message_from_child()


class ProcessGroupManagementTestCase(testing.BaseTestCase):

    def test_watchdog(self):
        def sleeping_child_function():
			time.sleep(5)

        # To test watchdog we'll kill the parent and see if the
        # watchdog takes care of killing the child process. We'll fork
        # here and run catbox in the forked process to kill the catbox
        # parent prematurely.
        catbox_pid = os.fork()
        if not catbox_pid: # catbox process

            # Change process group. We do not want catbox watchdog to
            # kill our test process (testify) too.
            os.setpgid(os.getpid(), os.getpid())

            self.run_child_function_in_catbox(
                child_function=sleeping_child_function
            )
            assert False, "Shouldn't get here. Parent should kill us already."
            sys.exit(0)
        else:
			# wait for catbox and traced child to start up and
			# initialized. We're just sleeping enough for catbox
			# process to have a chance to start running.
            time.sleep(.1)

            # Killing catbox (parent) will trigger watchdog process
            # and it will kill the process group.
            os.kill(catbox_pid, signal.SIGKILL)

        assert self.is_process_alive(catbox_pid)
        # TODO: find a way to check if traced process (catbox's child)
        # is alive.
