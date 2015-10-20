# thread_count_sparc
Records the number of running and runnable threads on SPARC with Solaris 10 with down to 1ms sampling period.
Run runme.sh with root user.

--- How it works:

All tunables and constants are defined in constants.h

Kernel module:
The kernel module counts the number of threads by looking at each CPUs dispatcher length and active thread. The total number is recorded in a circular array with size THR_CNT_AR_SIZE every THR_CNT_SAMPLING_PERIOD_NS. The recording starts and stops when a dummy write to the module is issued. The dummy write is controlled by the runme.sh script.

User program:
The user module attempts to read the entire record (duration is given to runme.sh as an argument) from the kernel module at single read command and waits. The kernel module copies the recorded numbers to the read buffer after filling 1/4th of the array.

Granularity:
The program consumes two threads that do not get blocked unless the scheduler forces them to. Blocking causes the recording threads unable to do their work for up to 10ms, which is the solaris scheduler tick.

