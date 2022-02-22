#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils.h"


int next_group()
{
	struct dirent *dp;
	DIR *dfd;

	char *dir = "/dev";

	if ((dfd = opendir(dir)) == NULL) {
		fprintf(stderr, "Can't open %s.\n", dir);
		return 0;
	}

	char devname[100];
	int n = -1;
	
	while ((dp = readdir(dfd)) != NULL) {
		sprintf(devname, "%s", dp->d_name);
		if(!strncmp(devname,"group",5))
		{
			int m;
			if(!sscanf(devname, "group%d", &m))
			{
				printf("Can't read group number.\n");
			}
			n = m>n ? m : n;
		}
	}
	
	return n+1;
}

char *rand_string(size_t size)
{
	// POST: char* @output : sizeof(@output) = @size, terminator included
	const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *str = malloc(size);
	if (size) {
		--size;
		for (size_t n = 0; n < size; n++) {
			int key = rand() % (int) (sizeof charset - 1);
			str[n] = charset[key];
		}
		str[size] = '\0';
	}
	return str;
}  

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINVAL);

    return res;
}
