#include <u.h>
#include <libc.h>
#include <keyboard.h>

static int mod;

enum {
	Mmod4 = 1<<0,
	Mctl = 1<<1,
	Mshift = 1<<2,
	Malt = 1<<3,
};

static char* prompt;
static char* home;

static ulong tsize;


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
processhist(int hop)
{
	/* history operations (hop) legend: */
	/* >0 - history foreward by x */
	/* <0 - history backward by x */

	int tfd, hfd, fr, lbc;
		
	char linebf[1024];
	char histpath[256];
	
	long hc;

	lbc = sizeof linebf - 1;


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
		for(hc = tsize; hc >= 0; hc--){
			pread(hfd, &linebf[lbc], 1, hc-1);
			if(linebf[lbc] == '\n' || hc == 0){
				if(lbc == sizeof linebf - 1)
					continue;

				toprompt(&linebf[lbc+1], sizeof linebf - lbc - 1);
				tsize = hc - 1;
				break;
			}	
			lbc--;
		}
	}

	/* history backward */
	int lc = 0;
	
	if(hop < 0){
		for(hc = tsize; ; hc++){
			fr = pread(hfd, &linebf[lc], 1, hc);
			if(fr == 0){
				/* no more history, set prompt to empty */
				toprompt("",0);
				break;
			}

			if(linebf[lc] == '\n'){
				if(lc == 0)
					continue;

				toprompt(&linebf[0], lc);
				tsize = hc + 1;
				break;
			}
			lc++;
		}
	}

	close(hfd);

	free(home);


/* set aside for parsing prompt - ignore */
//prompt = getenv("prompt");
//free(prompt);
//int pco = strlen(prompt) - 3; /* what are the extra characters at the end */
//int scr = -1;
//scr = strncmp(&linebf[lbc+1], prompt, pco);
//if(scr == 0){
//	write(2, &linebf[lbc+pco+1+1], sizeof linebf - 1 - 1 - pco - lbc);
//	fprint(2,"\n");
//}
}


static void
process(char *s)
{
	char b[128], *p;
	int n, o, skip;
	Rune r;

	o = 0;
	b[o++] = *s;

	int hop = 0;
	
	/* mod key */
	if(*s == 'k' || *s == 'K'){
		mod = 0;
		if(utfrune(s+1, Kmod4) != nil)
			mod |= Mmod4;
		if(utfrune(s+1, Kctl) != nil)
			mod |= Mctl;
		if(utfrune(s+1, Kshift) != nil)
			mod |= Mshift;
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
		if(*s == 'c' && mod == Mctl){
			if(r == Kup){
				hop = 1;
				skip = 1;
			}
			if(r == Kdown){
				hop = -1;
				skip = 1;
			}

			if(hop !=0)
				processhist(hop);
		}

		/* reset history tracking */
		if(r == 10){
			/* enter key */
			tsize = 0;
		}
		if(r == 127){
			/* delete key */
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
	fprint(2, "usage: %s [-i] [-g | -G]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int isinteractive = 0;
	int uselocal = 1;
	int useglobal = 0;


	ARGBEGIN{
	case 'i':
		isinteractive = 1;
		break;
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



	/* stand alone segment - print out history */

	if(!isinteractive){
		prompt = getenv("prompt");

		int tfd, hfd, fr;
		
		char linebf[1024];


		/* print global history from $home/lib/rchistory */
		if(useglobal){
			home = getenv("home");

			char histpath[128];
			memset(histpath, 0, sizeof histpath);
			strcat(histpath, home);
			strcat(histpath, "/lib/rchistory");

			print("# global history\n");

			hfd = open(histpath, OREAD);
			if(hfd > 0){
				for(;;){
					fr = read(hfd, linebf, sizeof linebf);
					write(1, linebf, fr);
					if(fr < sizeof linebf)
						break;
				}
				close(hfd);
			}

			free(home);
		}


		/* parse and print local history from /dev/text */
		if(uselocal){

			long tc;
			int lc = 0;
			int pc = 0;

			int pcx = 0;
		
			int pco = strlen(prompt) - 3; /* what are the extra characters at the end */

			/* get current /dev/text size before we exand it */
			tsize = textsize("/dev/text");

			print("# local history\n");

			tfd = open("/dev/text", OREAD);
			for(tc = 0;tc < tsize; tc++){
				/* TODO improve - kills itself with syscals and context switches */
				read(tfd, &linebf[lc], 1);
				if(pcx){
					if(linebf[lc] == '\n'){
						/* got EOL - cmd line end */
						write(1, &linebf[pco+1], lc-pco);
						pcx = 0;
						pc = 0;
						lc = 0;
					} else {
						lc++;
					}
				} else {
					if(linebf[lc] == prompt[pc]){
						lc++;
						pc++;
						if(pc > pco){
							/* got prompt - cmd line beginn */
							pcx = 1;
						}
					} else {
						lc = 0;
						pc = 0;
						pcx = 0;
					}
				}			
			}
			close(tfd);
		}

		free(prompt);

		exits(nil);
	}


	/* interactive segment - manipulate console promt */

	char b[128];
	int i, j, n;

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
