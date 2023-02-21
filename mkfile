</$objtype/mkfile

RC=/rc/bin
BIN=/$objtype/bin
MAN=/sys/man

ALL=hist histw

all:V:	$ALL

hist:		hist.$O
	$LD $LDFLAGS -o hist hist.$O

hist.$O:	hist.c
	$CC $CFLAGS hist.c

histw:		histw.$O
	$LD $LDFLAGS -o histw histw.$O

histw.$O:	histw.c
	$CC $CFLAGS histw.c

clean:V:
	rm -f *.$O hist
	rm -f *.$O histw

install:V:
	mv hist $BIN/
	mv histw $BIN/
	cp savehist $RC/
	
	cp man/1/hist $MAN/1/
	cp man/1/histw $MAN/1/
	cp man/1/savehist $MAN/1/

uninstall:V:
	rm -f $BIN/hist
	rm -f $BIN/histw
	rm -f $RC/savehist
	
	rm -f $MAN/1/hist
	rm -f $MAN/1/histw
	rm -f $MAN/1/savehist
