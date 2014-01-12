## src/Makefile.am

sbin_PROGRAMS = btnx
AM_CFLAGS = -Wall -Werror -W -Wunused-parameter -Wstrict-prototypes \
-Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch \
-Wcast-align -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls \
`pkg-config --cflags libdaemon`
btnx_LDFLAGS = `pkg-config --libs libdaemon`

btnx_SOURCES = \
	btnx.c \
	config_parser.c \
	device.c \
	revoco.c \
	uinput.c \
## HEADERS
	btnx.h \
	config_parser.h \
	device.h \
	revoco.h \
	uinput.h

uninstall-local:
	@echo "Stopping any leftover btnx processes."
	if test -x "$(DESTDIR)$(sbindir)/$(PACKAGE)"; then $(DESTDIR)$(sbindir)/$(PACKAGE) -k; fi
