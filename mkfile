</$objtype/mkfile

RC=$home/bin/rc
BIN=$home/bin/$objtype

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

uninstall:V:
	rm -f $BIN/hist
	rm -f $BIN/histw
	rm -f $RC/savehist
