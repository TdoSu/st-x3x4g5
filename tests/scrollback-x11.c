#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void
msleep(long ms)
{
	struct timespec delay = { ms / 1000, ms % 1000 * 1000000L };

	while (nanosleep(&delay, &delay) < 0 && errno == EINTR)
		;
}

static Window
findwindow(Display *dpy, Window parent, pid_t pid)
{
	Window root, owner, *children = NULL, found = None;
	unsigned int i, nchildren = 0;
	Atom actual, pidatom;
	int format;
	unsigned long nitems, left;
	unsigned char *value = NULL;

	pidatom = XInternAtom(dpy, "_NET_WM_PID", False);
	if (XGetWindowProperty(dpy, parent, pidatom, 0, 1, False,
	                       XA_CARDINAL, &actual, &format, &nitems, &left,
	                       &value) == Success && value) {
		if (actual == XA_CARDINAL && format == 32 && nitems == 1 &&
		    *(unsigned long *)value == (unsigned long)pid)
			found = parent;
		XFree(value);
		if (found != None)
			return found;
	}
	if (!XQueryTree(dpy, parent, &root, &owner, &children, &nchildren))
		return None;
	for (i = 0; i < nchildren && found == None; i++)
		found = findwindow(dpy, children[i], pid);
	if (children)
		XFree(children);
	return found;
}

static Display *display;
static pid_t originals[2];
static Window requester;
static char *scratch;
static const char *stage = "setup";

static int
ignoreerror(Display *dpy, XErrorEvent *event)
{
	(void)dpy; (void)event;
	return 0;
}

static void
check(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL (%s): %s\n", stage, message);
		exit(1);
	}
}

static void
cleanup(void)
{
	int i;
	for (i = 0; i < 2; i++)
		if (originals[i] > 0) {
			kill(originals[i], SIGTERM);
			waitpid(originals[i], NULL, 0);
		}
	if (display) {
		XSetSelectionOwner(display, XInternAtom(display, "CLIPBOARD", False), None, CurrentTime);
		XSync(display, False);
		XCloseDisplay(display);
	}
}

static void
sendkey(Window window, KeySym sym, unsigned int state)
{
	XEvent event = {0};
	event.xkey.type = KeyPress;
	event.xkey.display = display;
	event.xkey.window = window;
	event.xkey.root = DefaultRootWindow(display);
	event.xkey.same_screen = True;
	event.xkey.keycode = XKeysymToKeycode(display, sym);
	event.xkey.state = state;
	check(XSendEvent(display, window, False, KeyPressMask, &event), "send key");
	XFlush(display);
}

static void
sendtext(Window window, const char *text)
{
	for (; *text; text++) {
		KeySym sym = *text == '\n' ? XK_Return : (unsigned char)*text;
		unsigned int state = (*text == ':' || (*text >= 'A' && *text <= 'Z')) ? ShiftMask : 0;
		sendkey(window, sym, state);
	}
	msleep(150);
}

static Window
viewerwindow(Window parent, Window original, int *count)
{
	Window root, owner, transient, *children = NULL, found = None, sub;
	unsigned int i, nchildren = 0;
	if (XGetTransientForHint(display, parent, &transient) && transient == original) {
		(*count)++;
		found = parent;
	}
	if (XQueryTree(display, parent, &root, &owner, &children, &nchildren)) {
		for (i = 0; i < nchildren; i++) {
			sub = viewerwindow(children[i], original, count);
			if (sub) found = sub;
		}
		if (children) XFree(children);
	}
	return found;
}

static Window
waitviewer(Window original, int expected)
{
	int i, count;
	Window found;
	XWindowAttributes attributes;
	for (i = 0; i < 150; i++) {
		count = 0;
		found = viewerwindow(DefaultRootWindow(display), original, &count);
		check(count <= 1, "at most one viewer per original");
		if (count == expected && (!expected ||
		    (XGetWindowAttributes(display, found, &attributes) && attributes.map_state == IsViewable)))
			return found;
		msleep(20);
	}
	check(0, expected ? "viewer opened" : "viewer closed");
	return None;
}

static int
snapshots(char *content, size_t size)
{
	DIR *dir = opendir(scratch);
	struct dirent *entry;
	struct stat st;
	char path[4096];
	FILE *file;
	int count = 0;
	size_t n;
	check(dir != NULL, "open scratch");
	while ((entry = readdir(dir))) {
		if (strncmp(entry->d_name, "st-scrollback.", 14)) continue;
		count++;
		snprintf(path, sizeof path, "%s/%s", scratch, entry->d_name);
		check(stat(path, &st) == 0 && (st.st_mode & 0777) == 0400, "private read-only snapshot");
		if (content) {
			file = fopen(path, "r");
			check(file != NULL, "read snapshot");
			n = fread(content, 1, size - 1, file);
			content[n] = 0;
			fclose(file);
		}
	}
	closedir(dir);
	return count;
}

