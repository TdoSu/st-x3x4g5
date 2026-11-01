/* See LICENSE for license details. A supervisor, never a PTY owner. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>

static volatile sig_atomic_t stopped;

static void
onsignal(int signo)
{
	if (signo != SIGCHLD)
		stopped = 1;
}

static int
xerror(Display *display, XErrorEvent *event)
{
	/* The original window can disappear while the viewer is closing. */
	(void)display;
	(void)event;
	return 0;
}

int
main(int argc, char **argv)
{
	struct stat snapshot;
	struct sigaction sa = {0};
	struct pollfd life = { .fd = STDIN_FILENO, .events = POLLIN };
	struct timespec delay = { .tv_nsec = 10000000 };
	Display *display;
	XWindowAttributes attributes;
	Window original;
	XEvent activate = {0};
	pid_t child, result;
	int fd, status = 1, closing = 0, ticks = 0;
	char *end;

	if (argc != 6) {
		fprintf(stderr, "usage: st-scrollback-viewer snapshot window geometry font st\n");
		return 1;
	}
	/* Accept only the caller's private regular snapshot, never a symlink. */
	if (lstat(argv[1], &snapshot) < 0 || !S_ISREG(snapshot.st_mode) ||
	    snapshot.st_uid != getuid() || (snapshot.st_mode & 077) != 0) {
		fprintf(stderr, "st-scrollback-viewer: invalid private snapshot\n");
		return 1;
	}
	original = strtoul(argv[2], &end, 0);
	if (!original || *end) {
		unlink(argv[1]);
		return 1;
	}
	sa.sa_handler = onsignal;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGTERM, &sa, NULL) < 0 || sigaction(SIGHUP, &sa, NULL) < 0 ||
	    sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("st-scrollback-viewer: sigaction");
		unlink(argv[1]);
		return 1;
	}
	XSetErrorHandler(xerror);
	if (!(display = XOpenDisplay(NULL))) {
		fprintf(stderr, "st-scrollback-viewer: cannot open display\n");
		unlink(argv[1]);
		return 1;
	}
	if (!XGetWindowAttributes(display, original, &attributes) ||
	    fcntl(ConnectionNumber(display), F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(STDIN_FILENO, F_SETFD, FD_CLOEXEC) < 0) {
		XCloseDisplay(display);
		unlink(argv[1]);
		return 1;
	}
	child = stopped ? -1 : fork();
	if (child == 0) {
		/* Do not give the viewer st the parent's lifetime pipe or any old tty. */
		if ((fd = open("/dev/null", O_RDWR)) < 0 || dup2(fd, STDIN_FILENO) < 0)
			_exit(1);
		if (fd > 2)
			close(fd);
		if (setenv("ST_SCROLLBACK_PARENT", argv[2], 1) < 0)
			_exit(1);
		/* Select X11 even if this Xwayland session also exports WAYLAND_DISPLAY. */
		unsetenv("WAYLAND_DISPLAY");
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		execlp(argv[5], argv[5], "-n", "st-scrollback", "-t", "Scrollback (vis)",
		       "-g", argv[3], "-f", argv[4], "-e", "vis", "--scrollback", "--", argv[1],
		       (char *)NULL);
		perror("st-scrollback-viewer: st");
		_exit(127);
	}
	if (child < 0) {
		perror("st-scrollback-viewer: fork");
	} else {
		for (;;) {
			result = waitpid(child, &status, WNOHANG);
			if (result == child)
				break;
			if (result < 0 && errno != EINTR) {
				status = 1;
				break;
			}
			if (poll(&life, 1, 50) < 0 && errno != EINTR)
				stopped = 1;
			if (life.revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
				stopped = 1;
			if (stopped && !closing) {
				/* Viewer st handles this by hanging up only its own vis PTY. */
				kill(child, SIGTERM);
				closing = 1;
			}
			if (closing && ++ticks == 60)
				kill(child, SIGKILL);
			/* A closed pipe is always ready; keep cancellation waits bounded. */
			if (closing)
				nanosleep(&delay, NULL);
		}
	}
	unlink(argv[1]);
	if (!stopped && XGetWindowAttributes(display, original, &attributes) &&
	    attributes.map_state == IsViewable) {
		activate.xclient.type = ClientMessage;
		activate.xclient.window = original;
		activate.xclient.message_type = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
		activate.xclient.format = 32;
		activate.xclient.data.l[0] = 1; /* application request */
		activate.xclient.data.l[1] = CurrentTime;
		XSendEvent(display, DefaultRootWindow(display), False,
		           SubstructureRedirectMask | SubstructureNotifyMask, &activate);
		XRaiseWindow(display, original);
		XSetInputFocus(display, original, RevertToPointerRoot, CurrentTime);
		XSync(display, False);
	}
	XCloseDisplay(display);
	return child > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
