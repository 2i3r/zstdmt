
/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - lz5mt source repository: https://github.com/mcmilk/lz5mt
 */

/* getrusage */
#include <sys/resource.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>

#include "lz5mt.h"
#include "util.h"

/**
 * program for testing threaded stuff on lz5
 */

static void perror_exit(const char *msg)
{
	printf("%s\n", msg);
	fflush(stdout);
	exit(1);
}

static void version(void)
{
	printf("lz5mt version 0.1\n");

	exit(0);
}

static void usage(void)
{
	printf("Usage: lz5mt [options]... [FILE]\n\n");

	printf("Options:\n");
	printf(" -o filename   write result to `filename`\n");
	printf(" -l N          set level of compression (default: 3)\n");
	printf
	    (" -t N          set number of (de)compression threads (default: 2)\n");
	printf
	    (" -i N          set number of iterations for testing (default: 1)\n");
	printf(" -b N          set input chunksize to N KiB (default: auto)\n");
	printf(" -c            compress (default mode)\n");
	printf(" -d            use decompress mode\n");
	printf(" -H            print headline for the testing values\n");
	printf(" -h            show usage and exit\n");
	printf(" -v            show version and exit\n\n");

	printf("With no FILE, read standard input.\n");
	printf("With no `-o filename` option, write to standard output\n");

	exit(0);
}

