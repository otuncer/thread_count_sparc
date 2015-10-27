# thread_count_sparc
Records the number of running and runnable threads on SPARC with Solaris 10 with down to 1ms sampling period.
Run runme.sh with root user.

--- How it works:

All tunables and constants are defined in constants.h

Kernel module:
The kernel module counts the number of threads by looking at each CPUs dispatcher length and active thread. The total number is recorded in a ring buffer with size THR_CNT_AR_SIZE every THR_CNT_SAMPLING_PERIOD_NS. The recording starts and stops when a dummy write to the module is issued. The dummy write is issued by the runme.sh script. The kernel module copies the recorded numbers to the read buffer in the read() call in chunks of 1/4th of the ring buffer.

User program:
The user module attempts to read the entire record (duration is given to runme.sh as an argument) from the kernel module at a single read system call and waits.

Granularity:
The program consumes two threads that do not get blocked unless the scheduler forces them to. Blocking causes the recording to suspend for up to 10ms, which is the solaris scheduler tick. Busy polling with two threads is not a problem as the SPARC CPUs have 128+ hardware threads.

