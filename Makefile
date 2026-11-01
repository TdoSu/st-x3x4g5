# Common entry point for OpenBSD make and GNU make.
.POSIX:

CC = cc
TARGETS = all st st-scrollback-viewer st.o x.o boxdraw.o config.h clean dist install uninstall

$(TARGETS):
	+$(MAKE) -f config.mk -f build.mk $@

.PHONY: $(TARGETS)
