#!/usr/bin/python

# hashfork.py
#
# by Hubert Chan, March 2005
#
# This file is hereby placed in the public domain.

import os
import signal
import sys

if len(sys.argv) < 3:
    print "Run multiple instances of hashcash in parallel"
    print "usage:"
    print " ", sys.argv[0], "<processes> <hashcash command line arguments>"
    print
    print "    <processes> is the number of hashcash processes to start"
    print "    <hashcash command line arguments> is the command line argumens to pass to"
    print "        hashcash.  Note that the -m option will be automatically added, and"
    print "        so does not need to be specified."
    print
    print "Note: you MUST specify a resource on the command line."
    sys.exit ()

NUM_PROCS = int (sys.argv[1])

del (sys.argv[0:2])
sys.argv.insert(0, "hashcash")
sys.argv.insert(1, "-m")

children = [ ]
pipes = [ ]

for i in range (NUM_PROCS):
    pipe = os.pipe () # create a pipe to communicate with the child
    pid = os.fork ()
    if pid:
        # parent
        children.append (pid)
        os.close (pipe[1]) # close the write end of the pipe
        pipes.append (os.fdopen (pipe[0]))
#        print "process", pid, "started"
    else:
        # child
        os.close (pipe[0]) # close the read end of the pipe
        os.dup2 (pipe[1], 1) # redirect stdout to the pipe
        os.close (pipe[1])
        os.execvp ("hashcash", sys.argv)

status = os.wait ()

#print "process", status[0], "exited.\nkilling all children."

for i in range (NUM_PROCS):
#    print children[i]
    if children[i] == status[0]:
        print (pipes[i].readline ()),
    try:
        os.kill (children[i], signal.SIGKILL)
    except OSError: # kill raises an OSError when the process already exited
        pass
