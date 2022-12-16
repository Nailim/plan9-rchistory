</$objtype/mkfile

BIN=$home/bin/$objtype

hist: hist.$O
	$LD $LDFLAGS -o hist hist.$O

hist.$O: hist.c
	$CC $CFLAGS hist.c

clean:V:
	rm *.$O hist

install:V:
	mv hist $BIN/
