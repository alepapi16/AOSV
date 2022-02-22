#include <pthread.h>
#include <semaphore.h>

#define TEST_NO_MAIN
#include "acutest.h"

#include "lgroups.h"

#define EPSILON 100000


static int n = 10;  // test iterations

static struct lgroup_t *test_group;
static sem_t awake_semaphore, sleep_semaphore;


static void* sleeper(void *arg)
{
	int res;
	
	while(n)
	{
		// invoke the awaker
		sem_post(&awake_semaphore);
		
		res = sleep_on_barrier(test_group);
		TEST_ASSERT_(res==0, 0, "sleep ioctl: %s", strerror(errno));
		
		// invoke the awaker
		sem_post(&awake_semaphore);
		
		// wait the awaker's signal
		sem_wait(&sleep_semaphore);
	}
}

static void* awaker(void *arg)
{
	int res;
	
	while(n)
	{
		// wait that all sleepers are sleeping
		sem_wait(&awake_semaphore);
		sem_wait(&awake_semaphore);
		sem_wait(&awake_semaphore);
		
		// make sure sleepers invoked ioctl (non-deterministic)
		usleep(EPSILON);
		
		res = awake_barrier(test_group);
		TEST_ASSERT_(res==0, 0, "awake ioctl, exp: %d, got: %d", 0, res);
		
		// wait that all sleepers are waiting
		sem_wait(&awake_semaphore);
		sem_wait(&awake_semaphore);
		sem_wait(&awake_semaphore);
		
		res = awake_barrier(test_group);
		TEST_ASSERT_(res==2, 0, "awake ioctl, exp: %d, got: %d", 0, res);
		
		n--;
		
		// unleash the sleepers
		sem_post(&sleep_semaphore);
		sem_post(&sleep_semaphore);
		sem_post(&sleep_semaphore);
	}
}

void test_barrier(void)
{
	int err, res = -1;
	
	// installing test group
	test_group = lgroup_init();
	res = install_group(test_group, "barrier");
	TEST_ASSERT_(res>=0, 1, "test group - install ok");

	// initializing global variables
	pthread_t *tid = calloc(4, sizeof (pthread_t));  // pthreads
	sem_init(&sleep_semaphore, 0, 0);  // semaphores
	sem_init(&awake_semaphore, 0, 0);
	
	// spawn sleepers
	for (int i = 0; i < 3; i++)
	{
		err = pthread_create(&(tid[i]), NULL, &sleeper, NULL);
		TEST_ASSERT_(!err, 0, "spawn sleeper%d: %s", i+1, strerror(errno));
	}
	
	// spawn awaker
	err = pthread_create(&(tid[3]), NULL, &awaker, NULL);
	TEST_ASSERT_(!err, 0, "spawn awaker: %s", strerror(errno));
	
	// join threads
	for (int i = 0; i < 4; i++)
		pthread_join(tid[i], NULL);

	sem_destroy(&sleep_semaphore);
	sem_destroy(&awake_semaphore);
	lgroup_destroy(test_group);
}
