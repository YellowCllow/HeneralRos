#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "find/find.h"

int main(int argc, char **argv) {
    if (argc != 3) {
	printf("Usage: myfind dir filename");
	return 1;
    }

    find(argv[1], argv[2]);

    return 0;
}
