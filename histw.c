#include <u.h>
#include <libc.h>
#include <keyboard.h>


/* processing line buffer size */
#define LBFS 1024


static int mod;

static int uselocal;
static int useglobal;

static char* home;
static char* prompt;

static int tstate;
static ulong tsize;

static int hop;
static int hsrc;

static int pwid;

static int wsysfd;


void
resethstate(void){
	hop = 0;

	tstate = 0;

	hsrc = 1;
	if(useglobal > uselocal){
		hsrc = 2;
	}
}


int
readwctl(char *buf, int nbuf, int id)
{
	int fd, n;
	char s[64];

	snprint(s, sizeof(s), "/dev/wsys/%d/wctl", id);

	if((fd = open(s, OREAD)) < 0){
		buf[0] = 0;
		return -1;
	}

	n = read(fd, buf, nbuf-1);

	close(fd);

	if(n < 0){
		buf[0] = 0;
		return -1;
	}

	buf[n] = 0;
	return n;
}

ulong
textsize(char* fname)
{
	/* read text file insted of using file stats */
	/* some files like /dev/text have size 0 since they are generated */

	int fd, r;
	ulong sum = 0;
		
	char buf[1024];

	/* get current /dev/text size */
	fd = open(fname, OREAD);
	for(;;){
		r = read(fd, buf, sizeof buf);
		sum = sum + r;
		if(r < sizeof buf)
			break;
	}
	close(fd);
	
	return sum;
}


void
toprompt(char* text, int len)
{
	int kfd;
	char ctlbf[8];

	ctlbf[0] = 2;	/* STX (ctrl+b) - move cursor to prompt */
	ctlbf[1] = 5;	/* ENQ (ctrl+e) - move cursor to end of text in promt */
	ctlbf[2] = 21;	/* NAK (ctrl+u) - delete everything behind cursor */

	kfd = open("/dev/kbdin", OWRITE);

	write(kfd, ctlbf, 3);
	write(kfd, text, len);

	close(kfd);
}


void
processhist(void)
{
	/* history operations (hop) legend: */
	/* >0 - history foreward by x */
	/* <0 - history backward by x */

	/* history sorce (hsrc) legend: */
	/* 1 - local history */
	/* 2 - global hitory */
fprint(2, "HSRC %d\n", hsrc);
	int hfd, fr;
		
	char linebf[LBFS];
	char histpath[256];
	
	long hc;

	/* process local history */
	if(uselocal){
fprint(2, "WIN ID\n");

		/* find current window id */
		int dsn, dn, wid;
		Dir *ds, *d;
		char wctlpath[64], s[256], *t[8];

		wid = 0;

		seek(wsysfd, 0, 0);
		dsn = dirreadall(wsysfd, &ds);
fprint(2, "DRALL dsn  %d\n", dsn);
//!!!
		for(dn = 0, d = ds; dn < dsn; dn++, d++){
//fprint(2, "DRALL LOOP name  %d\n", atoi(d->name));


			if(readwctl(s, sizeof(s), atoi(d->name)) <= 0){
fprint(2, "DRALL WCTL READ fail\n");
				continue;
			}
//fprint(2, "DRALL WCTL READ s %s\n", s);


			if(tokenize(s, t, nelem(t)) != 6){
				continue;
			}
			
			if(strcmp(t[4], "current") == 0){
//fprint(2, "DRALL WCTL CURRENT id %d\n", atoi(d->name));
				wid = atoi(d->name);
				break;
			}
		}

		if(wid != pwid){
fprint(2, "DRALL WCTL RESET wid != pwid : %d %d\n", wid, pwid);
			resethstate();
			toprompt("", 0);
			pwid = wid;
		}

	}

	/* process local history */
	if(hsrc == 1 && hop != 0){
fprint(2, "LOCAL HIST\n");

		/* switch to local history if configured */
		if(useglobal){
			toprompt("# END OF LOCAL HISTORY", 22);
			hsrc = 2;
			hop = 0;
fprint(2, "LOCAL HIST - switch global\n");
		}
		
	}


	/* process global history */
	if(hsrc == 2 && hop != 0){
fprint(2, "GLOBAL HIST\n");
		home = getenv("home");

		memset(histpath, 0, sizeof histpath);
		strcat(histpath, home);
		strcat(histpath, "/lib/rchistory");

		/* no history file has been opened yet or we are at the end */
		if(tstate == 0){
			tsize = textsize(histpath);
			tstate = 1;
		}

		memset(linebf, 0, sizeof linebf);

		hfd = open(histpath, OREAD);

		if(hfd < 0)
			exits(nil);

		/* history foreward */
		if(hop > 0){
			int lbc = sizeof linebf - 1;

			for(hc = tsize; hc >= 0; hc--){
				pread(hfd, &linebf[lbc], 1, hc-1);
			
				if(linebf[lbc] == '\n' || hc == 0){
					if(hc == 0){
						tsize = 0;
					}
					else {
						tsize = hc - 1;
					}

					if(lbc == sizeof linebf - 1)
						continue;

					if(hop > 1){
						hop--;
						lbc = sizeof linebf - 1;
						continue;
					}

					toprompt(&linebf[lbc+1], sizeof linebf - lbc - 1);
fprint(2, "GLOBAL HIST - top (to prompt: hc - %d)\n", hc);
					break;
				}	
				lbc--;
			}
fprint(2, "GLOBAL HIST - top (tsize - %d : hc - %d)\n", tsize, hc);
			/* no more history, set prompt alert */
			if(tsize == 0 && hc < 0){
				toprompt("# END OF GLOBAL HISTORY", 23);
				hop = 0;
			}
		}
fprint(2, "GLOBAL HIST - mid (tsize - %d : hc - %d)\n", tsize, hc);
		/* history backward */	
		if(hop < 0){
			int lc = 0;

			for(hc = tsize; ; hc++){
				fr = pread(hfd, &linebf[lc], 1, hc);
				if(fr == 0){
fprint(2, "GLOBAL HIST - bottom\n");
					/* no more history, set prompt to empty */
					toprompt("", 0);
					hop = 0;

					/* switch to local history if configured */
					if(uselocal){
						toprompt("# START OF LOCAL HISTORY", 24);
						hsrc = 1;
fprint(2, "GLOBAL HIST - switch local\n");
					}
					break;
				}

				if(linebf[lc] == '\n'){
					tsize = hc + 1;

					if(lc == 0){
						continue;
					}

					if(hop < -1){
						hop++;
						lc = 0;
						continue;
					}

					toprompt(&linebf[0], lc);
					break;
				}
fprint(2, "GLOBAL HIST - bottom (tsize - %d : lc - %d : hc - %d)\n", tsize, lc, hc);
				lc++;
			}
		}
		close(hfd);

		free(home);
	}
}


