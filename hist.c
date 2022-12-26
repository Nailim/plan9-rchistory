#include <u.h>
#include <libc.h>
#include <keyboard.h>

static int mod;

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

	int hfd, fr, lbc;
		
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
		if(*s == 'c' && mod == Kctl){
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
		#define LBFS 424
		char linebf[LBFS];


		/* print global history from $home/lib/rchistory */
		if(useglobal){
			int hfd, rc;

			home = getenv("home");
			/* compose full path to global user history */
			char histpath[128];
			memset(histpath, 0, sizeof histpath);
			strcat(histpath, home);
			strcat(histpath, "/lib/rchistory");

			print("# global history\n");

			hfd = open(histpath, OREAD);
			if(hfd > 0){
				for(;;){
					rc = read(hfd, linebf, sizeof linebf);
					write(1, linebf, rc);
					if(rc < sizeof linebf)
						break;
				}
				close(hfd);
			}

			free(home);
		}


		/* parse and print local history from /dev/text */
		if(uselocal){
			int tfd, rc;

			prompt = getenv("prompt");
			/* zero out excess characters at the end of prompt variable */
			#define POFF 2
			memset(prompt + strlen(prompt) - POFF, 0, POFF);

			int prc = strlen(prompt);

			long tr = 0;	// text read
			long tp = 0;	// text proccesed

			char* ssp;		// pointer to prompt
			char* sse;		// pointer to EOL

			int bfl = LBFS - 1;	// buffer left to read in
			int bfld = 0;		// buffer diff between moved and remaining space

			memset(linebf, 0, LBFS);

			/* get current /dev/text size before we expand it */
			tsize = textsize("/dev/text");

			print("# local history\n");

			tfd = open("/dev/text", OREAD);

			/* first read exception */
			if(bfl > tsize){
				bfl = tsize;
				bfld = ((LBFS-1) - bfl);
			}

			while(tp < tsize){
				rc = read(tfd, linebf + ((LBFS-1)-bfl) - bfld,  bfl);
				bfl -= rc;
				bfld = 0;
				tr += rc;

				ssp = strstr(linebf, prompt);
				if(ssp != 0){
					if(ssp-linebf > 0){
						memmove(linebf, ssp, LBFS - (ssp-linebf));
						memset(linebf + LBFS - (ssp-linebf), 0, (ssp-linebf));
						bfl += ((ssp-linebf));
						tp += ((ssp-linebf));
					}

					sse = strchr(linebf, '\n');
					if(sse != 0){
						/* print out command without propt + ignore empty lines */
						if(linebf[prc] != '\n')
							write(1, linebf + prc, (sse-linebf) - prc + 1);
						memmove(linebf, sse+1, LBFS - (sse-linebf) + 1);
						memset(linebf + LBFS - (sse-linebf) + 1, 0, (sse-linebf));
						bfl += ((sse-linebf) + 1);
						tp += ((sse-linebf) + 1);
					}
				}

				/* no hit in buffer exception */
				if(bfl == 0){
					bfl = LBFS - 1;
					tp += strlen(linebf);
				}

				/* last read (end of data) exception */
				if((tr+bfl) > tsize){
					bfld = bfl;
					bfl = bfl - ((tr+bfl) - tsize);
					bfld = bfld - bfl;	
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