static void headline(void)
{
	printf
	    ("Type;Level;Threads;InSize;OutSize;Frames;Real;User;Sys;MaxMem\n");
	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

/* for the -i option */
#define MAX_ITERATIONS   1000

int my_read_loop(void *arg, LZ5MT_Buffer * in)
{
	int *fd = (int *)arg;
	ssize_t done = read_loop(*fd, in->buf, in->size);

#if 0
	// ssize_t x = llseek(*fd, 0, SEEK_CUR);
	printf("read_loop(fd=%d, buffer=%p,count=%zu) = %zd off = %zd\n", *fd,
	       in->buf, in->size, done);
	fflush(stdout);
#endif

	in->size = done;
	return 0;
}

int my_write_loop(void *arg, LZ5MT_Buffer * out)
{
	int *fd = (int *)arg;
	ssize_t done = write_loop(*fd, out->buf, out->size);

#if 0
	//ssize_t x = llseek(*fd, 0, SEEK_CUR);
	printf("write_loop(fd=%d, buffer=%p,count=%zu) = %zd off = %zd\n", *fd,
	       out->buf, out->size, done);
	fflush(stdout);
#endif

	out->size = done;
	return 0;
}

static void
do_compress(int threads, int level, int bufsize, int fdin, int fdout,
	    int no_print_suff)
{
	static int first = 1;
	LZ5MT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)&fdin;
	rdwr.arg_write = (void *)&fdout;

	/* 2) create compression context */
	LZ5MT_CCtx *ctx = LZ5MT_createCCtx(threads, level, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = LZ5MT_compressCCtx(ctx, &rdwr);
	if (LZ5MT_isError(ret))
		perror_exit(LZ5MT_getErrorString(ret));

	/* 4) get statistic */
	if (first && !no_print_suff) {
		printf("%d;%d;%zu;%zu;%zu",
		       level, threads,
		       LZ5MT_GetInsizeCCtx(ctx), LZ5MT_GetOutsizeCCtx(ctx),
		       LZ5MT_GetFramesCCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	LZ5MT_freeCCtx(ctx);
}

static void do_decompress(int threads, int bufsize, int fdin, int fdout,
			  int no_print_suff)
{
	static int first = 1;
	LZ5MT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)&fdin;
	rdwr.arg_write = (void *)&fdout;

	/* 2) create compression context */
	LZ5MT_DCtx *ctx = LZ5MT_createDCtx(threads, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = LZ5MT_decompressDCtx(ctx, &rdwr);
	if (LZ5MT_isError(ret))
		perror_exit(LZ5MT_getErrorString(ret));

	/* 4) get statistic */
	if (first && !no_print_suff) {
		printf("%d;%d;%zu;%zu;%zu",
		       0, threads,
		       LZ5MT_GetInsizeDCtx(ctx), LZ5MT_GetOutsizeDCtx(ctx),
		       LZ5MT_GetFramesDCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	LZ5MT_freeDCtx(ctx);
}

#define tsub(a, b, result) \
do { \
(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
 if ((result)->tv_nsec < 0) { \
  --(result)->tv_sec; (result)->tv_nsec += 1000000000; \
 } \
} while (0)

int main(int argc, char **argv)
{
	/* default options: */
	int opt, opt_threads = sysconf(_SC_NPROCESSORS_ONLN), opt_level = 3;
	int opt_mode = MODE_COMPRESS, fdin = -1, fdout = -1;
	int opt_iterations = 1, opt_bufsize = 0;
	struct rusage ru;
	struct timeval tms, tme, tm;
	char *ofilename = NULL;
	int no_print_suff = 0;

	while ((opt = getopt(argc, argv, "vhHl:t:i:dcb:o:")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
		case 'h':	/* help */
			usage();
		case 'H':	/* headline */
			headline();
		case 'l':	/* level */
			opt_level = atoi(optarg);
			break;
		case 't':	/* threads */
			opt_threads = atoi(optarg);
			break;
		case 'i':	/* iterations */
			opt_iterations = atoi(optarg);
			break;
		case 'd':	/* mode = decompress */
			opt_mode = MODE_DECOMPRESS;
			break;
		case 'c':	/* mode = compress */
			opt_mode = MODE_COMPRESS;
			break;
		case 'b':	/* input buffer in MB */
			opt_bufsize = atoi(optarg);
			break;
		case 'o':	/* output filename */
			ofilename = optarg;
			break;
		default:
			usage();
		}
	}

	/**
	 * check parameters
	 */

	/* opt_level = 1..22 */
	if (opt_level < 1)
		opt_level = 1;
	else if (opt_level > LZ5MT_LEVEL_MAX)
		opt_level = LZ5MT_LEVEL_MAX;

	/* opt_threads = 1..LZ5MT_THREAD_MAX */
	if (opt_threads < 1)
		opt_threads = 1;
	else if (opt_threads > LZ5MT_THREAD_MAX)
		opt_threads = LZ5MT_THREAD_MAX;

	/* opt_iterations = 1..MAX_ITERATIONS */
	if (opt_iterations < 1)
		opt_iterations = 1;
	else if (opt_iterations > MAX_ITERATIONS)
		opt_iterations = MAX_ITERATIONS;

	/* opt_bufsize is in KiB */
	if (opt_bufsize > 0)
		opt_bufsize *= 1024;

	/* File IO */
	if (argc < optind + 1)
		if (IS_CONSOLE(stdin)) {
			printf("Please specify a filename to compress, or redirect via stdin.\n");
			exit(0);
		} else
			fdin = fileno(stdin);
	else
		fdin = open_read(argv[optind]);
	if (fdin == -1)
		perror_exit("Opening infile failed");

	if (ofilename == NULL) {
		if (IS_CONSOLE(stdout)) {
			printf("Please specify a filename for output, via `-o` or stdout.\n");
			exit(0);
		} else {
			fdout = fileno(stdout);
			no_print_suff = 1;
		}
	} else
		fdout = open_rw(ofilename);

	if (fdout == -1)
		perror_exit("Opening outfile failed");

	/* begin timing */
	gettimeofday(&tms, NULL);

	for (;;) {
		if (opt_mode == MODE_COMPRESS) {
			do_compress(opt_threads, opt_level, opt_bufsize, fdin,
				    fdout, no_print_suff);
		} else {
			do_decompress(opt_threads, opt_bufsize, fdin, fdout,
				      no_print_suff);
		}

		opt_iterations--;
		if (opt_iterations == 0)
			break;

		lseek(fdin, 0, SEEK_SET);
		lseek(fdout, 0, SEEK_SET);
	}

	/* end of timing */
	if (!no_print_suff) {
		gettimeofday(&tme, NULL);
		timersub(&tme, &tms, &tm);
		getrusage(RUSAGE_SELF, &ru);
		printf(";%ld.%ld;%ld.%ld;%ld.%ld;%ld\n",
		       tm.tv_sec, tm.tv_usec / 1000,
		       ru.ru_utime.tv_sec, ru.ru_utime.tv_usec / 1000,
		       ru.ru_stime.tv_sec, ru.ru_stime.tv_usec / 1000,
		       ru.ru_maxrss);
	}

	/* exit should flush stdout */
	exit(0);
}
