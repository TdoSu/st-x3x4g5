#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#if defined(__linux__)
#include <pty.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#include <util.h>
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#endif

#include "st.h"
#include "win.h"

char *utmp;
char *scroll;
char *stty_args = "";
char *vtiden = "\033[?6c";
wchar_t *worddelimiters = L" ";
int allowaltscreen = 1;
int allowwindowops = 0;
char *termname = "st-split-test";
unsigned int tabspaces = 8;
unsigned int defaultfg = 7;
unsigned int defaultbg = 0;
unsigned int defaultcs = 256;
const int boxdraw = 0;
const int boxdraw_bold = 0;
const int boxdraw_braille = 0;

static int
test_split_cwd(void)
{
	char actual[PATH_MAX], ready;
	char dirname[] = "/tmp/st-split-cwd.XXXXXXXX";
	pid_t child;
	int failed = 0, master;

	if (mkdtemp(dirname) == NULL) {
		perror("mkdtemp");
		return 1;
	}
	if ((child = forkpty(&master, NULL, NULL, NULL)) < 0) {
		perror("forkpty");
		rmdir(dirname);
		return 1;
	}
	if (child == 0) {
		if (chdir(dirname) < 0 || write(STDOUT_FILENO, "R", 1) != 1)
			_exit(2);
		pause();
		_exit(0);
	}

	if (read(master, &ready, 1) != 1 || ready != 'R') {
		fprintf(stderr, "FAIL: cwd child did not become ready\n");
		failed = 1;
	} else {
		tdebugsplitcwd(master, actual, sizeof actual);
		if (strcmp(actual, dirname) != 0) {
			fprintf(stderr, "FAIL: split cwd is %s, expected %s\n",
			        actual, dirname);
			failed = 1;
		}
	}

	kill(child, SIGTERM);
	waitpid(child, NULL, 0);
	close(master);
	rmdir(dirname);
	return failed;
}

int
main(void)
{
	struct timespec delay = { .tv_nsec = 10000000 };
	pid_t child;
	int fd, i;

	if (test_split_cwd())
		return 1;

	if ((fd = open("/dev/null", O_RDWR)) < 0) {
		perror("open /dev/null");
		return 1;
	}
	if ((child = fork()) < 0) {
		perror("fork");
		return 1;
	}
	if (child == 0)
		_exit(0);

	/* No SIGCHLD handler is installed: ttyreap() must still find the child. */
	tdebugsplittrack(fd, child);
	if (!tdebugsplitisopen()) {
		fprintf(stderr, "FAIL: live split setup was rejected\n");
		return 1;
	}
	for (i = 0; i < 100 && tdebugsplitisopen(); i++) {
		ttyreap();
		if (tdebugsplitisopen())
			nanosleep(&delay, NULL);
	}
	if (tdebugsplitisopen()) {
		fprintf(stderr, "FAIL: exited split child was left active\n");
		return 1;
	}

	puts("split-state: ok");
	return 0;
}

void xbell(void) {}
void xclipcopy(void) {}
void xcursorclear(void) {}
void xdrawcursor(int a, int b, Glyph c, int d, int e, Glyph f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void xdrawline(Line a, int b, int c, int d)
{ (void)a; (void)b; (void)c; (void)d; }
void xdrawseparator(int a) { (void)a; }
void xdrawappseparator(int a, int b, int c, int d)
{ (void)a; (void)b; (void)c; (void)d; }
void xfinishdraw(void) {}
void xcopypanestate(int a, int b) { (void)a; (void)b; }
void xresetpanestate(int a) { (void)a; }
void xsetpanemode(int a) { (void)a; }
void xsetrowoff(int a) { (void)a; }
void xloadcols(void) {}
int xsetcolorname(int a, const char *b) { (void)a; (void)b; return 0; }
int xgetcolor(int a, unsigned char *b, unsigned char *c, unsigned char *d)
{ (void)a; (void)b; (void)c; (void)d; return 0; }
int isboxdraw(Rune a) { (void)a; return 0; }
ushort boxdrawindex(const Glyph *a) { (void)a; return 0; }
void xseticontitle(char *a) { (void)a; }
void xsettitle(char *a) { (void)a; }
int xsetcursor(int a) { (void)a; return 0; }
void xsetmode(int a, unsigned int b) { (void)a; (void)b; }
void xsetpointermotion(int a) { (void)a; }
void xsetsel(char *a) { free(a); }
int xstartdraw(void) { return 0; }
void xximspot(int a, int b) { (void)a; (void)b; }
