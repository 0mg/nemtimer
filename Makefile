# exe name
all = nemtimer.exe

# Library names
LIBS = kernel32 shell32 user32 gdi32 ole32 powrprof comctl32 comdlg32

# GCC
GCCFLAGS = $(AR:%=-o $@ -DUNICODE -mwindows -nostartfiles -s ${LIBS:%=-l%})
GCRC = $(AR:%=windres)
GCRFLAGS = $(AR:%=-o $*.o)
GCRM = $(AR:%=rm -f)

# VS
_LIBS = $(LIBS) %
_VSLIBS = $(_LIBS: =.lib )
_VSFLAGS = /utf-8 /DUNICODE /Ox /link /ENTRY:__start__ $(_VSLIBS:%=)
# /manifestdependency:"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'"
_VSRC = rc
_VSRFLAGS = /fo $*.o
_VSRM = del /f
VSFLAGS = $(_VSFLAGS:%=)
VSRC = $(_VSRC:%=)
VSRFLAGS = $(_VSRFLAGS:%=)
VSRM = $(_VSRM:%=)

# GCC or VS
CFLAGS = $(GCCFLAGS) $(VSFLAGS)
RC = $(GCRC) $(VSRC)
RFLAGS = $(GCRFLAGS) $(VSRFLAGS)
RM = $(GCRM) $(VSRM)

# main
.PHONY: all
all: $(all) clean

.SUFFIXES: .exe .o .rc .cpp

$(all): $(all:.exe=.o)
	$(CC) $*.o $*.cpp $(CFLAGS)

$(all:.exe=.o): $(all:.exe=.rc)
	$(RC) $(RFLAGS) $*.rc

clean:
	$(RM) *.obj *.o
