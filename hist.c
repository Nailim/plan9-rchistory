#include <u.h>
#include <libc.h>
#include <keyboard.h>

static int mod;
static int count;

enum {
	Mmod4 = 1<<0,
	Mctl = 1<<1,
	Mshift = 1<<2,
	Malt = 1<<3,
};

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
		
		/* mz code ... */
		skip = 0;
		if(*s == 'c'){
			if(r == Kup && mod == Mctl){
				fprint(2, "\nCTRL + Key UP\n");
				skip = 1;
			}
			if(r == Kdown && mod == Mctl){
				fprint(2, "\nCTRL + Key DOWN\n");
				skip = 1;
			}
			if(r == 21 && mod == Mctl){
				fprint(2, "\nCTRL + U\n");
				skip = 1;
			}
			fprint(2, "\nChar: %d\n", r);
		}
		if(r == 'p'){
			fprint(2, "\nCTRL + U AS char p\n");
			int fdc;
			char buf[64];

			fdc = open("/dev/kbdin", OWRITE);
			//sprint(buf, ":%d", 21);
			buf[0] = 2;
			buf[1] = 5;
			buf[2] = 21;
			write(fdc, buf, 3);
			sprint(buf, "%s", "ll");
			write(fdc, buf, 2);
			close(fdc);
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
	fprint(2, "usage: %s [-i]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int isinteractive = 0;

	ARGBEGIN{
	case 'i':
		isinteractive = 1;
		break;
	default:
		usage();
	}ARGEND

	char* prompt = getenv("prompt");
	char* home = getenv("home");

	char* relhpath = "/lib/rchistory";
	char* histpath = malloc(strlen(home) + strlen(relhpath) + 1);
	memset(histpath, 0, strlen(home) + strlen(relhpath) + 1);
	strcat(histpath, home);
	strcat(histpath, relhpath);



	/* stand alone segment - print out history */

	if(!isinteractive){
		int tfd, hfd, fr;
		ulong tsize = 0;
		
		char linebf[1024];

		/* get current /dev/text size */
		tfd = open("/dev/text", OREAD);
		for(;;){
			fr = read(tfd, linebf, sizeof linebf);
			tsize = tsize + fr;
			if(fr < sizeof linebf)
				break;
		}
		close(tfd);


		/* print global history from $home/lib/rchistory */
		
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


		/* parse and print local history from /dev/text */

		int tc;
		int lc = 0;
		int pc = 0;

		int pcx = 0;
		
		int pco = strlen(prompt) - 3; /* what are the extra characters at the end */

		print("# local history\n");
		

		tfd = open("/dev/text", OREAD);
		for(tc = 0;tc < tsize; tc++){
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

		exits(nil);
	}



	/* interactive segment - manipulate console promt */

	print("Nothing yet ...");
	exits(nil);

	char b[128];
	int i, j, n;

	count = 0;

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
