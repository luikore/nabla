We use the shorter name `Proc` to denote the light weight process (as in "process calculus"), and longer name `Process` to denote the much heavier operating system process.

# Spawning a new proc

    Proc[->
      ...
    end]

    proc = Proc[obj.method 'foo']

Proc starts running right after the spawning

To terminate a proc

    proc.stop

# Proc communication

Only through channels

    chan = Chan{"type": Integer}

`recv` and `send`

    Proc[->
      x = chanx.recv
      chan.send 'foo'
    ;]

For rendezvous (or barrier, join) behavior, the primitive way:

    chan = Chan{}
    Proc[->
      x = chanx.recv
      chan.send x
    ;]
    y = chan.recv
    x = chan.recv

A simpler, parallel way

    [x, y] = Proc.par ->
      chanx.recv
    end ->
      chany.recv
    end

## Buffered channel

Default channel only allows blocking send and recv. Buffered channel may make them not blocked.

    Chan{buffered: true}
    Chan{buffered: true, uniq: true}

## Channel lifecycle

channel is ref-counted (closes external connection too)

If a channel is totally used for external connection, just retain it.

    some_chan.retain # additional retain
    some_chan.release

## Channel with limited slots?

No need, just check size of the channel and decide whether to drop the message.

## Serialization optimizations

A single ref object that put into the channel is no need to be serialized and deserialized.

# Proc topics

## Error handling

Proc dies if error is thrown. We can make a pool to monitor procs inside.

    pool = Proc.pool
    pool.new_proc ->
      ...
    end
    pool.add some_proc
    pool.catch -> e proc
      ...
      # may re-spawn proc here
    end

    pool.stop_all

## Utils to avoid resource locking

Grouped channels - with biased priority

    g = Chan::Group[]
    c1 = g.new_channel
    c2 = g.new_channel

## On static analysis

It is hard to do so in a dynamic language, but maybe we can make the analysis in some point after all processes are loaded and detect resource exhaustion after building channel/process graph.

## Deadlock detection

If all procs are in forever sleep, then a deadlock error is raised. It requires `--detect-deadlock` starting arg since sometimes we mean to sleep the program forever.

# Concurrency patterns

## Generator

    c = Chan{}
    Proc[->
      i = 0
      while true
        i += 1
        c.send i
      end
    end]

## Select

    Chan.select_readable [c1, c2, c3]
    Chan.select_writable [c1, c2, c3]
    Chan.select [c1, c2, c3]

## Timer

Usually more precise than sleep:

    ct = Chan.timeout 5 'foo'

    case Chan.select_readable [ct, c1, c2, ...]
    when ct
      # timeout!
    when c1
      ...
    end

Also interval channel:

    Chan.interval 5 'foo'
    Chan.overlap_interval 5 'foo'

## Semaphore

Just start same proc several times.
