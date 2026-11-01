#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "st.h"
#include "win.h"

char *utmp;
char *scroll;
char *stty_args = "";
char *vtiden = "\033[?6c";
wchar_t *worddelimiters = L" ";
int allowaltscreen = 1;
int allowwindowops = 0;
char *termname = "st-wide-test";
unsigned int tabspaces = 8;
unsigned int defaultfg = 7;
unsigned int defaultbg = 0;
unsigned int defaultcs = 256;
const int boxdraw = 0;
const int boxdraw_bold = 0;
const int boxdraw_braille = 0;


static void
feed(const char *s)
{
	tdebugwidewrite(s, strlen(s));
}

static void
expect(const char *expected)
{
	FILE *file = tmpfile();
	char actual[65536];
	size_t n;
	if (!file || tsnapshot(fileno(file)) < 0) {
		perror("snapshot");
		exit(1);
	}
	rewind(file);
	n = fread(actual, 1, sizeof(actual) - 1, file);
	actual[n] = 0;
	fclose(file);
	if (strcmp(actual, expected)) {
		fprintf(stderr, "expected [%s], got [%s]\n", expected, actual);
		exit(1);
	}
}

static void
test_launcher(void)
{
	char directory[] = "/tmp/st-launcher-test.XXXXXXXX";
	char helper[PATH_MAX], alternate[PATH_MAX], bindir[PATH_MAX], terminal[PATH_MAX];
	char report[PATH_MAX], actual[PATH_MAX], snapshot[PATH_MAX], content[128];
	char *oldpath = getenv("PATH"), *oldtmp = getenv("TMPDIR");
	struct timespec delay = { .tv_nsec = 10000000 };
	struct stat st;
	FILE *file;
	int i, scenario, ready;

	oldpath = oldpath ? xstrdup(oldpath) : NULL;
	oldtmp = oldtmp ? xstrdup(oldtmp) : NULL;
	if (!mkdtemp(directory))
		goto fail;
	snprintf(helper, sizeof helper, "%s/st-launcher-fixture", directory);
	snprintf(bindir, sizeof bindir, "%s/bin", directory);
	snprintf(alternate, sizeof alternate, "%s/bin/st-launcher-fixture", directory);
	snprintf(terminal, sizeof terminal, "%s/st", directory);
	snprintf(report, sizeof report, "%s/report", directory);
	if (mkdir(bindir, 0700) < 0)
		goto fail;
	for (i = 0; i < 2; i++) {
		const char *path = i ? alternate : helper;
		if (!(file = fopen(path, "w")))
			goto fail;
		fputs("#!/bin/sh\nIFS= read -r line <\"$1\" || exit 1\n"
		      "printf '%s\\n%s\\n%s\\n' \"$0\" \"$1\" \"$line\" >\"$ST_LAUNCHER_TEST_REPORT\"\n", file);
		if (fclose(file) != 0 || chmod(path, 0700) < 0)
			goto fail;
	}
	if (setenv("TMPDIR", directory, 1) < 0 || setenv("ST_LAUNCHER_TEST_REPORT", report, 1) < 0)
		goto fail;
	tnew(24, 2);
	feed("launcher-snapshot");
	if (setenv("ST_SCROLLBACK_PARENT", "1", 1) < 0 ||
	    tscrollback("st-launcher-fixture", "1", "24x2", "monospace", terminal) != -1 || errno != ELOOP)
		goto fail;
	unsetenv("ST_SCROLLBACK_PARENT");
	if (setenv("ST_SCROLLBACK_LAUNCHER", "1", 1) < 0 ||
	    tscrollback("st-launcher-fixture", "1", "24x2", "monospace", terminal) != -1 || errno != ELOOP)
		goto fail;
	unsetenv("ST_SCROLLBACK_LAUNCHER");
	for (scenario = 0; scenario < 3; scenario++) {
		/* Missing PATH entry, PATH precedence, then an explicit helper path. */
		if (setenv("PATH", scenario == 1 ? bindir : "/nonexistent", 1) < 0 ||
		    tscrollback(scenario == 2 ? helper : "st-launcher-fixture",
		                "1", "24x2", "monospace", terminal) < 0)
			goto fail;
		ready = 0;
		for (i = 0; i < 200 && !ready; i++) {
			if ((file = fopen(report, "r"))) {
				ready = fgets(actual, sizeof actual, file) &&
				        fgets(snapshot, sizeof snapshot, file) &&
				        fgets(content, sizeof content, file);
				fclose(file);
			}
			if (!ready)
				nanosleep(&delay, NULL);
		}
		if (!ready)
			goto fail;
		actual[strcspn(actual, "\n")] = '\0';
		snapshot[strcspn(snapshot, "\n")] = '\0';
		if (strcmp(actual, scenario == 1 ? alternate : helper) ||
		    strncmp(content, "launcher-snapshot", strlen("launcher-snapshot")) ||
		    stat(snapshot, &st) < 0 || (st.st_mode & 0777) != 0400)
			goto fail;
		for (i = 0; i < 200 && access(snapshot, F_OK) == 0; i++) {
			ttyreap();
			nanosleep(&delay, NULL);
		}
		if (access(snapshot, F_OK) == 0 || unlink(report) < 0)
			goto fail;
	}
	if (oldpath) setenv("PATH", oldpath, 1); else unsetenv("PATH");
	if (oldtmp) setenv("TMPDIR", oldtmp, 1); else unsetenv("TMPDIR");
	unsetenv("ST_LAUNCHER_TEST_REPORT");
	free(oldpath);
	free(oldtmp);
	unlink(helper);
	unlink(alternate);
	rmdir(bindir);
	rmdir(directory);
	puts("scrollback launcher: sibling fallback, PATH precedence, explicit path, snapshot creation/cleanup ok");
	return;
fail:
	fprintf(stderr, "FAIL: scrollback launcher discovery or snapshot lifecycle\n");
	exit(1);
}

