</$objtype/mkfile

RC=/rc/bin
BIN=/$objtype/bin

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
	
	rm -f $home/bin/$objtype/hist
	rm -f $home/bin/$objtype/histw
	rm -f $home/bin/rc/savehist

uninstall:V:
	rm -f $BIN/hist
	rm -f $BIN/histw
	rm -f $RC/savehist
	
	rm -f $home/bin/$objtype/hist
	rm -f $home/bin/$objtype/histw
	rm -f $home/bin/rc/savehist
