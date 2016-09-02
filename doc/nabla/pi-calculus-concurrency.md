We use the shorter name `Proc` to denote the light weight process (as in "process calculus"), and longer name `Process` to denote the much heavier operating system process.

# Spawning a new proc

    Proc[->
      ...
    end]

    proc = Proc[obj.method 'foo']

Proc starts running right after the spawning

### New proc utils

    Proc{sleep: true, call: ->
      ...
    ;}

We don't need `new_seq` ? We can manual code the invoking.

# Sleeping and terminating a proc

    proc.stop

sleep and awake

    proc.sleep $(5 seconds) # wake after 5 seconds
    proc.sleep              # sleep permantly
    proc.wake

sleep current proc

    :sleep 5.0
    Proc.current.sleep

# Proc communication

Only through channels

    chan = Channel{"type": Integer}

`recv` and `send`

    Proc[->
      x = chanx.recv
      chan.send 'foo'
    ;]

For rendezvous (or barrier, join) behavior, the primitive way:

    chan = Channel{}
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

### Uniq channel

    Channel{"uniq": true}

# Timer proc

    # one-shot
    Proc.timeout 5.3 ->
      ...
    end

    # if one round takes more than the interval to finish
    # then the next round (or maybe more rounds) is canceled
    Proc.interval_seq 5.3 ->
      ...
    end

    # always start a proc after the interval
    Proc.overlap_interval 5.3 ->
      ...
    end

# Error handling

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

# Utils to avoid resource locking

Grouped channels - with biased priority

    g = Channel::Group[]
    c1 = g.new_channel
    c2 = g.new_channel

# Uniq channel (reconcilation?)

Note: only keep messages in buffer uniq

    c = Channel.uniq
    c.send 1
    c.send 1

# Channel lifecycle

channel is ref-counted (closes external connection too)

If a channel is totally used for external connection, just retain it.

    some_chan.retain # additional retain
    some_chan.release

# Channel with limited slots?

No need, just check size of the channel and decide whether to drop the message.

# Semaphore

Just start same proc several times.

# About static analysis

It is hard to do so in a dynamic language, but maybe we can make the analysis in some point after all processes are loaded and detect resource exhausion after building channel/process graph.

