#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lgroups.h"
#include "utils.h"

#define INTERVAL 1000  // milliseconds

static int len;  // messages length
static int load;  // threads amount
static int stop = 0;  // termination condition

static pthread_t* tid;
static sem_t tx_lock, wr_full_lock, rd_null_lock;
static struct lgroup_t *group;  // synchronization group

static unsigned long read_tx = 0, write_tx = 0, wr_full = 0, rd_null = 0;
static FILE* f_data;


static void* reader(void *arg)
{
	int res=0, count=0, empty = 0;
	char *buf = calloc(len, sizeof(char));

	while(!stop){
		buf[0] = '\0';

		if((res = deliver_message(group, buf, len)) < 0)
		{
			perror("Error reading device file");
			exit(EXIT_FAILURE);
		}
		if (res > 0) count++;
		else 
		{
			empty++;
			sched_yield();
		}
	}

	sem_wait(&tx_lock);
	read_tx += count;
	sem_post(&tx_lock);
	
	sem_wait(&rd_null_lock);
	rd_null += empty;
	sem_post(&rd_null_lock);

	free(buf);
	return 0;
}

static void* writer(void *arg)
{
	int res=0, count=0, full = 0;
	char* buf = rand_string(len);

	while (!stop) {

		if ((res = publish_message(group, buf)) < 0) {
			if(errno==ENOSPC) {
				full++;
			}
			else {
				perror("Write");
				break;
			}
			continue;
		}
		count++;
	}

	sem_wait(&tx_lock);
	write_tx += count;
	sem_post(&tx_lock);
	
	sem_wait(&wr_full_lock);
	wr_full += full;
	sem_post(&wr_full_lock);

	free(buf);
	return 0;
}

int main(int argc, char** argv)
{
	int err;

	if(argc<4)
	{
		printf("PARAMETERS: arg1=load, arg2=msg_size.\n");
		exit(EXIT_FAILURE);
	}
	
	// installing benchmarkgroup
	group = lgroup_init();
	
	int res = install_group(group, argv[3]);
	
	if(res < 0)  // install failure
	{
		perror("Failed installing group");
		goto error;
	}
	else if(!res)  // group already existed
	{
		// reset group
		set_send_delay(group, 0);
		revoke_delayed_messages(group);
		char msg[2];
		while (deliver_message(group, msg, 2));  // empty message queue
	}
	
	// open group
	if (!(f_data = fopen("data/rw_tps.data", "a")))
	{
		perror("Open data.txt");
		exit(EXIT_FAILURE);
	}

	// parsing parameters
	int load = atoi(argv[1]);
	len = atoi(argv[2]);
	
	// prepare threads creation
	tid = calloc(load, sizeof(pthread_t));
	sem_init(&tx_lock, 0, 1);
	sem_init(&wr_full_lock, 0, 1);
	sem_init(&rd_null_lock, 0, 1);
	
	// spawn writers
	for (int i=0; i<load/2; i++)
	{
		int* j = malloc(sizeof(int)); 
		*j = i;

		if((err = pthread_create(&(tid[i]), NULL, &writer, j)))
			printf("Can't create thread %d: %s.\n", i, strerror(err));
	}

	// spawn readers
	for (int i=load/2; i<load; i++)
	{
		int* j = malloc(sizeof(int)); 
		*j = i;

		if((err = pthread_create(&(tid[i]), NULL, &reader, j)))
			printf("Can't create thread %d: %s.\n", i, strerror(err));
	}

	usleep(INTERVAL * 1000);  // benchmark lasting
	stop = 1;
	
	// join threads
	for (int i=0; i<load; i++)
		pthread_join(tid[i], NULL);

	// output results
	fprintf(f_data, "load=%d  len=%d  txs=%ld  tps=%.2f  wr_full=%ld  rd_null=%ld  read_txs=%ld\n", 
			load, len, read_tx, (float)(read_tx)/(INTERVAL/1000),
			wr_full, rd_null, read_tx);

	sem_destroy(&tx_lock);
	sem_destroy(&wr_full_lock);
	sem_destroy(&rd_null_lock);
	lgroup_destroy(group);
	fclose(f_data);
	exit(EXIT_SUCCESS);

error:
	lgroup_destroy(group);
	exit(EXIT_FAILURE);
}
