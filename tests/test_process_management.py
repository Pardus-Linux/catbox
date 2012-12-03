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

#class ProcessGroupManagementTestCase(ProcessManagementBaseTestCase):

    # @T.setup
    # def setup_parent_watchdog(self):
    #   pid = os.fork()
    #   if pid == 0: # child
    #       self.watchdog()
    #       sys.exit(0)
    #   else: # parent
    #       print "watchdog pid:", pid
    #       pass

    # def watchdog(self):
    #   print "watchdog"
    #   while True:
    #       pass

    # def test_init_hook_closes_pipe(self):
    #   def init_hook_function():
    #       os.close(self.write_pipe)

    #   def child_function():
    #       os.close(self.read_pipe)
    #       try:
    #           os.write(self.write_pipe, self.default_expected_message_from_child)
    #       except:
    #           print "WRITE FAILED!"
    #       else:
    #           print "FINE!"

    #   self.run_child_function_in_catbox(init_hook=init_hook_function, child_function=child_function)
    #   self.verify_message_from_child()