/* A test xclip provider serving real X11 selections from a separate process.
 * The installed vis-clipboard script invokes this via a test-only PATH entry. */
static int
clipboardprovider(void)
{
	char content[65536];
	size_t n = 0;
	ssize_t bytes;
	int ready[2];
	pid_t child;
	Display *dpy;
	Window owner;
	Atom clipboard, utf8, targets;
	XEvent event, reply;
	while (n < sizeof content && (bytes = read(0, content + n, sizeof content - n)) > 0)
		n += bytes;
	if (pipe(ready) < 0 || (child = fork()) < 0) return 1;
	if (child > 0) {
		close(ready[1]);
		bytes = read(ready[0], content, 1);
		close(ready[0]);
		return bytes == 1 ? 0 : 1;
	}
	close(ready[0]);
	setsid();
	if (!(dpy = XOpenDisplay(NULL))) _exit(1);
	owner = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);
	clipboard = XInternAtom(dpy, "CLIPBOARD", False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);
	targets = XInternAtom(dpy, "TARGETS", False);
	XSetSelectionOwner(dpy, clipboard, owner, CurrentTime);
	XSync(dpy, False);
	write(ready[1], "R", 1);
	close(ready[1]);
	for (;;) {
		XNextEvent(dpy, &event);
		if (event.type == SelectionClear) break;
		if (event.type != SelectionRequest) continue;
		XSelectionRequestEvent *request = &event.xselectionrequest;
		Atom property = request->property ? request->property : request->target;
		reply = (XEvent){0};
		reply.xselection = (XSelectionEvent){ .type = SelectionNotify,
			.display = dpy, .requestor = request->requestor, .selection = clipboard,
			.target = request->target, .property = property, .time = request->time };
		if (request->target == targets) {
			Atom supported[] = { targets, utf8, XA_STRING };
			XChangeProperty(dpy, request->requestor, property, XA_ATOM, 32,
			                PropModeReplace, (unsigned char *)supported, 3);
		} else if (request->target == utf8 || request->target == XA_STRING) {
			XChangeProperty(dpy, request->requestor, property, request->target, 8,
			                PropModeReplace, (unsigned char *)content, n);
		} else reply.xselection.property = None;
		XSendEvent(dpy, request->requestor, False, 0, &reply);
		XFlush(dpy);
	}
	XCloseDisplay(dpy);
	_exit(0);
}

static void
expectclipboard(const char *expected)
{
	Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
	Atom utf8 = XInternAtom(display, "UTF8_STRING", False), actual;
	Atom property = XInternAtom(display, "ST_TEST_PASTE", False);
	XEvent event;
	int i, format;
	unsigned long n, left;
	unsigned char *value = NULL;
	/* Model a real destination application. Xwayland may bridge selections
	 * differently when the requesting client has no mapped/focused window. */
	XMapRaised(display, requester);
	XSetInputFocus(display, requester, RevertToPointerRoot, CurrentTime);
	XConvertSelection(display, clipboard, utf8, property, requester, CurrentTime);
	XFlush(display);
	for (i = 0; i < 100; i++) {
		while (XPending(display)) {
			XNextEvent(display, &event);
			if (event.type != SelectionNotify) continue;
			if (event.xselection.property != property)
				fprintf(stderr, "clipboard owner=%lu\n", XGetSelectionOwner(display, clipboard));
			check(event.xselection.property == property, "clipboard conversion");
			check(XGetWindowProperty(display, requester, property, 0, 16384, True,
			                         utf8, &actual, &format, &n, &left, &value) == Success,
			      "clipboard property");
			check(value && n == strlen(expected) && !memcmp(value, expected, n),
			      "clipboard survives viewer exit with exact selected text");
			XFree(value);
			return;
		}
		msleep(20);
	}
	check(0, "clipboard response timeout");
}

static void
failprogram(const char *name, int create)
{
	char path[4096];
	FILE *file;
	snprintf(path, sizeof path, "%s/../bin/%s", scratch, name);
	if (!create) {
		check(unlink(path) == 0, "remove failure fixture");
		return;
	}
	file = fopen(path, "w");
	check(file != NULL, "create failure fixture");
	fputs("#!/bin/sh\nexit 7\n", file);
	fclose(file);
	check(chmod(path, 0700) == 0, "executable failure fixture");
}

