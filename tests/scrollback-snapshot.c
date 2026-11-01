#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int
main(void)
{
	char line[32], expected[65536] = "";
	int i;
	if (!setlocale(LC_CTYPE, "") || wcwidth(0x4f60) != 2)
		return 2;
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
	expect("  你好\nxyz012\nALT   \n      \n");
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
