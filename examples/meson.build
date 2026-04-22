lficc_name = cpu + '_lfi-linux-musl-clang'
lficc = find_program(lficc_name, required: false)
have_lficc = lficc.found()

thread_dep = dependency('threads')

# Override -fvisibility=hidden from the root project so that passthrough
# shared libraries export their symbols for dlsym.
example_lib_args = ['-fvisibility=default']

subdir('add')
subdir('bench')
subdir('callback')
subdir('inout')
subdir('shm')
subdir('static')
subdir('threads')