static void
killlauncher(Window viewer)
{
	Atom actual;
	int format;
	unsigned long n, left;
	unsigned char *value = NULL;
	char command[128];
	long launcher;
	FILE *file;
	check(XGetWindowProperty(display, viewer, XInternAtom(display, "_NET_WM_PID", False),
	                         0, 1, False, XA_CARDINAL, &actual, &format, &n, &left,
	                         &value) == Success && value && n == 1, "viewer pid");
	snprintf(command, sizeof command, "ps -o ppid= -p %lu", *(unsigned long *)value);
	XFree(value);
	file = popen(command, "r");
	check(file && fscanf(file, "%ld", &launcher) == 1 && launcher > 1, "launcher pid");
	pclose(file);
	check(kill((pid_t)launcher, SIGKILL) == 0, "simulate launcher crash");
}

static int
foreground(const char *report, int alt)
{
	struct termios saved, raw;
	char path[4096], byte;
	FILE *file;
	int received = 0;
	if (alt) fputs("\033[?1049h\033[H\033[2JFULLSCREEN-ACTIVE", stdout);
	fflush(stdout);
	if (tcgetattr(0, &saved) < 0) return 1;
	raw = saved;
	cfmakeraw(&raw);
	if (tcsetattr(0, TCSANOW, &raw) < 0) return 1;
	snprintf(path, sizeof path, "%s.ready", report);
	file = fopen(path, "w");
	if (!file) return 1;
	fputs("ready", file);
	fclose(file);
	while (read(0, &byte, 1) == 1 && byte != 'z') received++;
	tcsetattr(0, TCSANOW, &saved);
	if (alt) fputs("\033[?1049l", stdout);
	file = fopen(report, "w");
	if (!file) return 1;
	fprintf(file, "%d\n", received);
	fclose(file);
	return received != 0;
}

