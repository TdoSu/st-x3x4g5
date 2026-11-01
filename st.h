/* See LICENSE for license details. */

#include <stdint.h>
#include <sys/select.h>
#include <sys/types.h>

/* macros */
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#define MAX(a, b)		((a) < (b) ? (b) : (a))
#define LEN(a)			(sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)	((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)		(((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)		(a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)		(x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define ATTRCMP(a, b)		((a).mode != (b).mode || (a).fg != (b).fg || \
				(a).bg != (b).bg)
#define TIMEDIFF(t1, t2)	((t1.tv_sec-t2.tv_sec)*1000 + \
				(t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)	((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))

#define TRUECOLOR(r,g,b)	(1 << 24 | (r) << 16 | (g) << 8 | (b))
#define IS_TRUECOL(x)		(1 << 24 & (x))
#define HISTSIZE		2000

enum glyph_attribute {
	ATTR_NULL       = 0,
	ATTR_BOLD       = 1 << 0,
	ATTR_FAINT      = 1 << 1,
	ATTR_ITALIC     = 1 << 2,
	ATTR_UNDERLINE  = 1 << 3,
	ATTR_BLINK      = 1 << 4,
	ATTR_REVERSE    = 1 << 5,
	ATTR_INVISIBLE  = 1 << 6,
	ATTR_STRUCK     = 1 << 7,
	ATTR_WRAP       = 1 << 8,
	ATTR_WIDE       = 1 << 9,
	ATTR_WDUMMY     = 1 << 10,
	ATTR_BOXDRAW    = 1 << 11,
	ATTR_BOLD_FAINT = ATTR_BOLD | ATTR_FAINT,
};

enum keyboard_flags {
	KBD_DISAMBIGUATE    = 1u << 0,
	KBD_REPORT_EVENTS   = 1u << 1,
	KBD_ALTERNATE_KEYS  = 1u << 2,
	KBD_ALL_KEYS        = 1u << 3,
	KBD_ASSOCIATED_TEXT = 1u << 4,
};

enum drawing_mode {
    DRAW_NONE = 0,
    DRAW_BG = 1 << 0,
    DRAW_FG = 1 << 1,
};

enum selection_mode {
	SEL_IDLE = 0,
	SEL_EMPTY = 1,
	SEL_READY = 2
};

enum selection_type {
	SEL_REGULAR = 1,
	SEL_RECTANGULAR = 2
};

enum selection_snap {
	SNAP_WORD = 1,
	SNAP_LINE = 2
};

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

typedef uint_least32_t Rune;

#define Glyph Glyph_
typedef struct {
	Rune u;           /* character code */
	ushort mode;      /* attribute flags */
	uint32_t fg;      /* foreground  */
	uint32_t bg;      /* background  */
} Glyph;

typedef Glyph *Line;

typedef union {
	int i;
	uint ui;
	float f;
	const void *v;
	const char *s;
} Arg;

enum copymode_move {
	COPYMODE_LEFT = 0,
	COPYMODE_RIGHT,
	COPYMODE_UP,
	COPYMODE_DOWN,
	COPYMODE_HALFPAGEUP,
	COPYMODE_HALFPAGEDOWN,
	COPYMODE_TOP,
	COPYMODE_BOTTOM,
	COPYMODE_WORDNEXT,
	COPYMODE_WORDBACK,
	COPYMODE_BLANKUP,
	COPYMODE_BLANKDOWN,
};

void die(const char *, ...);
void redraw(void);
void draw(void);

int copymode_active(void);
void copymode_deselect(const Arg *);
void copymode_move(const Arg *);
void copymode_quit(const Arg *);
void copymode_select(const Arg *);
void copymode_selectline(const Arg *);
void copymode_toggle(const Arg *);
void kscrolldown(const Arg *);
void kscrollup(const Arg *);
void printscreen(const Arg *);
void printsel(const Arg *);
void sendbreak(const Arg *);
void splitadjust(const Arg *);
void splitfocus(const Arg *);
void splitrunselection(const Arg *);
void splittoggle(const Arg *);
void toggleprinter(const Arg *);

int tattrset(int);
void tnew(int, int);
void tresize(int, int);
int tsnapshot(int);
int tsnapshotstyled(int, int);
int tscrollback(const char *, const char *, const char *, const char *, const char *);
void tsetdirtattr(int);
void ttyhangup(void);
int ttynew(const char *, char *, const char *, char **);
void ttyreap(void);
int ttyfdset(fd_set *);
int ttyreadfds(fd_set *);
size_t ttyread(void);
void ttyresize(int, int);
void ttywrite(const char *, size_t, int);
unsigned int tkeyboardflags(void);
void tkeyboardreset(void);

#ifdef DEBUG_WIDE
int tdebugwidewrite(const char *, int);
int tdebugwidecell(int, int, Glyph *);
int tdebugwidevalid(int);
#endif

#ifdef DEBUG_SPLIT
void tdebugsplitcwd(int, char *, size_t);
void tdebugsplittrack(int, pid_t);
int tdebugsplitisopen(void);
#endif

int splitactive(void);
int splitfocused(void);
void splitfocusrow(int);
int splitrow(int);
int splitrowoff(void);
void splitresize(int, int, int, int);

void resettitle(void);

void selclear(void);
void selinit(void);
void selstart(int, int, int);
void selextend(int, int, int, int);
int selected(int, int);
char *getsel(void);

size_t utf8encode(Rune, char *);

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

/* config.h globals */
extern char *utmp;
extern char *scroll;
extern char *stty_args;
extern char *vtiden;
extern wchar_t *worddelimiters;
extern int allowaltscreen;
extern int allowwindowops;
extern int disableda;
extern char *termname;
extern unsigned int tabspaces;
extern unsigned int defaultfg;
extern unsigned int defaultbg;
extern unsigned int defaultcs;
extern const int boxdraw, boxdraw_bold, boxdraw_braille;
