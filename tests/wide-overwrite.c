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

static int failures;

static void
feed(const char *s)
{
	tdebugwidewrite(s, strlen(s));
}

static Glyph
cell(int x, int y)
{
	Glyph gp;

	if (!tdebugwidecell(x, y, &gp)) {
		fprintf(stderr, "invalid cell %d,%d\n", x, y);
		exit(1);
	}
	return gp;
}

static void
expect(int condition, const char *name)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", name);
		failures++;
	}
}

static void
expectpair(int x, int y, Rune u, const char *name)
{
	Glyph lead = cell(x, y);
	Glyph dummy = cell(x+1, y);

	expect(lead.u == u && (lead.mode & ATTR_WIDE), name);
	expect(dummy.mode & ATTR_WDUMMY, name);
}

static void
resetterm(int col, int row)
{
	tnew(col, row);
}

static void
test_regression_sequence(void)
{
	resetterm(12, 2);
	feed(" \033[1m好\033[0m\033[3D你 ");
	expect(tdebugwidevalid(0), "reported overwrite invariant");
	expectpair(0, 0, 0x4f60, "reported overwrite pair");
	expect(cell(2, 0).u == ' ', "reported overwrite adjacent ASCII");
}

static void
test_attribute_overlap(void)
{
	static const char *const sgr[] = { "1", "7", "4", "31" };
	char buf[80];
	size_t i;

	for (i = 0; i < LEN(sgr); i++) {
		resetterm(12, 2);
		snprintf(buf, sizeof(buf), " \033[%sm好\033[0m\033[3D你 ", sgr[i]);
		feed(buf);
		expect(tdebugwidevalid(0), "attributed overlap invariant");
		expectpair(0, 0, 0x4f60, "attributed overlap pair");
		expect(cell(2, 0).u == ' ', "attributed overlap adjacent cell");
	}
}

static void
test_overwrite_halves(void)
{
	resetterm(8, 2);
	feed("你\033[2DA");
	expect(cell(0, 0).u == 'A', "overwrite lead writes ASCII");
	expect(!(cell(1, 0).mode & ATTR_WDUMMY), "overwrite lead clears dummy");
	expect(tdebugwidevalid(0), "overwrite lead invariant");

	resetterm(8, 2);
	feed("你\033[1DA");
	expect(!(cell(0, 0).mode & ATTR_WIDE), "overwrite dummy clears lead");
	expect(cell(1, 0).u == 'A', "overwrite dummy writes ASCII");
	expect(tdebugwidevalid(0), "overwrite dummy invariant");
}

static void
test_wide_over_wide(void)
{
	resetterm(10, 2);
	feed("好\033[2D你");
	expectpair(0, 0, 0x4f60, "wide exact overlap");

	resetterm(10, 2);
	feed(" 好\033[3D你");
	expectpair(0, 0, 0x4f60, "wide overlap one cell left");
	expect(cell(2, 0).u == ' ', "left overlap clears old dummy");

	resetterm(10, 2);
	feed("好\033[1D你");
	expect(!(cell(0, 0).mode & ATTR_WIDE), "wide overlap right clears old lead");
	expectpair(1, 0, 0x4f60, "wide overlap one cell right");
}

static void
test_boundaries(void)
{
	resetterm(6, 3);
	feed("你");
	expectpair(0, 0, 0x4f60, "column zero");

	resetterm(6, 3);
	feed("\033[1;5H你");
	expectpair(4, 0, 0x4f60, "penultimate column");

	resetterm(6, 3);
	feed("\033[1;6H你");
	expectpair(0, 1, 0x4f60, "last column with wrap");

	resetterm(6, 3);
	feed("\033[?7l\033[1;6H你");
	expectpair(4, 0, 0x4f60, "last column without wrap");

	resetterm(1, 2);
	feed("你");
	expect(!(cell(0, 1).mode & (ATTR_WIDE | ATTR_WDUMMY)),
	       "single column has no orphan wide flag");
	expect(tdebugwidevalid(1), "single column invariant");
}

static void
test_mixed(void)
{
	resetterm(12, 2);
	feed("A你B好C");
	expect(cell(0, 0).u == 'A' && cell(3, 0).u == 'B' &&
	       cell(6, 0).u == 'C', "mixed initial ASCII");
	feed("\033[1;3HX");
	expect(cell(0, 0).u == 'A' && cell(3, 0).u == 'B' &&
	       cell(6, 0).u == 'C', "mixed dummy overwrite neighbors");
	feed("\033[1;2H你\033[1;5HY");
	expect(cell(0, 0).u == 'A' && cell(3, 0).u == 'B' &&
	       cell(6, 0).u == 'C', "mixed repeated overwrite neighbors");
	expect(tdebugwidevalid(0), "mixed invariant");
}

int
main(void)
{
	if (!setlocale(LC_CTYPE, "") || wcwidth(0x4f60) != 2) {
		fprintf(stderr, "UTF-8 locale with wcwidth(CJK) == 2 required\n");
		return 2;
	}

	test_regression_sequence();
	test_attribute_overlap();
	test_overwrite_halves();
	test_wide_over_wide();
	test_boundaries();
	test_mixed();

	if (failures)
		return 1;
	puts("wide-overwrite: ok");
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
