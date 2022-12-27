#include <u.h>
#include <libc.h>
#include <keyboard.h>

/* processing line buffer size */
#define LBFS 1024

static int mod;

static char* prompt;
static char* home;

static ulong tsize;

static int hop;

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

	int hfd, fr;
		
	char linebf[LBFS];
	char histpath[256];
	
	long hc;


	/* so far only for global history */

	home = getenv("home");

	memset(histpath, 0, sizeof histpath);
	strcat(histpath, home);
	strcat(histpath, "/lib/rchistory");

	/* no history file has been opened yet or we are at the end */
	if(tsize == 0){
		tsize = textsize(histpath);
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
				tsize = hc - 1;

				if(lbc == sizeof linebf - 1)
					continue;

				if(hop > 1){
					hop--;
					lbc = sizeof linebf - 1;
					continue;
				}

				toprompt(&linebf[lbc+1], sizeof linebf - lbc - 1);
				break;
			}	
			lbc--;
		}
	}

	/* history backward */	
	if(hop < 0){
		int lc = 0;

		for(hc = tsize; ; hc++){
			fr = pread(hfd, &linebf[lc], 1, hc);
			if(fr == 0){
				/* no more history, set prompt to empty */
				toprompt("",0);
				hop = 0;
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
			lc++;
		}
	}
	close(hfd);

	free(home);
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

		if(exec !=0)
			processhist();

		/* reset history tracking */
		if(r == 10){
			/* enter key */
			hop = 0;
			tsize = 0;
		}
		if(r == 127){
			/* delete key */
			hop = 0;
			tsize = 0;
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
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	ARGBEGIN{
	default:
		usage();
	}ARGEND

	/* interactive segment - manipulate console promt */

	char b[128];
	int i, j, n;

	hop = 0;	/* init history operations tracking */

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

	exits(nil);
}
