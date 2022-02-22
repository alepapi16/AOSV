#include <pthread.h>
#include <semaphore.h>

#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


#define BUF_SIZE 100  // at least 1

// test message buffer
static char **buf;
static int rd_idx = BUF_SIZE-1,  wr_idx = BUF_SIZE-1;

static int max_msg_len = 100;  // shall be at least 2

// number of child threads: load/2 readers, load-load/2 writers
static int load = 4;

static struct lgroup_t *test_group;
static sem_t wr_lock, rd_lock, buf_lock;


// children: pthread routines

static void* reader(void *arg)
{
	int res=0;
	char *msg = calloc(max_msg_len, sizeof(char));

	// check rd_idx atomically
	sem_wait(&rd_lock);
	while(rd_idx >= 0)
	{
		if((res = deliver_message(test_group, msg, max_msg_len)) < 0)
		{
			sem_post(&rd_lock);
			perror("Error reading");
		}
		else if (res > 0)
		{
			// double check that the read message is actually what was expected
			sem_wait(&buf_lock);
			char *exp = buf[rd_idx--];
			sem_post(&rd_lock);  // yield other readers the critical section
			
			TEST_ASSERT_(!strcmp(msg, exp), 0, "reading what was expected");
			msg[0] = '\0';
			free(exp);
		}
		else
		{
			sem_post(&rd_lock);
		}
		sem_wait(&rd_lock);
	}
	sem_post(&rd_lock);

	free(msg);
	return 0;
}

static void* writer(void *arg)
{
	// check wr_idx atomically
	sem_wait(&wr_lock);
	while (wr_idx >= 0)
	{
		sem_post(&wr_lock);
		// rand_string must be at least 2 bytes, at most max_msg_len ('\0' included)
		int len = rand() % (max_msg_len-1) +2;
		// message crafted outside the critical section
		char *msg = rand_string(len);
		
		sem_wait(&wr_lock);
		if (publish_message(test_group, msg) < 0)
		{
			sem_post(&wr_lock);
			perror("Write error");
		}
		else
		{
			buf[wr_idx--] = msg;
			sem_post(&wr_lock);
			sem_post(&buf_lock);
		}
		sem_wait(&wr_lock);
	}
	sem_post(&wr_lock);

	return 0;
}


// father: main test routine

void test_rw_fifo(void)
{
	int err, res = -1;
	
	// installing test group
	test_group = lgroup_init();
	res = install_group(test_group, "fifo");
	TEST_ASSERT_(res>=0, 1, "test group - install ok");
	
	// group already existed
	if(!res)
	{
		// reset group
		set_send_delay(test_group, 0);
		revoke_delayed_messages(test_group);
		char msg[2];
		while (deliver_message(test_group, msg, 2));  // empty message queue
	}
	
	// initialize global variables
	pthread_t *tid = calloc(load, sizeof(pthread_t));  // threads
	sem_init(&wr_lock, 0, 1);  // semaphores
	sem_init(&rd_lock, 0, 1);
	sem_init(&buf_lock, 0, 0);
	buf = calloc(BUF_SIZE, sizeof(char*));  // message buffer

	// spawn writers
	for (int i=0; i < load/2; i++)
		if((err = pthread_create(&(tid[i]), NULL, &writer, NULL)))
			printf("Can't create thread %d: %s.\n", i, strerror(err));
		
	// spawn readers
	for (int i=load/2; i < load; i++)
		if((err = pthread_create(&(tid[i]), NULL, &reader, NULL)))
			printf("Can't create thread %d: %s.\n", i, strerror(err));
	
	// join threads
	for (int i=0; i<load; i++)
		pthread_join(tid[i], NULL);

	sem_destroy(&wr_lock);
	sem_destroy(&rd_lock);
	sem_destroy(&buf_lock);
	lgroup_destroy(test_group);
}
