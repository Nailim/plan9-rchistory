#include <u.h>
#include <libc.h>
#include <keyboard.h>


/* processing line buffer size */
#define LBFS 1024
#define HPS 512

static int mod;

static int uselocal;
static int useglobal;

static char *home;
static char *prompt;

static int tstate;
static ulong tpos;
static ulong tsize;

static int hop;
static int hsrc;

static int pwid;

static int wsysfd;


void
resethstate(void){
	hop = 0;

	tstate = 0;

	pwid = 0;	// TODO better logic to rest state between windows

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
textsize(char *fname)
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
		if(r < sizeof buf){
			break;
		}
	}
	close(fd);
	
	return sum;
}


void
toprompt(char *text, int len)
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

	int hfd, fr;
		
	char linebf[LBFS];
	char histpath[HPS];
	
	long hc;

	int wid;


	/* find current window id */
	if(uselocal){
		int dsn, dn;
		Dir *ds, *d;
		char s[256], *t[8];

		wid = 0;

		seek(wsysfd, 0, 0);
		dsn = dirreadall(wsysfd, &ds);

		for(dn = 0, d = ds; dn < dsn; dn++, d++){
			if(readwctl(s, sizeof(s), atoi(d->name)) <= 0){
				continue;
			}

			if(tokenize(s, t, nelem(t)) != 6){
				continue;
			}
			
			if(strcmp(t[4], "current") == 0){
				wid = atoi(d->name);
				break;
			}
		}

		if(wid != pwid){
			resethstate();
			toprompt("", 0);
			pwid = wid;
		}

	}

	/* process local history from /dev/wsys/%id/text */
	if(hsrc == 1 && hop != 0){
		int tfd, rc;

		tokenize(getenv("prompt"), &prompt, 1);

		int prc = strlen(prompt);

		long tr = 0;	/* text read */
		long tp = 0;	/* text proccesed */

		char *ssp;		/* pointer to prompt */
		char *sse;		/* pointer to EOL */
		char *ssee;		/* pointer to sencond last EOL */
		char *tmpfr;

		int bfl = LBFS - 1;	/* buffer left to read in */
		int bfld = 0;		/* buffer diff between moved and remaining space */

		memset(histpath, 0, sizeof histpath);

		/* compose full path to local user history */
		snprint(histpath, sizeof histpath, "/dev/wsys/%d/text", wid);

		/* no history file has been opened yet or we are changing state */
		if(tstate == 0){
			if(hop > 0){
				tpos = textsize(histpath);	/* starting history up */
			} else {
				tpos = 0;	/* starting history down */
			}
			
			tsize = textsize(histpath);
			tstate = 1;
		}

		tfd = open(histpath, OREAD);


		/* history up */
		if(hop > 0){
			/* search in reverse MUST NOT have null chars at the beginning */
			memset(linebf, 64, LBFS);

			/* first read exception - less text than buffer*/
			if(bfl > (tsize - (tsize-tpos))){
				bfl = (tsize - (tsize-tpos));
				bfld = ((LBFS-1) - bfl);
			}

			tp = tpos;
			if(tp != tsize){
				/* not processing history from the end */
				tr = tsize - tp;
			}


			/* no more history, set prompt alert */
			if(tp == 0){
				toprompt("# END OF LOCAL HISTORY", 22);

				/* switch to global history if configured */
				if(useglobal){
					toprompt("# START OF GLOBAL HISTORY", 25);
					tstate = 0;
					hsrc = 2;
					hop = 0;
				}
			}


			while(tp > 0){
				rc = pread(tfd, linebf + bfld, bfl, (tsize-tr) - bfl);
				bfl -= rc;
				bfld = 0;
				tr += rc;


				/* find prompt */
				ssp = 0;
				tmpfr = linebf;
				do {
					tmpfr = strstr(tmpfr, prompt);
					if(tmpfr != 0){
						ssp = tmpfr;
						tmpfr = tmpfr + 1;
					}
				} while (tmpfr != 0 && tmpfr < (linebf + LBFS - 1));

				/* find newline char */
				sse = 0;
				ssee = 0;
				if(ssp != 0){
					tmpfr = ssp;
				} else {
					tmpfr = linebf;
				}
				do {
					tmpfr = strchr(tmpfr, '\n');
					if(tmpfr != 0){
						ssee = sse;
						sse = tmpfr;
						if(ssp != 0){
							break;	/* if prompt is found only next one is needed */
						}
						tmpfr = tmpfr + 1;
					}
				} while (tmpfr != 0 && tmpfr < (linebf + LBFS - 1));


				/* history hit */
				if((ssp != 0) && (sse != 0)){
					/* print out command to propt */
					toprompt(ssp + prc + 1, (sse-ssp) - prc - 1);
					if((ssp-linebf) == 0){
						/* nothing to move if prompt is at the beginning of buffer */
						memset(linebf, 64, (LBFS-1));
						bfl += (LBFS-1);
						tp -= (LBFS-1);
					} else {
						/* move the rest carefuly */
						memmove(linebf + (LBFS-1-(ssp-linebf)), linebf, (ssp-linebf));
						memset(linebf, 64, (LBFS-1-(ssp-linebf)));
						bfl += (LBFS-1-(ssp-linebf));
						tp -= (LBFS-1-(ssp-linebf));
					}
					tpos = tp;
					break;
				}

				/* buffer move - prompt */
				if((ssp != 0) && (sse == 0)){
					if((ssp-linebf) == 0){
						/* nothing to move if prompt is at the beginning of buffer */
						memset(linebf, 64, (LBFS-1));
						bfl += (LBFS-1);
						tp -= (LBFS-1);
					} else {
						/* move the rest carefuly */
						memmove(linebf + (LBFS-1-(ssp-linebf)), linebf, (ssp-linebf));
						memset(linebf, 64, (LBFS-1-(ssp-linebf)));
						bfl += (LBFS-1-(ssp-linebf));
						tp -= (LBFS-1-(ssp-linebf));
					}
				}

				/* buffer move - newline */
				if((ssp == 0) && (sse != 0)){
					if((sse-linebf) == LBFS-2){
						/* if newline char is the last char in buffer ... */
						if (ssee != 0){
							/* move till second last newline char */
							memmove(linebf + (LBFS-2-(ssee-linebf)), linebf, (ssee-linebf) + 1);
							memset(linebf, 64, (LBFS-2-(ssee-linebf)));
							bfl += (LBFS-2-(ssee-linebf));
							tp -= (LBFS-2-(ssee-linebf));
						} else {
							/* nothing to do but clear whole buffer */
							memset(linebf, 64, (LBFS-1));
							bfl += (LBFS-1);
							tp -= (LBFS-1);
						}
					} else {
						/* move till last newline char */
						memmove(linebf + (LBFS-2-(sse-linebf)), linebf, (sse-linebf) + 1);
						memset(linebf, 64, (LBFS-2-(sse-linebf)));
						bfl += (LBFS-2-(sse-linebf));
						tp -= (LBFS-2-(sse-linebf));
					}
				}

				/* no hit in buffer exception */
				if((ssp == 0) && (sse == 0)){
					memset(linebf, 64, (LBFS-1));
					bfl = (LBFS-1);
				}

				/* last read (end of data) exception */
				if((tr+bfl) > tsize){
					bfld = bfl;
					bfl = bfl - ((tr+bfl) - tsize);
					bfld = bfld - bfl;
				}

				/* fail-save's - just in case if don't know what's going on */
				if((rc == 0) && (bfl != 0)){
					/* tainted history - less to read than detected at start */
					toprompt("", 0);
					tp = 0;
					hop = 0;
				}
				if((rc == 0) && (bfl == 0) && (ssp == sse)){
					/* tainted buffer - nothing to read or parse */
					toprompt("", 0);
					tp = 0;
					hop = 0;
				}
			}
		}


		/* history down */
		if(hop < 0){
			/* search in reverse MUST have null chars at the end */
			memset(linebf, 0, LBFS);

			/* first read exception - less text than buffer */
			if(bfl > (tsize - tpos)){
				bfl = (tsize - tpos);
				bfld = ((LBFS-1) - bfl);
			}

			tp = tpos;
			if(tp != 0){
				/* not processing history from the beginning */
				tr = tp;
			}

			while(tp < tsize){
				rc = pread(tfd, linebf + ((LBFS-1)-bfl) - bfld,  bfl, tr);
				bfl -= rc;
				bfld = 0;
				tr += rc;

				ssp = strstr(linebf, prompt);
				if(ssp != 0){
					if(ssp-linebf > 0){
						/* align prompt with start of the buffer */
						memmove(linebf, ssp, LBFS - (ssp-linebf));
						memset(linebf + LBFS - (ssp-linebf), 0, (ssp-linebf));
						bfl += ((ssp-linebf));
						tp += ((ssp-linebf));
					}

					/* we trust the buffer is long enough for the whole command */

					sse = strchr(linebf, '\n');
					if(sse != 0){
						/* print out command to propt */
						if(linebf[prc] != '\n'){
							toprompt(linebf + prc + 1, (sse-linebf) - prc - 1);
							tpos = tp + (sse-linebf) + 1;
							break;
						}
						memmove(linebf, sse+1, LBFS - (sse-linebf) + 1);
						memset(linebf + LBFS - (sse-linebf) + 1, 0, (sse-linebf));
						bfl += ((sse-linebf) + 1);
						tp += ((sse-linebf) + 1);
					}
				} else {
					/* if there is no prompt in buffer */
					/* move to next new line character instead */
					sse = strchr(linebf, '\n');
					if((sse != 0) && ((sse-linebf) < LBFS-2)){
						memmove(linebf, sse+1, LBFS - (sse-linebf) + 1);
						memset(linebf + LBFS - (sse-linebf) + 1, 0, (sse-linebf));
						bfl += ((sse-linebf) + 1);
						tp += ((sse-linebf) + 1);
					}
				}

				/* no hit in buffer exception */
				if(bfl == 0){
					bfl += LBFS - 1;
					tp += strlen(linebf);
					memset(linebf, 0, (LBFS-1));

					/* no more history, set prompt to empty */
					toprompt("", 0);
					hop = 0;
				}

				/* last read (end of data) exception */
				if((tr+bfl) > tsize){
					bfld = bfl;
					bfl = bfl - ((tr+bfl) - tsize);
					bfld = bfld - bfl;
				}

				/* tainted history - less to read than detected at start */
				if((rc == 0) && (bfl != 0)){
					/* don't know what's going on, set prompt to empty */
					toprompt("", 0);
					tp = tsize;
					hop = 0;
				}
			}
		}


		close(tfd);

		free(prompt);
	}


	/* process global history from $home/lib/rchistory */
	if(hsrc == 2 && hop != 0){

		home = getenv("home");

		memset(histpath, 0, sizeof histpath);
		strcat(histpath, home);
		strcat(histpath, "/lib/rchistory");

		/* no history file has been opened yet or we are at the end */
		if(tstate == 0){
			tpos = textsize(histpath);
			tstate = 1;
		}

		memset(linebf, 0, sizeof linebf);

		hfd = open(histpath, OREAD);

		if(hfd < 0){
			exits(nil);
		}


		/* history up */
		if(hop > 0){
			int lbc = sizeof linebf - 1;

			for(hc = tpos; hc >= 0; hc--){
				pread(hfd, &linebf[lbc], 1, hc-1);
			
				if(linebf[lbc] == '\n' || hc == 0){
					if(hc == 0){
						tpos = 0;
					}
					else {
						tpos = hc - 1;
					}

					if(lbc == sizeof linebf - 1){
						continue;
					}

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

			/* no more history, set prompt alert */
			if(tpos == 0 && hc < 0){
				toprompt("# END OF GLOBAL HISTORY", 23);
				hop = 0;
			}
		}


		/* history down */	
		if(hop < 0){
			int lc = 0;

			for(hc = tpos; ; hc++){
				fr = pread(hfd, &linebf[lc], 1, hc);
				if(fr == 0){
					/* no more history, set prompt to empty */
					toprompt("", 0);
					hop = 0;

					/* switch to local history if configured */
					if(uselocal){
						toprompt("# START OF LOCAL HISTORY", 24);
						tstate = 0;
						hsrc = 1;
					}
					break;
				}

				if(linebf[lc] == '\n'){
					tpos = hc + 1;

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
		if(utfrune(s+1, Kctl) != nil){
			mod = Kctl;
		}
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

		if(exec != 0){
			processhist();
		}

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
	if(o == 1 && p-s > 1){
		return;
	}

	b[o++] = 0;
	if(write(1, b, o) != o){
		exits(nil);
	}
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
	resethstate();

	if(uselocal){
		if((wsysfd = open("/dev/wsys", OREAD)) < 0){
			sysfatal("%r");
		}
	}


	for(i = 0;;){
		if((n = read(0, b+i, sizeof(b)-i)) <= 0){
			break;
		}
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
