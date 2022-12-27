#include <u.h>
#include <libc.h>
#include <keyboard.h>

/* processing line buffer size */
#define LBFS 1024


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

		long tr = 0;	/* text read */
		long tp = 0;	/* text proccesed */

		char* ssp;		/* pointer to prompt */
		char* sse;		/* pointer to EOL */

		int bfl = LBFS - 1;	/* buffer left to read in */
		int bfld = 0;		/* buffer diff between moved and remaining space */

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
