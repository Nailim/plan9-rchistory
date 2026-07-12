#include <u.h>
#include <libc.h>
#include <keyboard.h>


/* processing line buffer size */
#define LBFS 1024
#define HPS 512


static char *prompt;
static char *home;

static ulong tsize;


ulong
procfilesize(char *fname)
{
	/* read file instead of using file stats */
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


char*
lbgrow(char *buf, long newcap)
{
	/* grow linebf when a single parsed command exceeds the current window */
	/* NOTE: realloc may move the block - any pointer derived from the old */
	/*       buf (ssp/sse) must be recomputed after calling this */

	char *nb;

	nb = realloc(buf, newcap);
	if(nb == nil){
		sysfatal("hist: realloc: %r");
	}
	return nb;
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
	int uselocal = 1;
	int useglobal = 0;

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


	char *linebf;
	int lbcap;

	linebf = malloc(LBFS);
	if(linebf == nil){
		sysfatal("hist: malloc: %r");
	}
	lbcap = LBFS;


	/* print global history from $home/lib/rchistory */
	if(useglobal){
		int hfd, rc;

		home = getenv("home");
		/* compose full path to global user history */
		char histpath[HPS];
		memset(histpath, 0, sizeof histpath);
		strcat(histpath, home);
		strcat(histpath, "/lib/rchistory");

		print("# global history\n");

		hfd = open(histpath, OREAD);
		if(hfd > 0){
			do{
				rc = read(hfd, linebf, lbcap);
				write(1, linebf, rc);
			}while(rc == lbcap);
			close(hfd);
		}

		free(home);
	}


	/* parse and print local history from /dev/text */
	if(uselocal){
		int tfd, rc;

		tokenize(getenv("prompt"), &prompt, 1);

		int prc = strlen(prompt);

		ulong tr = 0;	/* text read */
		ulong tp = 0;	/* text proccesed */

		char *ssp;		/* pointer to prompt */
		char *sse;		/* pointer to EOL */

		int bfl = lbcap - 1;	/* buffer left to read in */
		int bfld = 0;		/* buffer diff between moved and remaining space */
		int oldcap, newcap;	/* used when growing linebf */

		memset(linebf, 0, lbcap);

		/* get current /dev/text size before we expand it */
		tsize = procfilesize("/dev/text");

		print("# local history\n");

		tfd = open("/dev/text", OREAD);

		/* first read exception - less text than buffer */
		if(bfl > tsize){
			bfl = tsize;
			bfld = ((lbcap-1) - bfl);
		}

		while(tp < tsize){
			rc = read(tfd, linebf + ((lbcap-1)-bfl) - bfld,  bfl);
			bfl -= rc;
			bfld = 0;
			tr += rc;

			ssp = strstr(linebf, prompt);
			if((ssp != 0) && (*(ssp - 1) == '\n' || ssp == linebf)){
				/* if prompt is found and is behind newline char or start of buffer */
				if(ssp-linebf > 0){
					/* align prompt with start of the buffer */
					memmove(linebf, ssp, lbcap - (ssp-linebf));
					memset(linebf + lbcap - (ssp-linebf), 0, (ssp-linebf));
					bfl += (ssp-linebf);
					tp += (ssp-linebf);
				}

				sse = strchr(linebf, '\n');
				if(sse != 0){
					/* print out command without propt */
					if(linebf[prc] != '\n'){
						/* and ignore empty lines */
						if((sse-linebf) - prc > 1){
							write(1, linebf + prc + 1, (sse-linebf) - prc);
						}
					}

					memmove(linebf, sse+1, lbcap - (sse-linebf) + 1);
					memset(linebf + lbcap - (sse-linebf) + 1, 0, (sse-linebf));
					bfl += ((sse-linebf) + 1);
					tp += ((sse-linebf) + 1);
				} else {
					/* prompt is aligned to the start of the buffer but no */
					/* terminating newline was found - the command is longer */
					/* than lbcap, grow the buffer instead of falling through */
					/* to the "no hit" exception below (which would silently */
					/* drop this command from the output) */
					oldcap = lbcap;
					newcap = oldcap * 2;
					linebf = lbgrow(linebf, newcap);
					memset(linebf + oldcap, 0, (newcap-oldcap));
					bfl += (newcap-oldcap);
					lbcap = newcap;
					/* tp is intentionally left untouched, tr already */
					/* guarantees forward progress next iteration */
				}
			} else {
				/* if there is no prompt in buffer */
				/* move to next new line character instead */
				sse = strchr(linebf, '\n');
				if((sse != 0) && ((sse-linebf) < lbcap-2)){
					memmove(linebf, sse+1, lbcap - (sse-linebf) + 1);
					memset(linebf + lbcap - (sse-linebf) + 1, 0, (sse-linebf));
					bfl += ((sse-linebf) + 1);
					tp += ((sse-linebf) + 1);
				}
			}

			/* no hit in buffer exception */
			if(bfl == 0){
				bfl = lbcap - 1;
				tp += strlen(linebf);

				memset(linebf, 0, (lbcap-1));
			}

			/* last read (end of data) exception */
			if((tr+bfl) > tsize){
				bfld = bfl;
				bfl = bfl - ((tr+bfl) - tsize);
				bfld = bfld - bfl;
			}

			/* tainted history - less to read than detected at start */
			if((rc == 0) && (bfl != 0)){
				/* don't know what's going oy */
				tp = tsize;
			}
		}

		close(tfd);

		free(prompt);

	}

	free(linebf);

	exits(nil);
}
