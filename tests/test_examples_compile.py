import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(__file__))
SRC = os.path.join(ROOT, "src")
EXAMPLES = os.path.join(ROOT, "examples")

sys.path.insert(0, SRC)

from ergo.main import load_project  # noqa: E402
from ergo.typechecker import lower_program, typecheck_program  # noqa: E402


class ExampleCompileTests(unittest.TestCase):
    def test_examples_compile(self):
        for name in sorted(os.listdir(EXAMPLES)):
            if not name.endswith(".e"):
                continue
            path = os.path.join(EXAMPLES, name)
            with self.subTest(example=name):
                prog = load_project(path)
                prog = lower_program(prog)
                typecheck_program(prog)


if __name__ == "__main__":
    unittest.main()
