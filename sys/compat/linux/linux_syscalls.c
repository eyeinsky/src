/*	$OpenBSD: linux_syscalls.c,v 1.13 1999/12/06 19:37:49 aaron Exp $	*/

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	OpenBSD: syscalls.master,v 1.13 1997/12/10 11:55:28 deraadt Exp 
 */

char *linux_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"waitpid",			/* 7 = waitpid */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execve",			/* 11 = execve */
	"chdir",			/* 12 = chdir */
	"time",			/* 13 = time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"#18 (obsolete ostat)",		/* 18 = obsolete ostat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"mount",			/* 21 = mount */
	"umount",			/* 22 = umount */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"stime",			/* 25 = stime */
	"#26 (unimplemented ptrace)",		/* 26 = unimplemented ptrace */
	"alarm",			/* 27 = alarm */
	"#28 (obsolete ofstat)",		/* 28 = obsolete ofstat */
	"pause",			/* 29 = pause */
	"utime",			/* 30 = utime */
	"#31 (unimplemented stty)",		/* 31 = unimplemented stty */
	"#32 (unimplemented gtty)",		/* 32 = unimplemented gtty */
	"access",			/* 33 = access */
	"nice",			/* 34 = nice */
	"#35 (unimplemented ftime)",		/* 35 = unimplemented ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"rename",			/* 38 = rename */
	"mkdir",			/* 39 = mkdir */
	"rmdir",			/* 40 = rmdir */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"times",			/* 43 = times */
	"#44 (unimplemented prof)",		/* 44 = unimplemented prof */
	"brk",			/* 45 = brk */
	"setgid",			/* 46 = setgid */
	"getgid",			/* 47 = getgid */
	"signal",			/* 48 = signal */
	"geteuid",			/* 49 = geteuid */
	"getegid",			/* 50 = getegid */
	"acct",			/* 51 = acct */
	"#52 (unimplemented phys)",		/* 52 = unimplemented phys */
	"#53 (unimplemented lock)",		/* 53 = unimplemented lock */
	"ioctl",			/* 54 = ioctl */
	"fcntl",			/* 55 = fcntl */
	"#56 (unimplemented mpx)",		/* 56 = unimplemented mpx */
	"setpgid",			/* 57 = setpgid */
	"#58 (unimplemented ulimit)",		/* 58 = unimplemented ulimit */
	"oldolduname",			/* 59 = oldolduname */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"#62 (unimplemented ustat)",		/* 62 = unimplemented ustat */
	"dup2",			/* 63 = dup2 */
	"getppid",			/* 64 = getppid */
	"getpgrp",			/* 65 = getpgrp */
	"setsid",			/* 66 = setsid */
	"sigaction",			/* 67 = sigaction */
	"siggetmask",			/* 68 = siggetmask */
	"sigsetmask",			/* 69 = sigsetmask */
	"setreuid",			/* 70 = setreuid */
	"setregid",			/* 71 = setregid */
	"sigsuspend",			/* 72 = sigsuspend */
	"sigpending",			/* 73 = sigpending */
	"sethostname",			/* 74 = sethostname */
	"setrlimit",			/* 75 = setrlimit */
	"getrlimit",			/* 76 = getrlimit */
	"getrusage",			/* 77 = getrusage */
	"gettimeofday",			/* 78 = gettimeofday */
	"settimeofday",			/* 79 = settimeofday */
	"getgroups",			/* 80 = getgroups */
	"setgroups",			/* 81 = setgroups */
	"oldselect",			/* 82 = oldselect */
	"symlink",			/* 83 = symlink */
	"olstat",			/* 84 = olstat */
	"readlink",			/* 85 = readlink */
	"uselib",			/* 86 = uselib */
	"swapon",			/* 87 = swapon */
	"reboot",			/* 88 = reboot */
	"readdir",			/* 89 = readdir */
	"mmap",			/* 90 = mmap */
	"munmap",			/* 91 = munmap */
	"truncate",			/* 92 = truncate */
	"ftruncate",			/* 93 = ftruncate */
	"fchmod",			/* 94 = fchmod */
	"fchown",			/* 95 = fchown */
	"getpriority",			/* 96 = getpriority */
	"setpriority",			/* 97 = setpriority */
	"profil",			/* 98 = profil */
	"statfs",			/* 99 = statfs */
	"fstatfs",			/* 100 = fstatfs */
#ifdef __i386__
	"ioperm",			/* 101 = ioperm */
#else
	"#101 (unimplemented ioperm)",		/* 101 = unimplemented ioperm */
