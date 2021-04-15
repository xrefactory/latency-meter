/*


latency-meter is a simple program to measure latency of another program

latency-meter launches a command specified on command line, it connects
to the command by two pipes and repeatedly sends a one line message to
it. The time between sending and receiving is meassured and displayed.


Install/Build

  gcc -o latency-meter latency-meter.c

Usage

  latency-meter [<message>] <command>


Examples

Measure latency of cat program (together with two linux pipes):

  ./latency-meter "cat"

Measure latency of ssh connection to a remote computer

  ./latency-meter "ssh remotehost cat"


*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#define PRINT_PREFIX() 		"latencymeter"
#define TV2MSEC(tv) 		(tv.tv_sec*1000.0 + tv.tv_usec/1000.0)

static int createPipesForPopens(int *in_fd, int *out_fd, int *pin, int *pout) {

    if (out_fd != NULL) {
        if (pipe(pin) != 0) {
            printf("%s: %s:%d: Can't create output pipe\n", PRINT_PREFIX(), __FILE__, __LINE__);
            return(-1);
        }
        *out_fd = pin[1];
        // printf("pipe pin: %p %p: %d %d\n", pin, pin+1, pin[0], pin[1]);
    }
    if (in_fd != NULL) {
        if (pipe(pout) != 0) {
            printf("%s: %s:%d: Can't create input pipe\n", PRINT_PREFIX(), __FILE__, __LINE__);
            if (out_fd != NULL) {
                close(pin[0]);
                close(pin[1]);
            }
            return(-1);
        }
        *in_fd = pout[0];
        // printf("pipe pout: %p %p: %d %d\n", pout, pout+1, pout[0], pout[1]);
    }
    return(0);
}


static void closePopenPipes(int *in_fd, int *out_fd, int pin[2], int pout[2]) {
    if (out_fd != NULL) {
        close(pin[0]);
        close(pin[1]);
    }
    if (in_fd != NULL) {
        close(pout[0]);
        close(pout[1]);
    }
}

pid_t popen2(char *command, int *in_fd, int *out_fd) {
    int                 pin[2], pout[2];
    pid_t               pid;
    int                 r, md;

    if (createPipesForPopens(in_fd, out_fd, pin, pout) == -1) return(-1);

    pid = fork();

    if (pid < 0) {
        printf("%s:%s:%d: fork failed in popen2: %s\n", PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        closePopenPipes(in_fd, out_fd, pin, pout);
        return pid;
    }

    if (pid == 0) {

        if (out_fd != NULL) {
            close(pin[1]);
            dup2(pin[0], 0);
            close(pin[0]);
        } else {
            // do not inherit stdin, if no pipe is defined, close it.
            close(0);
        }

        if (in_fd != NULL) {
            close(pout[0]);
            dup2(pout[1], 1);
            close(pin[1]);
        } else {
            // if there is no pipe for stdout, join stderr.
            dup2(2, 1);
        }

		execlp("bash", "bash", "-c", command, NULL);

        fprintf(stderr, "%s:%s:%d: Exec failed in popen2: %s\n", PRINT_PREFIX(), __FILE__, __LINE__, strerror(errno));
        exit(1);
    }

    if (in_fd != NULL) {
        close(pout[1]);
    }
    if (out_fd != NULL) {
        close(pin[0]);
    }

    return pid;
}

int main(int argc, char **argv) {
	int 			i, n, r, msglen, blen;
	int				infd, outfd;
	char			cc;
	struct timeval 	tv1, tv2;
	char			*msg, *b;

	if (argc != 2 && argc != 3) {
		printf("usage :\n");
		printf("  %s [<message>] <command>\n", argv[0]);
		printf("examples :\n");
		printf("  %s \"cat\"\n", argv[0]);
		printf("  %s \"ssh remotehost cat\"\n", argv[0]);
		printf("  %s \"hello world\" \"ssh remotehost xargs -n 2 echo\"\n", argv[0]);		
		printf("  %s \"mymessagetotest\" \"myprogramtotest\"\n", argv[0]);
		exit(-1);
	}

	if (argc == 2) {
		msg = strdup("ping packet\n");
		popen2(argv[1], &infd, &outfd);
	} else {
		msg = malloc(strlen(argv[1]+2));
		sprintf(msg, "%s\n", argv[1]);
		popen2(argv[2], &infd, &outfd);
	}

	msglen = strlen(msg);
	// possible expect more chars
	blen = msglen * 2 + 1024;
	b = malloc(blen);
	
	close(0);
	dup2(infd, 0);

	close(1);
	dup2(outfd, 1);

	for(i=0; i<100; i++) {
		usleep(200000);
		gettimeofday(&tv1, NULL);
		// sending
		n = 0;
		for(;;) {
			r = write(1, msg+n, msglen-n) ;
			if (r <= 0) exit(-1);
			n += r;
			if (n >= msglen) break;
		}
		// receiving
		for(n=0; n<blen; n++) {
			r = read(0, &cc, 1);
			if (r < 0) exit(-1);
			if (r == 0) break;
			b[n] = cc;
			if (cc == '\n') break;
		}
		if (n >= blen) exit(-1);
		b[n] = 0;
		gettimeofday(&tv2, NULL);
		fprintf(stderr, "%3d: %.*s --> %s: roundtrip time: %0.3g ms\n", i, msglen-1, msg, b, TV2MSEC(tv2)-TV2MSEC(tv1)) ;
	}
}

