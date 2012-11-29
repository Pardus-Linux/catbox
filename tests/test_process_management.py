import os

import catbox
import testify as T

class ProcessManagement(T.TestCase):

	@T.setup
	def prepare(self):
		self.MAX_READ_SIZE = 1024
		self.read_pipe, self.write_pipe = os.pipe()
		self.expected_message_from_child = "I'm ALIVE!"
		def __child_function():
			os.write(self.write_pipe, self.expected_message_from_child)

		self.child_function = __child_function

	def test_child(self):
		catbox.run(self.child_function)
		actual_message_from_child = os.read(self.read_pipe, self.MAX_READ_SIZE)
		T.assert_equal(actual_message_from_child, self.expected_message_from_child)
