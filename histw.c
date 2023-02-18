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

static int fstate;
static char *pfilter;

static int hop;
static int hsrc;

static int wwid;

static int wsysfd;


void
resethstate(void){
	tstate = 0;

	wwid = 0;

	hsrc = 1;
	if(useglobal > uselocal){
		hsrc = 2;
	}

	fstate = 0;
	free(pfilter);
	pfilter = 0;
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
procfilesize(char *fname)
{
	/* read file insted of using file stats */
	/* generated files like /dev/text have size 0 in stats */

	int fd, r;
	ulong sum = 0;
	char buf[4096];

	fd = open(fname, OREAD);
	if(fd < 0){
		return 0;
	}

	do{
		r = read(fd, buf, sizeof buf);
		sum = sum + r;
	}while(r == sizeof buf);

	close(fd);
	
	return sum;
}


ulong
diskfilesize(char *fname)
{
	/* get file size using stats */

	Dir *dir;
	ulong sum;

	dir = dirstat(fname);
	if (dir == nil){
		return 0;
	}

	sum = dir->length;

	free(dir);
	
	return sum;
}


int
filtercheck(char *sarr, int slen, char *fstr)
{
	/* check if withing given length of char array exist filter string */
	/* do so by converint array + size to string then look for substring */

	/* no filter, return true */
	if(fstr == nil){
		return 1;
	}

	/* prepare and compare strings */
	char *sstr = malloc(slen+1);

	memset(sstr, 0, slen+1);
	memcpy(sstr, sarr, slen);

	char *subres = cistrstr(sstr, fstr);

	free(sstr);

	if(subres != 0){
		return 1;
	}

	return 0;
}


char*
fromprompt(void)
{
	/* parse current window text file to scrape any text after prompt */

	#define TPS 128
	#define PBS 512

	int tfd;
	char textpath[TPS];
	char pbf[PBS];

	fstate = 1;

	/* compose full path to text resource of current terminal */
	memset(textpath, 0, TPS);
	snprint(textpath, TPS, "/dev/wsys/%d/text", wwid);

	ulong ts = procfilesize(textpath);

	if(ts == 0){
		return 0;
	}

	/* search in reverse MUST NOT have null chars at the beginning */
	memset(pbf, 64, PBS);
	pbf[PBS-1] = 0;

	
	ulong bfl = PBS - 1;
	if(ts < bfl){
		/* less text than buffer */
		bfl = ts;
	}

	tfd = open(textpath, OREAD);
	long rc = pread(tfd, pbf, bfl, (ts-bfl));
	close(tfd);


	tokenize(getenv("prompt"), &prompt, 1);
	int prc = strlen(prompt);

	char *ssp = 0;
	char *tmpfr = pbf;
	do{
		tmpfr = strstr(tmpfr, prompt);
		if(tmpfr != 0){
			ssp = tmpfr;
			tmpfr = tmpfr + 1;
		}
	} while(tmpfr != 0 && tmpfr < (pbf + PBS - 1));

	free(prompt);

	if(ssp == 0){
		return 0;
	}


	char *pstr = 0;

	if(rc - ((ssp-pbf)+(prc+1)) > 0){
		pstr = (char*) malloc(rc - ((ssp-pbf)+prc));
		memset(pstr, 0, rc - ((ssp-pbf)+prc));
		strncpy(pstr, ssp+prc+1, rc - ((ssp-pbf)+(prc+1)));
	}

	return pstr;
}


void
toprompt(char *text, int len)
{
	#define CHUNKSIZE 64

	int kfd, chunk, offset;
	char ctlbf[4];

	ctlbf[0] = 2;	/* STX (ctrl+b) - move cursor to prompt */
	ctlbf[1] = 5;	/* ENQ (ctrl+e) - move cursor to end of text in promt */
	ctlbf[2] = 21;	/* NAK (ctrl+u) - delete everything behind cursor */
	ctlbf[3] = 0;	/* NULL - everything should be null terminted just in case */

	kfd = open("/dev/kbdin", OWRITE);

	/* clear prompt */
	write(kfd, ctlbf, 3);

	if(len > 0){
		offset = 0;
		do{
			if(len > CHUNKSIZE){
				chunk = CHUNKSIZE;
				len -= CHUNKSIZE;
			} else {
				chunk = len;
				len = 0;
			}
			write(kfd, text+offset, chunk);
			offset += chunk;
		} while(len > 0);
	}

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


	/* find current window id */
	int dsn, dn, wid;
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

	if(wid != wwid){
		resethstate();
		toprompt("", 0);
		wwid = wid;
	}


	/* check for prompt input as search filter */
	if(fstate == 0){
		pfilter = fromprompt();
	}

	/* process local history from /dev/wsys/%id/text */
	if(hsrc == 1 && hop != 0){
		int tfd, rc;

		tokenize(getenv("prompt"), &prompt, 1);

		int prc = strlen(prompt);

		ulong tr;		/* text read */
		ulong tp;		/* text proccesed */

		char *ssp;		/* pointer to prompt */
		char *sse;		/* pointer to EOL */
		char *ssee;		/* pointer to sencond last EOL */
		char *tmpfr;

		int bfl = LBFS - 1;	/* buffer left to read in */
		int bfld = 0;		/* buffer diff between moved and remaining space */

		memset(histpath, 0, HPS);

		/* compose full path to local user history */
		snprint(histpath, HPS, "/dev/wsys/%d/text", wwid);

		/* no history file has been opened yet or we are changing state */
		if(tstate == 0){
			tsize = procfilesize(histpath);
			tstate = 1;

			if(hop > 0){
				tpos = tsize;	/* starting history up */
			} else {
				tpos = 0;		/* starting history down */
			}
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

			tr = 0;
			tp = tpos;
			if(tp != tsize){
				/* not processing history from the end */
				tr = tsize - tp;
			}




			while(tp > 0){
				rc = pread(tfd, linebf + bfld, bfl, (tsize-tr) - bfl);
				bfl -= rc;
				bfld = 0;
				tr += rc;


				/* find prompt */
				ssp = 0;
				tmpfr = linebf;
				do{
					tmpfr = strstr(tmpfr, prompt);
					if(tmpfr != 0){
						ssp = tmpfr;
						tmpfr = tmpfr + 1;
					}
				} while(tmpfr != 0 && tmpfr < (linebf + LBFS - 1));

				/* find newline char */
				sse = 0;
				ssee = 0;
				if(ssp != 0){
					tmpfr = ssp;
				} else {
					tmpfr = linebf;
				}
				do{
					tmpfr = strchr(tmpfr, '\n');
					if(tmpfr != 0){
						ssee = sse;
						sse = tmpfr;
						if(ssp != 0){
							break;	/* if prompt is found only next one is needed */
						}
						tmpfr = tmpfr + 1;
					}
				} while(tmpfr != 0 && tmpfr < (linebf + LBFS - 1));


				/* history hit */
				if((ssp != 0) && (sse != 0)){
					/* if prompt is found and is behing newline char or start of buffer */
					if(*(ssp - 1) == '\n' || *(ssp - 1) == '@' || ssp == linebf){
						/* and ignore empty lines */
						if((sse-ssp) - prc - 1 > 0){
							/* skip displaying same command on direction change */
							if(hop > 1){
								hop--;
							} else {
								/* skip displaying commands without filter string */
								if(filtercheck(ssp+prc+1, (sse-ssp)-prc-1, pfilter)){
									/* command to propt */

									if(tp < (LBFS-1-(ssp-linebf))){
										tp = 0;
									} else {
										tp -= (LBFS-1-(ssp-linebf));
									}

									toprompt(ssp+prc+1, (sse-ssp)-prc-1);
									break;
								}
							}
						}
					}
					if((ssp-linebf) != 0){
						/* move only if there is something to move */
						memmove(linebf + (LBFS-1-(ssp-linebf)), linebf, (ssp-linebf));
					}
					memset(linebf, 64, (LBFS-1-(ssp-linebf)));
					bfl += (LBFS-1-(ssp-linebf));
					if(tp < (LBFS-1-(ssp-linebf))){
						tp = 0;
					} else {
						tp -= (LBFS-1-(ssp-linebf));
					}
				}

				/* buffer move - prompt */
				if((ssp != 0) && (sse == 0)){
					if((ssp-linebf) == 0){
						/* nothing to move if prompt is at the beginning of buffer */
						memset(linebf, 64, (LBFS-1));
						bfl += (LBFS-1);
						if(tp < (LBFS-1)){
							tp = 0;
						} else {
							tp -= (LBFS-1);
						}
					} else {
						/* move the rest carefuly */
						memmove(linebf + (LBFS-1-(ssp-linebf)), linebf, (ssp-linebf));
						memset(linebf, 64, (LBFS-1-(ssp-linebf)));
						bfl += (LBFS-1-(ssp-linebf));
						if(tp < (LBFS-1-(ssp-linebf))){
							tp = 0;
						} else {
							tp -= (LBFS-1-(ssp-linebf));
						}
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
							if(tp < (LBFS-2-(ssee-linebf))){
								tp = 0;
							} else {
								tp -= (LBFS-2-(ssee-linebf));
							}
						} else {
							/* nothing to do but clear whole buffer */
							memset(linebf, 64, (LBFS-1));
							bfl += (LBFS-1);
							if(tp < (LBFS-1)){
								tp = 0;
							} else {
								tp -= (LBFS-1);
							}
						}
					} else {
						/* move till last newline char */
						memmove(linebf + (LBFS-2-(sse-linebf)), linebf, (sse-linebf) + 1);
						memset(linebf, 64, (LBFS-2-(sse-linebf)));
						bfl += (LBFS-2-(sse-linebf));
						if(tp < (LBFS-2-(sse-linebf))){
							tp = 0;
						} else {
							tp -= (LBFS-2-(sse-linebf));
						}
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

			tpos = tp; /* mark where we stopped */
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

			tr = 0;
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
				if(ssp != 0 && (*(ssp - 1) == '\n' || ssp == linebf)){
					/* if prompt is found and is behing newline char or start of buffer */
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
						/* skip empty lines */
						if(linebf[prc] != '\n'){
							/* skip displaying same command on direction change */
							if((sse-linebf) - prc > 1){
								if(hop < -1){
									hop++;
								} else {
									/* skip displaying commands without filter string */
									if(filtercheck(linebf+prc+1, (sse-linebf)-prc-1, pfilter)){
										/* command to propt */
										tp += (sse-linebf)+1;
										toprompt(linebf+prc+1, (sse-linebf)-prc-1);
										break;
									}
								}
							}
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

					/* no more history, reset prompt */
					if(pfilter == 0){
						toprompt("", 0);
					} else {
						toprompt(pfilter, strlen(pfilter));
					}
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
					/* don't know what's going on, reset prompt */
					if(pfilter == 0){
						toprompt("", 0);
					} else {
						toprompt(pfilter, strlen(pfilter));
					}
					tp = tsize;
					hop = 0;
				}
			}
			tpos = tp; /* mark where we stopped */
		}


		close(tfd);

		free(prompt);
	}


	/* process global history from $home/lib/rchistory */
	if(hsrc == 2 && hop != 0){

		home = getenv("home");

		memset(histpath, 0, HPS);
		strcat(histpath, home);
		strcat(histpath, "/lib/rchistory");

		/* no history file has been opened yet or we are at the end */
		if(tstate == 0){
			tpos = diskfilesize(histpath);
			tstate = 1;
		}

		memset(linebf, 0, LBFS);

		hfd = open(histpath, OREAD);

		if(hfd < 0){
			toprompt("# NO GLOBAL HISTORY", 19);
			hop = 0;
		}


		/* history up */
		if(hop > 0){
			int lbc = LBFS - 1;

			for(hc = tpos; hc >= 0; hc--){
				pread(hfd, linebf+lbc, 1, hc-1);
			
				if(linebf[lbc] == '\n' || hc == 0){
					if(hc == 0){
						tpos = 0;
					}
					else {
						tpos = hc - 1;
					}

					if(lbc == LBFS - 1){
						continue;
					}

					if(hop > 1){
						hop--;
						lbc = LBFS - 1;
						continue;
					}

					/* skip displaying commands without filter string */
					if(filtercheck(linebf+lbc+1, LBFS-lbc-1, pfilter)){
						toprompt(linebf+lbc+1, LBFS-lbc-1);
						break;
					} else {
						lbc = LBFS - 1;
						continue;
					}
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
				fr = pread(hfd, linebf+lc, 1, hc);
				if(fr == 0){
					/* no more history, reset prompt */
					if(pfilter == 0){
						toprompt("", 0);
					} else {
						toprompt(pfilter, strlen(pfilter));
					}
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

					/* skip displaying commands without filter string */
					if(filtercheck(linebf, lc, pfilter)){
						toprompt(linebf, lc);
						break;
					} else {
						lc = 0;
						continue;
					}
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

		/* reset history tracking and state if command entered or canceled */
		if(r == 10){
			/* enter key */
			hop = 0;
			resethstate();
		}
		if(r == 127){
			/* delete key */
			hop = 0;
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

	/* init history operations tracking and state */
	hop = 0;
	resethstate();

	if((wsysfd = open("/dev/wsys", OREAD)) < 0){
		sysfatal("%r");
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
