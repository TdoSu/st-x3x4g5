#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "st.h"
#include "win.h"

char *utmp, *scroll;
char *stty_args = "", *vtiden = "\033[?6c", *termname = "st-divider-test";
wchar_t *worddelimiters = L" ";
int allowaltscreen = 1, allowwindowops;
unsigned int tabspaces = 8, defaultfg = 7, defaultbg, defaultcs = 256;
const int boxdraw = 0, boxdraw_bold = 0, boxdraw_braille = 0;
typedef struct { int x, y, length, vertical; } TestLine;
static TestLine drawn[256];
static int drawn_count;

static void feed(const char *s) { tdebugwidewrite(s, strlen(s)); }
static void expect(int ok, const char *message) {
 if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static void frame(int count) {
 drawn_count = 0;
 draw();
 expect(drawn_count == count, "native rectangle count");
}
#define OSC "\033]777;st-separators;"
#define BEL "\007"

int main(void) {
 Glyph glyph;
 tnew(80, 24);
 feed("text");
 feed(OSC "begin" BEL OSC "add;v;39;0;24" BEL);
 frame(0);
 feed(OSC "end" BEL);
 frame(1);
 expect(drawn[0].x == 39 && drawn[0].length == 24 && drawn[0].vertical,
        "rectangle geometry");
 expect(tdebugwidecell(39, 0, &glyph) && glyph.u == ' ', "divider is not a text glyph");
 expect(tdebugwidecell(0, 0, &glyph) && glyph.u == 't', "text stays intact");
 const char *invalid[] = {
  "add;v;-1;0;2", "add;v;80;0;2", "add;h;0;24;2", "add;h;0;0;0",
  "add;h;0;0;81", "add;v;0;0;25", "add;z;0;0;1", "add;h;x;0;1",
  "add;h;999999999999999999999999999;0;1", "add;h;1;0", "add;h;1;0;2;3"
 };
 for (unsigned int i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
  feed(OSC "begin" BEL);
  feed(OSC); feed(invalid[i]); feed(BEL);
  feed(OSC "end" BEL);
  frame(1);
  expect(drawn[0].x == 39, "invalid frame preserves previous rectangles");
 }
 feed(OSC "begin" BEL);
 for (int i = 0; i < 257; i++) feed(OSC "add;h;0;0;1" BEL);
 feed(OSC "end" BEL);
 frame(1);
 expect(drawn[0].x == 39, "overflow frame is rejected atomically");
 feed("\033[?1049h");
 frame(0);
 feed(OSC "begin" BEL OSC "add;h;0;3;80" BEL OSC "end" BEL);
 frame(1);
 feed("\033[?1049l");
 frame(1);
 expect(drawn[0].vertical, "main-screen graphics restored independently");
 feed("\033[?1049h");
 frame(0);
 feed("\033[?1049l");
 feed(OSC "clear" BEL);
 frame(0);
 feed(OSC "begin" BEL OSC "add;v;2;1;23" BEL OSC "end" BEL);
 tresize(8, 5);
 frame(1);
 expect(drawn[0].length == 4, "resize clips rectangle to pane bounds");
 feed(OSC "begin" BEL OSC "end" BEL);
 frame(0);
 feed(OSC "begin" BEL OSC "add;h;0;0;8" BEL OSC "end" BEL);
 frame(1);
 feed("\033c");
 frame(0);
 puts("native separators: atomic frames, invalid input, overflow, text isolation, alternate screen, resize and reset passed");
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
void xdrawappseparator(int x, int y, int length, int vertical)
{
 if (drawn_count < 256) drawn[drawn_count] = (TestLine){x, y, length, vertical};
 drawn_count++;
}
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
int xstartdraw(void) { return 1; }
void xximspot(int a, int b) { (void)a; (void)b; }
