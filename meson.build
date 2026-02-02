project('ergo', 'python',
  version : '0.1.0',
  default_options : ['warning_level=2']
)

python = import('python').find_installation('python3')

srcdir = meson.project_source_root()
src_main = 'src/ergo/main.py'

# Custom target to run the compiler on a .e file
run_target('run',
  command : [
    python,
    '-c',
    'import sys; sys.path.insert(0, "@SOURCE_ROOT@/src"); from ergo.main import main; main()',
    '@INPUT0@'
  ],
  input : [],
  capture : false,
  description : 'Run the Ergo compiler on a .e file (use: meson run -Dinput=examples/yourfile.e)'
)

# Custom target to run the test suite
test('ergo-tests',
  python,
  args : ['-m', 'unittest', 'discover', 'tests'],
  workdir : meson.project_source_root()
)

# Optional: PyInstaller build (requires pyinstaller)
pyinstaller_script = '''
import sys
import subprocess

try:
    import PyInstaller  # noqa: F401
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "--user", "pyinstaller"])

subprocess.check_call([
    sys.executable, "-m", "PyInstaller",
    "--onefile", "--name", "ergo",
    "--paths", "src",
    "--add-data", "src/ergo/stdlib:ergo/stdlib",
    "ergo_cli.py"
])
'''

pyinstaller_build = custom_target(
  'pyinstaller-build',
  output : 'dist/ergo',
  command : [python, '-c', pyinstaller_script],
  build_by_default : false
)

# Summary message
message('''
Ergo Compiler Project (Meson)
----------------------------
- To run the compiler: meson compile run -Dinput=examples/yourfile.e
- To run tests:        meson test
- To build binary:     meson compile pyinstaller-build
''')
