import os
import select

import catbox
import testify as T

class ChildDidNotReportBackException(Exception):
	pass

class ProcessManagement(T.TestCase):

	@T.setup
	def prepare_parent_child_communication(self):
		self.MAX_READ_SIZE = 1024
		# Create a pipe to verify that child is created and can
		# communicate with the parent.
		self.read_pipe, self.write_pipe = os.pipe()
		self.default_expected_message_from_child = "I'm ALIVE!"

		self.epoll = select.epoll()
		self.epoll.register(self.read_pipe, select.EPOLLIN | select.EPOLLET)

		def child_function():
			os.write(self.write_pipe, self.default_expected_message_from_child)

		self.default_child_function = child_function

	@T.teardown
	def close_pipe(self):
		os.close(self.read_pipe)
		os.close(self.write_pipe)

	def poll(self):
		events = self.epoll.poll(.1)
		if events:
			first_event = events[0]
			read_fd = first_event[0]
			return os.read(read_fd, self.MAX_READ_SIZE)
		return False
		
	def verify_message_from_child(self, expected_message=None):
		expected_message = expected_message or self.default_expected_message_from_child
		actual_message_from_child = self.poll()
		if actual_message_from_child:
			T.assert_in(expected_message, actual_message_from_child)
		else:
			raise ChildDidNotReportBackException

	def run_child_function_in_catbox(self, child_function=None, init_hook=None):
		child_function = child_function or self.default_child_function
		if init_hook:
			catbox.run(child_function, init_hook=init_hook)
		else:
			catbox.run(child_function)

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
		with T.assert_raises(ChildDidNotReportBackException):
			# lazy_child function will not report anything back and
			# parent should timeout waiting for it.
			self.run_child_function_in_catbox(lazy_child)
			self.verify_message_from_child()

	def test_init_hook(self):
		expected_message_from_init_function = "Init function successful!"
		def init_hook_function():
			os.write(self.write_pipe, expected_message_from_init_function)

		self.run_child_function_in_catbox(init_hook=init_hook_function)
		self.verify_message_from_child(expected_message_from_init_function)
