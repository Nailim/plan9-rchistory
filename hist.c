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
textsize(char *fname)
{
	/* read text file insted of using file stats */
	/* some files like /dev/text have size 0 since they are generated */

	int fd, r;
	ulong sum = 0;
		
	char buf[1024];

	/* get current /dev/text size */
	fd = open(fname, OREAD);
	do{
		r = read(fd, buf, sizeof buf);
		sum = sum + r;
	}while(r == sizeof buf);
	close(fd);
	
	return sum;
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


	/* stand alone segment - print out history */
	char linebf[LBFS];


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
				rc = read(hfd, linebf, sizeof linebf);
				write(1, linebf, rc);
			}while(rc == sizeof linebf);
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

		int bfl = LBFS - 1;	/* buffer left to read in */
		int bfld = 0;		/* buffer diff between moved and remaining space */

		memset(linebf, 0, LBFS);

		/* get current /dev/text size before we expand it */
		tsize = textsize("/dev/text");

		print("# local history\n");

		tfd = open("/dev/text", OREAD);

		/* first read exception - less text than buffer */
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
			if((ssp != 0) && (*(ssp - 1) == '\n' || ssp == linebf)){
				/* if prompt is found and is behing newline char or start of buffer */
				if(ssp-linebf > 0){
					/* align prompt with start of the buffer */
					memmove(linebf, ssp, LBFS - (ssp-linebf));
					memset(linebf + LBFS - (ssp-linebf), 0, (ssp-linebf));
					bfl += (ssp-linebf);
					tp += (ssp-linebf);
				}

				/* we trust the buffer is long enough for the whole command */

				sse = strchr(linebf, '\n');
				if(sse != 0){
					/* print out command without propt */
					if(linebf[prc] != '\n'){
						/* and ignore empty lines */
						if((sse-linebf) - prc > 1){
							write(1, linebf + prc + 1, (sse-linebf) - prc);
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
				bfl = LBFS - 1;
				tp += strlen(linebf);
				
				memset(linebf, 0, (LBFS-1));
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

	exits(nil);
}