int
main(int argc, char **argv)
{
	Window windows[2], viewer, focus;
	char reports[2][4096], ready[8192], content[65536], expected[128];
	FILE *file;
	int i, j, k, revert, status, bytes;
	if (strstr(argv[0], "xclip")) return clipboardprovider();
	if (argc == 4 && !strcmp(argv[1], "--foreground"))
		return foreground(argv[2], atoi(argv[3]));
	check(argc == 4, "usage: scrollback-x11 self shell-script scratch");
	scratch = argv[3];
	check(setenv("TMPDIR", scratch, 1) == 0, "set scratch");
	check((display = XOpenDisplay(NULL)) != NULL, "open X display");
	XSetErrorHandler(ignoreerror);
	fcntl(ConnectionNumber(display), F_SETFD, FD_CLOEXEC);
	requester = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 200, 40, 0, 0, 0);
	XStoreName(display, requester, "Scrollback clipboard acceptance target");
	XMapWindow(display, requester);
	atexit(cleanup);
	for (i = 0; i < 2; i++) {
		snprintf(reports[i], sizeof reports[i], "%s/report%d", scratch, i);
		originals[i] = fork();
		check(originals[i] >= 0, "fork original st");
		if (!originals[i]) {
			execl("./st", "./st", "-f", "monospace:pixelsize=14", "-g", "48x10+40+40",
			      "-e", "ksh", argv[2], argv[1], reports[i], i ? "1" : "0", (char *)NULL);
			_exit(127);
		}
		windows[i] = None;
		snprintf(ready, sizeof ready, "%s.ready", reports[i]);
		for (j = 0; j < 200; j++) {
			windows[i] = findwindow(display, DefaultRootWindow(display), originals[i]);
			if (windows[i] && access(ready, F_OK) == 0) break;
			msleep(20);
		}
		check(windows[i] && access(ready, F_OK) == 0, "ksh foreground ready");
	}
	if (getenv("SCROLLBACK_TEST_LEGACY")) {
		for (i = 0; i < 2; i++) {
			sendkey(windows[i], XK_q, Mod1Mask);
			sendtext(windows[i], "vhlq");
			waitviewer(windows[i], 0);
			check(snapshots(NULL, 0) == 0, "legacy mode creates no snapshot");
			sendtext(windows[i], "z");
			check(waitpid(originals[i], &status, 0) == originals[i] &&
			      WIFEXITED(status) && WEXITSTATUS(status) == 0, "legacy original session preserved");
			originals[i] = 0;
			file = fopen(reports[i], "r");
			check(file && fscanf(file, "%d", &bytes) == 1 && bytes == 0,
			      "legacy copy motions and Alt-q stay out of original PTY");
			fclose(file);
		}
		puts("scrollback X11: legacy toggle, copy selection/motions and original PTY preservation passed");
		return 0;
	}
	/* Two independent st instances can each hold one viewer simultaneously. */
	stage = "independent viewers";
	for (i = 0; i < 2; i++) {
		sendkey(windows[i], XK_q, Mod1Mask);
		waitviewer(windows[i], 1);
	}
	check(snapshots(NULL, 0) == 2, "two independent snapshots");
	for (i = 0; i < 2; i++) {
		viewer = waitviewer(windows[i], 1);
		msleep(400);
		for (k = 0; k < 4; k++) sendkey(windows[i], XK_q, Mod1Mask);
		msleep(150);
		waitviewer(windows[i], 1);
		sendtext(viewer, "gg/HISTORY-0003\n0v11ly");
		msleep(250);
		sendtext(viewer, ":q\n");
		waitviewer(windows[i], 0);
		expectclipboard("HISTORY-0003");
	}
	msleep(150);
	check(snapshots(NULL, 0) == 0, "snapshots removed after :q");
	for (i = 0; i < 2; i++) {
		stage = "repeated activation";
		for (j = 0; j < 3; j++) {
			sendkey(windows[i], XK_q, Mod1Mask);
			viewer = waitviewer(windows[i], 1);
			check(snapshots(content, sizeof content) == 1, "single private snapshot");
			check(strstr(content, "HISTORY-0003") != NULL, "history exported");
			check(strstr(content, i ? "FULLSCREEN-ACTIVE" : "CURRENT-SCREEN") != NULL, "active screen exported");
			msleep(350);
			sendtext(viewer, ":q\n");
			waitviewer(windows[i], 0);
			msleep(150);
			check(snapshots(NULL, 0) == 0, "repeated workflow cleanup");
			check(kill(originals[i], 0) == 0, "original session alive");
			for (k = 0; k < 50; k++) {
				XGetInputFocus(display, &focus, &revert);
				if (focus == windows[i]) break;
				msleep(20);
			}
			if (focus != windows[i])
				fprintf(stderr, "focus=%lu, original=%lu\n", focus, windows[i]);
			check(focus == windows[i], "focus returned to original st");
			expectclipboard("HISTORY-0003");
		}
		if (i == 0) {
			stage = "startup failure";
			/* Startup failures clean up and do not disable future activation. */
			const char *programs[] = { "st-scrollback-viewer", "vis" };
			for (j = 0; j < 2; j++) {
				failprogram(programs[j], 1);
				sendkey(windows[i], XK_q, Mod1Mask);
				msleep(900);
				waitviewer(windows[i], 0);
				check(snapshots(NULL, 0) == 0, "failed startup cleanup");
				failprogram(programs[j], 0);
			}
			sendkey(windows[i], XK_q, Mod1Mask);
			viewer = waitviewer(windows[i], 1);
			msleep(300);
			stage = "launcher crash";
			killlauncher(viewer);
			waitviewer(windows[i], 0);
			msleep(150);
			check(snapshots(NULL, 0) == 0, "launcher crash cleanup");
			/* Closing the temporary window is cancellation, independent of :q. */
			stage = "window cancellation";
			sendkey(windows[i], XK_q, Mod1Mask);
			viewer = waitviewer(windows[i], 1);
			XEvent close = {0};
			close.xclient.type = ClientMessage;
			close.xclient.window = viewer;
			close.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
			close.xclient.format = 32;
			close.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
			XSendEvent(display, viewer, False, NoEventMask, &close);
			XFlush(display);
			waitviewer(windows[i], 0);
			msleep(150);
			check(snapshots(NULL, 0) == 0, "cancel cleanup");
			expectclipboard("HISTORY-0003");
		} else {
			stage = "original exit";
			/* Leave a viewer open when its original terminal exits. */
			sendkey(windows[i], XK_q, Mod1Mask);
			waitviewer(windows[i], 1);
		}
		sendtext(windows[i], "z");
		check(waitpid(originals[i], &status, 0) == originals[i] && WIFEXITED(status) &&
		      WEXITSTATUS(status) == 0, "original ksh exits normally");
		originals[i] = 0;
		waitviewer(windows[i], 0);
		msleep(150);
		check(snapshots(NULL, 0) == 0, "original exit cleanup");
		expectclipboard("HISTORY-0003");
		file = fopen(reports[i], "r");
		check(file && fscanf(file, "%d", &bytes) == 1 && bytes == 0, "Alt-q never reached foreground PTY");
		fclose(file);
		snprintf(ready, sizeof ready, "%s.state", reports[i]);
		file = fopen(ready, "r");
		check(file && fgets(expected, sizeof expected, file), "read shell state");
		check(!strcmp(expected, "unchanged\n"), "ksh cwd, environment and PID preserved");
		fclose(file);
	}
	puts("scrollback X11: ksh, fullscreen, copy/paste after :q, focus, isolation, repetition, failure/cancel/crash/parent-exit cleanup passed");
	return 0;
}