static void
test_styles(void)
{
	FILE *text = tmpfile(), *styles = tmpfile();
	char content[256], metadata[2048];
	size_t size;
	if (!text || !styles) exit(1);
	tnew(8, 5);
	feed("\033[38;2;255;0;0mA你\033[0m B");
	if (tsnapshotstyled(fileno(text), fileno(styles)) < 0) exit(1);
	rewind(text);
	size = fread(content, 1, sizeof content - 1, text);
	content[size] = 0;
	rewind(styles);
	size = fread(metadata, 1, sizeof metadata - 1, styles);
	metadata[size] = 0;
	if (strcmp(content, "A你 B   \n") ||
	    !strstr(metadata, "R\t0\t4\tfore:#ff0000,back:#000000\n") ||
	    !strstr(metadata, "V\t0\t6\t8\t1\n")) {
		fprintf(stderr, "FAIL: styled UTF-8 snapshot or cursor/unused-row export\n%s\n%s", content, metadata);
		exit(1);
	}
	fclose(text);
	fclose(styles);
	feed("\r\n\r\n");
	expect("A你 B   \n        \n        \n");
	puts("scrollback styles: RGB/UTF-8 byte ranges, cursor, unused rows and intentional blank lines ok");
}

int
main(void)
{
	char line[32], expected[65536] = "";
	int i;
	if (!setlocale(LC_CTYPE, "") || wcwidth(0x4f60) != 2)
		return 2;
	test_launcher();
	test_styles();
	tnew(6, 2);
	feed("ab你好Z");
	expect("ab你好Z     \n");
	tnew(6, 2);
	feed("  你好\r\nx");
	expect("  你好\nx     \n");
	/* Wide glyph at a wrap boundary, then history-to-screen continuation. */
	feed("yz0123456789");
	expect("  你好\nxyz0123456789     \n");
	kscrollup(&(Arg){.i = 1});
	expect("  你好\nxyz0123456789     \n");
	expect("  你好\nxyz0123456789     \n");
	feed("\033[?1049h\033[HALT");
	expect("  你好\nxyz012\nALT   \n");
	feed("\033[?1049l");
	expect("  你好\nxyz0123456789     \n");
	tnew(6, 2);
	for (i = 0; i < HISTSIZE + 4; i++) {
		snprintf(line, sizeof line, "%04d\r\n", i);
		feed(line);
	}
	for (i = 3; i < HISTSIZE + 4; i++) {
		snprintf(line, sizeof line, "%04d  \n", i);
		strcat(expected, line);
	}
	strcat(expected, "      \n");
	expect(expected);
	if (tsnapshot(-1) != -1)
		return 1;
	puts("scrollback snapshot: ok");
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
