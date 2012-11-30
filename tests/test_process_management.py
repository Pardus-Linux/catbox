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
		self.expected_message_from_child = "I'm ALIVE!"

		self.epoll = select.epoll()
		self.epoll.register(self.read_pipe, select.EPOLLIN | select.EPOLLET)

		def __child_function():
			os.write(self.write_pipe, self.expected_message_from_child)
		self.child_function = __child_function

	def run_child_function_in_catbox(self):
		catbox.run(self.child_function)
		events = self.epoll.poll(.1)
		if events:
			first_event = events[0]
			read_fd = first_event[0]
			actual_message_from_child = os.read(read_fd, self.MAX_READ_SIZE)
			T.assert_equal(actual_message_from_child, self.expected_message_from_child)
		else:
			raise ChildDidNotReportBackException

	def test_child(self):
		"""catbox.run will fork(). Child process will execute
		self.child_function. This test verifies that child is actually
		running the given function reporting back to parent through a
		Unix pipe.
		"""
		self.run_child_function_in_catbox()

	def test_child_does_not_report_back(self):
		def lazy_child():
			pass
		self.child_function = lazy_child

		with T.assert_raises(ChildDidNotReportBackException):
			# lazy_child function will not report anything back and
			# parent should timeout waiting for it.
			self.run_child_function_in_catbox()