#endif
	"socketcall",			/* 102 = socketcall */
	"#103 (unimplemented klog)",		/* 103 = unimplemented klog */
	"setitimer",			/* 104 = setitimer */
	"getitimer",			/* 105 = getitimer */
	"stat",			/* 106 = stat */
	"lstat",			/* 107 = lstat */
	"fstat",			/* 108 = fstat */
	"olduname",			/* 109 = olduname */
#ifdef __i386__
	"iopl",			/* 110 = iopl */
#else
	"#110 (unimplemented iopl)",		/* 110 = unimplemented iopl */
#endif
	"#111 (unimplemented vhangup)",		/* 111 = unimplemented vhangup */
	"#112 (unimplemented idle)",		/* 112 = unimplemented idle */
	"#113 (unimplemented vm86)",		/* 113 = unimplemented vm86 */
	"wait4",			/* 114 = wait4 */
	"#115 (unimplemented swapoff)",		/* 115 = unimplemented swapoff */
	"#116 (unimplemented sysinfo)",		/* 116 = unimplemented sysinfo */
	"ipc",			/* 117 = ipc */
	"fsync",			/* 118 = fsync */
	"sigreturn",			/* 119 = sigreturn */
	"#120 (unimplemented clone)",		/* 120 = unimplemented clone */
	"setdomainname",			/* 121 = setdomainname */
	"uname",			/* 122 = uname */
#ifdef __i386__
	"modify_ldt",			/* 123 = modify_ldt */
#else
	"#123 (unimplemented modify_ldt)",		/* 123 = unimplemented modify_ldt */
#endif
	"#124 (unimplemented adjtimex)",		/* 124 = unimplemented adjtimex */
	"mprotect",			/* 125 = mprotect */
	"sigprocmask",			/* 126 = sigprocmask */
	"#127 (unimplemented create_module)",		/* 127 = unimplemented create_module */
	"#128 (unimplemented init_module)",		/* 128 = unimplemented init_module */
	"#129 (unimplemented delete_module)",		/* 129 = unimplemented delete_module */
	"#130 (unimplemented get_kernel_syms)",		/* 130 = unimplemented get_kernel_syms */
	"#131 (unimplemented quotactl)",		/* 131 = unimplemented quotactl */
	"getpgid",			/* 132 = getpgid */
	"fchdir",			/* 133 = fchdir */
	"#134 (unimplemented bdflush)",		/* 134 = unimplemented bdflush */
	"#135 (unimplemented sysfs)",		/* 135 = unimplemented sysfs */
	"personality",			/* 136 = personality */
	"#137 (unimplemented afs_syscall)",		/* 137 = unimplemented afs_syscall */
	"#138 (unimplemented setfsuid)",		/* 138 = unimplemented setfsuid */
	"#139 (unimplemented getfsuid)",		/* 139 = unimplemented getfsuid */
	"llseek",			/* 140 = llseek */
	"getdents",			/* 141 = getdents */
	"select",			/* 142 = select */
	"flock",			/* 143 = flock */
	"msync",			/* 144 = msync */
	"readv",			/* 145 = readv */
	"writev",			/* 146 = writev */
	"getsid",			/* 147 = getsid */
	"fdatasync",			/* 148 = fdatasync */
	"__sysctl",			/* 149 = __sysctl */
	"mlock",			/* 150 = mlock */
	"munlock",			/* 151 = munlock */
	"#152 (unimplemented mlockall)",		/* 152 = unimplemented mlockall */
	"#153 (unimplemented munlockall)",		/* 153 = unimplemented munlockall */
	"#154 (unimplemented sched_setparam)",		/* 154 = unimplemented sched_setparam */
	"#155 (unimplemented sched_getparam)",		/* 155 = unimplemented sched_getparam */
	"#156 (unimplemented sched_setscheduler)",		/* 156 = unimplemented sched_setscheduler */
	"#157 (unimplemented sched_getscheduler)",		/* 157 = unimplemented sched_getscheduler */
	"#158 (unimplemented sched_yield)",		/* 158 = unimplemented sched_yield */
	"#159 (unimplemented sched_get_priority_max)",		/* 159 = unimplemented sched_get_priority_max */
	"#160 (unimplemented sched_get_priority_min)",		/* 160 = unimplemented sched_get_priority_min */
	"#161 (unimplemented sched_rr_get_interval)",		/* 161 = unimplemented sched_rr_get_interval */
	"nanosleep",			/* 162 = nanosleep */
	"mremap",			/* 163 = mremap */
};
