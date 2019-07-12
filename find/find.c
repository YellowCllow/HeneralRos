#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "find.h"

#define _STRLEN_ 1024
#define BUFF_LEN 2048
#define THREAD_NUMS 8
#define QUEUE_SIZE 256

const char *OUT_FILE = "out.txt";

struct find_thread_t {
    char inStartDir[_STRLEN_];
};

struct pool_t {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    char **queue;
    int thread_count;
    int queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
    int started;
};

const char *inFindFile;

void save_result(const char *name) {
    FILE *fp = fopen(OUT_FILE, "a+");

    fprintf(fp, "%s\n", name);
    fflush(fp);

    fclose(fp);
}

int _find(void *ptr) {
    DIR *pDir;
    struct dirent *pDirent;
    struct stat bufInfo;
    char bufStr[BUFF_LEN];

    struct find_thread_t *t = ptr;

    if (!(pDir = opendir(t->inStartDir))) {
	return 1;
    }

    while ((pDirent = readdir(pDir))) {
	if (!strcmp(pDirent->d_name, ".") || !strcmp(pDirent->d_name, "..")) {
	    continue;
	}

	if (pDirent->d_type == DT_LNK) {  // Avoid link folder
	    continue;
	}

	snprintf(bufStr, BUFF_LEN, "%s/%s", t->inStartDir, pDirent->d_name);

	if (!strcmp(pDirent->d_name, inFindFile)) {
	    /*printf("Founded %s\n", bufStr);*/

	    save_result(bufStr);

	    return 0;
	}

	stat(bufStr, &bufInfo);

	if (S_ISDIR(bufInfo.st_mode) && !S_ISLNK(bufInfo.st_mode)) {
	    if (!_find(bufStr)) {
		return 0;
	    }
	}
    }

    closedir(pDir);

    return 1;
}

void *thread_find(void *ptr) {
    struct pool_t *pool = ptr;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    for (;;) {
	pthread_mutex_lock(&(pool->lock));

	while ((pool->count == 0) && (!pool->shutdown)) {
	    pthread_cond_wait(&(pool->notify), &(pool->lock));
	}

	if (pool->shutdown) {
	    break;
	}

	char *arg = pool->queue[pool->head];
	pool->head = (pool->head + 1) % pool->queue_size;
	pool->count--;

	pthread_mutex_unlock(&(pool->lock));

	int res;

	if (!(res = _find(arg))) {
	    exit(1);
	}
    }

    pool->started--;

    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);

    return NULL;
}

void pool_init(struct pool_t *pool) {
    pool->thread_count = 0;
    pool->queue_size = QUEUE_SIZE;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    if (!(pool->threads = malloc(sizeof(pthread_t) * THREAD_NUMS))) {
	perror("Malloc");
	exit(1);
    }

    if (!(pool->queue = malloc(sizeof(char *) * THREAD_NUMS))) {
	perror("Malloc");
	exit(1);
    }

    if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
	(pthread_cond_init(&(pool->notify), NULL) != 0)) {
	perror("Pthread");
	exit(1);
    }

    for (int i = 0; i < THREAD_NUMS; i++) {
	if (pthread_create(&(pool->threads[i]), NULL, thread_find,
			   (void *)pool) != 0) {
	    printf("Error: thread creation\n");
	    return;
	}
	pool->thread_count++;
	pool->started++;
    }
}

int pool_add(struct pool_t *pool, void *argument) {
    int err = 0;
    int next;

    if (pool == NULL) {
	return -1;
    }

    if (pthread_mutex_lock(&(pool->lock)) != 0) {
	return -1;
    }

    next = (pool->tail + 1) % pool->queue_size;

    do {
	if (pool->count == pool->queue_size) {
	    err = 1;
	    break;
	}

	if (pool->shutdown) {
	    err = -1;
	    break;
	}

	pool->queue[pool->tail] = argument;
	pool->tail = next;
	pool->count += 1;

	if (pthread_cond_signal(&(pool->notify)) != 0) {
	    err = -1;
	    break;
	}
    } while (0);

    if (pthread_mutex_unlock(&pool->lock) != 0) {
	err = -1;
    }

    return err;
}

void pool_detach(struct pool_t *pool) {
    for (int i = 0; i < pool->thread_count; i++) {
	pthread_detach(pool->threads[i]);
    }
}

int find(const char *inStartDir, const char *file) {
    DIR *pDir;
    struct dirent *pDirent;
    struct stat bufInfo;
    char bufStr[BUFF_LEN];

    struct pool_t pool = {};
    inFindFile = file;

    pool_init(&pool);

    if ((pDir = opendir(inStartDir)) == NULL) {
	return 1;
    }

    while ((pDirent = readdir(pDir))) {
	if (!strcmp(pDirent->d_name, ".") || !strcmp(pDirent->d_name, "..") ||
	    !strcmp(pDirent->d_name, "proc") ||  // Ignore system dirs (may be
						 // extended with other sys
						 // diers)
	    !strcmp(pDirent->d_name, "sys")) {
	    continue;
	}

	snprintf(bufStr, BUFF_LEN, "%s%s%s", inStartDir,
		 !strcmp(inStartDir, "/") ? "" : "/", pDirent->d_name);

	if (!strcmp(pDirent->d_name, inFindFile)) {
	    printf("Founded %s\n", bufStr);
	    return 0;
	}

	stat(bufStr, &bufInfo);

	if (S_ISDIR(bufInfo.st_mode) && !S_ISLNK(bufInfo.st_mode)) {
	    pool_add(&pool, strdup(bufStr));
	}
    }

    closedir(pDir);

    pool_detach(&pool);
    pthread_exit(0);

    return 0;
}
