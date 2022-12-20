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


static void
process(char *s)
{
	char b[128], *p;
	int n, o, skip;
	Rune r;

	o = 0;
	b[o++] = *s;
	
	/* mod key */
	if(*s == 'k' || *s == 'K'){
		fprint(2, "\nmod key: %d\n,", *s);
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
		
		/* my code ... */
		skip = 0;
		if(*s == 'c' && mod == Mctl){
			if(r == Kup){
				fprint(2, "\nCTRL + Key UP\n");
// from here
//
	int tfd, hfd, fr;
		
	char linebf[1024];
	char cmdbf[1024];
	


	fprint(2, "\nNothing yet ...\n\n");


	long hc;

	int lbc = sizeof linebf - 1;

// set aside for parsing prompt
//
//prompt = getenv("prompt");
//free(prompt);
//int pco = strlen(prompt) - 3; /* what are the extra characters at the end */
//
//int scr = -1;
//scr = strncmp(&linebf[lbc+1], prompt, pco);
//if(scr == 0){
//	
//	write(2, &linebf[lbc+pco+1+1], sizeof linebf - 1 - 1 - pco - lbc);
//	fprint(2,"\n");
//}

			home = getenv("home");

			char histpath[128];
			memset(histpath, 0, sizeof histpath);
			strcat(histpath, home);
			strcat(histpath, "/lib/rchistory");

	fprint(2, "%d\n", tsize);
	if(tsize == 0){
		fprint(2, "NEW tsize!!!");
		tsize = textsize(histpath);
	}
	fprint(2, "%d\n", tsize);

			fprint(2, "# global history %s\n", histpath);

			hfd = open(histpath, OREAD);
	/* -1 to remove last LF (first one read) */
	for(hc = tsize; hc >= 0; hc--){
		pread(hfd, &linebf[lbc], 1, hc-1);
		//fprint(2,"%c\n", linebf[lbc]);
		if(linebf[lbc] == '\n' || hc == 0){
			fprint(2,"\nLine: %d %d %d - ", hc, lbc, sizeof linebf);
			write(2, &linebf[lbc+1], sizeof linebf - 1 - lbc);

			memset(cmdbf, 0, sizeof cmdbf);
			
			char buf[8];
			buf[0] = 2;
			buf[1] = 5;
			buf[2] = 21;
			int kfd;
			kfd = open("/dev/kbdin", OWRITE);
			write(kfd, buf, 3);
			strncat(cmdbf, &linebf[lbc+1], sizeof linebf - 1 - lbc);
			write(kfd, &linebf[lbc+1], sizeof linebf - 1 - lbc);
			close(kfd);

			tsize = hc - 1;

			break;
			
			//lbc = sizeof linebf - 1;
		} else {
			lbc--;
		}
		
	}
	close(hfd);

	free(home);

//
// to here
				skip = 1;
			}
			if(r == Kdown){
				fprint(2, "\nCTRL + Key DOWN\n");
				skip = 1;
			}
			fprint(2, "\nChar: %d\n", r);

		
		}
//		if(r == 'p'){
//			fprint(2, "\nCTRL + U AS char p\n");
//			int fdc;
//			char buf[64];
//
//			fdc = open("/dev/kbdin", OWRITE);
//			//sprint(buf, ":%d", 21);
//			buf[0] = 2;
//			buf[1] = 5;
//			buf[2] = 21;
//			write(fdc, buf, 3);
//			sprint(buf, "%s", "ll");
//			write(fdc, buf, 2);
//			close(fdc);
//		}
		if(r == '\n'){
			fprint(2, "\nENTER!!!\n");
			tsize = 0;
		}



		//skip = 0;
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

		/* get current /dev/text size */
		tsize = textsize("/dev/text");


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
			int tc;
			int lc = 0;
			int pc = 0;

			int pcx = 0;
		
			int pco = strlen(prompt) - 3; /* what are the extra characters at the end */

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

	print("\nNothing yet ...\n\n");
	exits(nil);

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