static void
process(char *s)
{
	char b[128], *p;
	int n, o, skip, exec;
	Rune r;

	o = 0;
	b[o++] = *s;

	/* mod key */
	if(*s == 'k' || *s == 'K'){
		mod = 0;
		if(utfrune(s+1, Kctl) != nil)
			mod = Kctl;
	}	

	for(p = s+1; *p != 0; p += n){
		if((n = chartorune(&r, p)) == 1 && r == Runeerror){
			/* bail out */
			n = strlen(p);
			memmove(b+o, p, n);
			o += n;
			p += n;
			break;
		}
		
		/* react to key combinations */
		skip = 0;
		exec = 0;
		if(*s == 'c' && mod == Kctl){
			if(r == Kup){
				if(hop < 0){
					hop = 2;
				} else {
					hop = 1;
				}
				skip = 1;
				exec = 1;
			}
			if(r == Kdown){
				if(hop > 0){
					hop = -2;
				} else {
					hop = -1;
				}
				skip = 1;
				exec = 1;
			}
		}

		if(exec != 0)
			processhist();

		/* reset history tracking if command entered or canceled */
		if(r == 10){
			/* enter key */
			resethstate();
		}
		if(r == 127){
			/* delete key */
			resethstate();
		}

		if(!skip){
			memmove(b+o, p, n);
			o += n;
		}
	}

	/* all runes filtered out - ignore completely */
	if(o == 1 && p-s > 1)
		return;

	b[o++] = 0;
	if(write(1, b, o) != o)
		exits(nil);
}


static void
usage(void)
{
	fprint(2, "usage: %s [-g | -G]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	uselocal = 1;
	useglobal = 0;

	ARGBEGIN{
	case 'g':
		useglobal = 1;
		break;
	case 'G':
		uselocal = 0;
		useglobal = 1;
		break;
	default:
		usage();
	}ARGEND


	/* interactive segment - manipulate console promt */

	char b[128];
	int i, j, n;

	/* init history operations tracking */
	hop = 0;
	pwid = 0;
	hsrc = 1;
	if(useglobal > uselocal){
		hsrc = 2;
	}

	if(uselocal){
		if((wsysfd = open("/dev/wsys", OREAD)) < 0){
			sysfatal("%r");
		}
	}


	for(i = 0;;){
		if((n = read(0, b+i, sizeof(b)-i)) <= 0)
			break;
		n += i;

		for(j = 0; j < n; j++){
			if(b[j] == 0){
				process(b+i);
				i = j+1;
			}
		}

		memmove(b, b+i, j-i);
		i -= j;
	}

	close(wsysfd);

	exits(nil);
}
