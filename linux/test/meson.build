# Simple tests that directly use the lfi_linux API.
tests = [
  'test_init.c',
]

foreach t : tests
  test(t, executable(
    t + '.elf',
    t,
    dependencies: [lfi_linux],
  ), suite: ['linux'])
endforeach

# Remaining tests work by running actual programs and checking outputs.
lficc = find_program(cpu + '-lfi-linux-musl-clang', required: false)
if not lficc.found()
  warning('lficc not found: cannot test liblfi')
  subdir_done()
endif

go = find_program('go', required: false)
if not go.found()
  warning('go not found: cannot test liblfi')
  subdir_done()
endif

test_run = executable(
  'test_run.elf',
  'test_run.c',
  dependencies: [lfi_linux],
)

testdata = [
  {'bin': test_run, 'file': 'programs/hello.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/alloc.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/argv.c', 'inputs': ['one', 'two', 'three'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/envp.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/mmap.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/thread.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/time.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/random.c', 'inputs': [], 'cflags': []},
  {'bin': test_run, 'file': 'programs/chdir.c', 'inputs': ['programs/inputs', 'file.txt'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/file.c', 'inputs': ['programs/inputs/file.txt'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/access.c', 'inputs': ['programs/inputs/file.txt'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/rename.c', 'inputs': ['programs/inputs/rename.txt'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/truncate.c', 'inputs': ['programs/inputs/truncate.txt'], 'cflags': []},
  {'bin': test_run, 'file': 'programs/stat.c', 'inputs': ['programs/inputs/file.txt'], 'cflags': []},
  # requires rmdir
  # {'bin': test_run, 'file': 'programs/mkdir.c', 'inputs': [], 'cflags': []},
]

foreach input : testdata
  out = input['file'].replace('/', '_') + '.lfi'
  exec = custom_target(out,
    output: out,
    input: input['file'],
    command: [lficc, '-O2', '-o', '@OUTPUT@', '@INPUT@', '-static-pie'] + input['cflags'],
  )
  expected = input['file'] + '.out'
  test(input['file'],
    go, args: ['run', 'run.go', expected, input['bin'], exec] + input['inputs'],
    workdir: meson.current_source_dir(),
    suite: ['linux']
  )
endforeach
