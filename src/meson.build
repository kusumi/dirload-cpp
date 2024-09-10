src = [
  'dir.cc',
  'flist.cc',
  'main.cc',
  'stat.cc',
  'util.cc',
  'worker.cc',
  ]

# https://mesonbuild.com/Threads.html
dep = [dependency('threads')]

if get_option('debug')
  add_global_arguments('-DDEBUG', language : 'cpp')
  src += 'log.cc'
endif

if get_option('stdthread')
  add_global_arguments('-DCONFIG_STDTHREAD', language : 'cpp')
  src += 'thread_std.cc'
else
  dep += meson.get_compiler('cpp').find_library('pthread', required : true)
  src += 'thread_posix.cc'
endif

# `dnf install cppunit cppunit-devel` on Fedora
if get_option('cppunit')
  add_global_arguments('-DCONFIG_CPPUNIT', language : 'cpp')
  dep += dependency('cppunit')
  src += 'cppunit.cc'
endif

executable('dirload-cpp', src, dependencies : dep, install : true)
