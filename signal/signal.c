/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Userspace posix signals
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <signal.h>
#include <sys/threads.h>
#include <phoenix/signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


extern int threadKill(int pid, int tid, int signal);


struct {
	sighandler_t sightab[NSIG];
	sigset_t sigset[NSIG];
	handle_t lock;
} signal_common;


static void _signal_bug(int sig)
{
#ifndef NDEBUG
	printf("%u: BUG - received signal #0\n", getpid());
#endif
}


static void _signal_ignore(int sig)
{
}


static void _signal_terminate(int sig)
{
	_exit(((sig) & 0x7f) << 8);
}


static void (*_signal_getdefault(int sig))(int)
{
	switch (sig) {
		case 0:
			return _signal_bug;

		case SIGHUP:
		case SIGINT:
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT: /* And SIGIOT */
		case SIGEMT:
		case SIGFPE: /* Should be handled by the kernel? */
		case SIGKILL: /* Should kill the process before handler is ever invoked */
		case SIGBUS:
		case SIGSEGV:
		case SIGSYS:
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGIO:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGPROF:
			return _signal_terminate;

		case SIGURG:
		case SIGSTOP: /* TODO: Stop process. Should be handled by the kernel? */
		case SIGTSTP: /* TODO: Stop process. Should be handled by the kernel? */
		case SIGCONT: /* TODO: Continue process. Should be handled by the kernel? */
		case SIGCHLD:
		case SIGTTIN: /* TODO: Stop process. Should be handled by the kernel? */
		case SIGTTOU: /* TODO: Stop process. Should be handled by the kernel? */
		case SIGWINCH:
		case SIGINFO:
		case SIGUSR1:
		case SIGUSR2:
			return _signal_ignore;

		default:
			return NULL;
	}
}


static int _signal_ismutable(int sig)
{
	switch (sig) {
		case SIGKILL:
		case SIGSTOP:
			return 0;

		default:
			return 1;
	}
}


void _signal_handler(int sig)
{
	unsigned int oldmask;

	if (sig < 0 || sig >= NSIG) {
		/* Don't know what to do, ignore it */
		return;
	}

	oldmask = signalMask(signal_common.sigset[sig], 0xffffffffUL);

	/* Invoke handler */
	(signal_common.sightab[sig])(sig);

	signalMask(oldmask, 0xffffffffUL);
}


int raise(int sig)
{
	return SET_ERRNO(threadKill(getpid(), gettid(), sig));
}


int kill(pid_t pid, int sig)
{
	return SET_ERRNO(threadKill(pid, 0, sig));
}


int killpg(pid_t pgrp, int sig)
{
	if (!pgrp)
		pgrp = -getpgrp();
	else
		pgrp = -pgrp;

	return SET_ERRNO(threadKill(pgrp, 0, sig));
}


void (*signal(int signum, void (*handler)(int)))(int)
{
	struct sigaction sa, osa;
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(signum, &sa, &osa) == -1)
		return SIG_ERR;

	return osa.sa_handler;
}


/* TODO: Handle flags */
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	return SET_ERRNO(sys_sigaction(sig, act, oact));
}


int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	int i;
	unsigned int phxv = 0, mask = 0, old;

	if (set != NULL) {
		if (*set & (1UL << SIGKILL))
			return SET_ERRNO(-EINVAL);

		for (i = 0, phxv = 0; i < NSIG; ++i) {
			if (*set & (1UL << i))
				phxv |= 1UL << i;
		}

		if (how == SIG_BLOCK) {
			mask = phxv;
			phxv = 0xffffffffUL;
		}
		else if (how == SIG_UNBLOCK) {
			mask = phxv;
			phxv = 0;
		}
		else if (how == SIG_SETMASK) {
			mask = 0xffffffffUL;
		}
		else {
			return SET_ERRNO(-EINVAL);
		}
	}

	old = signalMask(phxv, mask);

	if (oldset != NULL) {
		memset(oldset, 0, sizeof(sigset_t));
		for (i = 0, phxv = 0; i < NSIG; ++i) {
			if (old & (1UL << i))
				(*oldset) |= 1UL << i;
		}
	}

	if (set == NULL && oldset == NULL)
		return SET_ERRNO(-EINVAL);

	return EOK;
}


int sigsuspend(const sigset_t *sigmask)
{
	unsigned int phxv = 0, i;

	for (i = 0, phxv = 0; i < NSIG; ++i) {
		if (*sigmask & (1UL << i))
			phxv |= 1UL << i;
	}

	return signalSuspend(phxv);
}


int sigfillset(sigset_t *set)
{
	return -ENOSYS;
}


int sigaddset(sigset_t *set, int signo)
{
	if (set == NULL || signo >= NSIG)
		return -1;

	*set |= 1 << signo;
	return 0;
}


int sigismember(const sigset_t *set, int signum)
{
	return !!(*set & (1 << signum));
}


int sigemptyset(sigset_t *set)
{
	if (set == NULL)
		return -1;

	memset(set, 0, sizeof(sigset_t));
	return 0;
}


int sigisemptyset(sigset_t *set)
{
	return -ENOSYS;
}


int sigdelset(sigset_t *set, int signum)
{
	if (set == NULL || signum >= NSIG)
		return -1;

	*set &= ~(1 << signum);
	return 0;
}


extern void _signal_trampoline(void);


void _signals_init(void)
{
	int i;

	/* Set default actions */
	for (i = 0; i < NSIG; ++i) {
		signal_common.sightab[i] = _signal_getdefault(i);
		signal_common.sigset[i] = 1UL << i;
	}

	mutexCreate(&signal_common.lock);

	/* Register userspace handler */
	signalHandle(_signal_trampoline, 0, 0xffffffffUL);
}
