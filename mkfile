</$objtype/mkfile

RC=$home/bin/rc
BIN=$home/bin/$objtype

hist: hist.$O
	$LD $LDFLAGS -o hist hist.$O

hist.$O: hist.c
	$CC $CFLAGS hist.c

clean:V:
	rm -f *.$O hist

install:V:
	mv hist $BIN/
	cp savehist $RC/

uninstall:V:
	rm -f $BIN/hist
	rm -f $RC/savehist
