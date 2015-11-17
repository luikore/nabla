multiple threads compete for a kqueue

events may use change-trigger so we don't miss any

channels are implemented in fd (even in-process fd)

channel can be event source

    chan = Channel.watch_dir dir
    loop
      chan.read
      ...
    end
