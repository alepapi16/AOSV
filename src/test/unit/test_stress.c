#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


#define GROUPS 4
char **g_ids;

static int stop = 0;
static int duration = 10; // seconds (test name in unit.c)
static unsigned int delay = 100; // ms
static unsigned long msg_sizes[3] = {100, 200, 300};
static unsigned long strg_sizes[3] = {10000, 20000, 30000};

#define SWITCH_GROUP 0
#define WRITE 1
#define READ 2
#define SET_DELAY 3
#define UNSET_DELAY 4
#define SLEEP 5
#define REVOKE 6
#define SET_MSG_SIZE 7  // root only
#define SET_STOR_SIZE 8  // root only
static int test_commands = 7;

static FILE* log;


/**
 * Chooses a random group among those in g_ids
 * @param group : an initialized group
 * @return 0 on success, or...
 * -1 on errors
 */
static char* choose_group(struct lgroup_t *group, unsigned long *size)
{
	int res, x = rand() % GROUPS;
	res = install_group(group, g_ids[x]);
	TEST_ASSERT_(!res, 0, "choose_group: installing %s, got: %d.", g_ids[x], res);
	res = get_max_message_size(group, size);
	TEST_ASSERT_(!res, 0, "choose_group: msg_size of %s, got: %d.", g_ids[x], res);
	return g_ids[x];
}

// Continuously/Randomly invoke commands on available groups.
static void *stresser(void *arg)
{
	int res = 0, t_id = *((int*)arg);
	char *g_id;
	unsigned long size;
	struct lgroup_t *group = lgroup_init();
	g_id = choose_group(group, &size);
	
	while(!stop)
	{
		switch(rand() % test_commands)
		{
			case SWITCH_GROUP:
			{
				char old[32]; strncpy(old, g_id, 32);
				g_id = choose_group(group, &size);
				fprintf(log, "t%d: switch %.5s -> %.5s.\n", t_id, old, g_id);
				break;
			}
			
			case WRITE:
			{
				char *msg = rand_string(size);
				res = publish_message(group, msg);
				if(res == -2 && errno == EMSGSIZE)
				{
					// update size
					unsigned long newsize;
					res = get_max_message_size(group, &newsize);
					fprintf(log, "t%d: EMSGSIZE %.5s, updating size %lu -> %lu.\n", t_id, g_id, size, newsize);
					// commented: could be that some other thread already re-changed it!
					//TEST_ASSERT_(size > newsize, 0, "stresser.write: EDABMSG, old = %lu, new = %lu.", size, newsize);
					size = newsize;
					res = 0;
				}
				else if(res == -2 && errno == ENOSPC)
				{
					fprintf(log, "t%d: ENOSPC %s, msg = %lu.\n", t_id, g_id, size);
					res = 0;
				}
				else
				{
					TEST_ASSERT_(res == size, 0, "stresser.write: expected: %d, got: %d.", size, res);
				}
				fprintf(log, "t%d: written %d B to %.5s.\n", t_id, res, g_id);
				free(msg);
				break;
			}
			
			case READ:
			{
				char *msg = calloc(size, sizeof(char));
				res = deliver_message(group, msg, size);
				TEST_ASSERT_(res >= 0, 0, "stresser.read: %s", strerror(res));
				fprintf(log, "t%d: read %d B from %.5s.\n", t_id, res, g_id);
				break;
			}
			
			case SET_DELAY:
			{
				res = set_send_delay(group, delay);
				TEST_ASSERT_(!res, 0, "stresser.set_delay: %s.", strerror(res));
				fprintf(log, "t%d: set delay to %.5s.\n", t_id, g_id);
				break;
			}
			
			case UNSET_DELAY:
			{
				res = set_send_delay(group, 0);
				TEST_ASSERT_(!res, 0, "stresser.unset_delay: %s.", strerror(res));
				fprintf(log, "t%d: unset delay from %.5s.\n", t_id, g_id);
				break;
			}
			
			case SLEEP:
			{
				res = sleep_on_barrier(group);
				fprintf(log, "t%d: sleep on %.5s.\n", t_id, g_id);
				TEST_ASSERT_(!res, 0, "stresser.sleep: %s.", strerror(res));
				fprintf(log, "t%d: awakened from %.5s.\n", t_id, g_id);
				break;
			}
			
			case REVOKE:
			{
				res = revoke_delayed_messages(group);
				TEST_ASSERT_(!res, 0, "stresser.revoke: %s.", strerror(res));
				fprintf(log, "t%d: revoked delayed messages on %.5s.\n", t_id, g_id);
				break;
			}
			
			case SET_MSG_SIZE:
			{
				int x = rand() % (sizeof(msg_sizes) / sizeof(unsigned long));
				res = set_max_message_size(group, msg_sizes[x]);
				TEST_ASSERT_(!res, 0, "stresser.set_msg_size: %s.", strerror(res));
				fprintf(log, "t%d: set %lu msg_size to %.5s.\n", t_id, msg_sizes[x], g_id);
				break;
			}
			
			case SET_STOR_SIZE:
			{
				int x = rand() % (sizeof(strg_sizes) / sizeof(unsigned long));
				res = set_max_storage_size(group, strg_sizes[x]);
				TEST_ASSERT_(!res, 0, "stresser.set_strg_size: %s.", strerror(res));
				fprintf(log, "t%d: set %lu strg_size to %.5s.\n", t_id, strg_sizes[x], g_id);
				break;
			}
			
			default:
			{
				TEST_ASSERT_(0, 0, "stresser.default: why here?");
			}
		}
	}
	lgroup_destroy(group);
}

// Keep awaking every group's barrier.
static void *awaker(void *arg)
{
	struct lgroup_t *group = lgroup_init();
	int res = -1;
	while(!stop)
	{
		usleep(500000); // 0.5 s
		for(int i = 0; i < GROUPS; i++)  // awake all
		{
			res = install_group(group, g_ids[i]);
			TEST_ASSERT_(!res, 0, "awaker: %s already installed.", g_ids[i]);
			awake_barrier(group);
		}
	}
	lgroup_destroy(group);
}

// Install GROUPS new groups and spawn stresser children.
void test_stress(void)
{
	// root only commands
	if(!geteuid())
	{
		// superuser can set sysfs params
		test_commands = 9;
	}
	
	g_ids = calloc(GROUPS, sizeof(char*));
	
	// install GROUPS fresh new groups,
	// write their ids in g_ids
	struct lgroup_t *group = lgroup_init();
	int i = 0, res = -1;
	char buf[100];
	while(i < GROUPS)
	{
		sprintf(buf, "stress%d", i);
		g_ids[i] = malloc(strlen(buf)+1);
		strcpy(g_ids[i], buf);
		res = install_group(group, g_ids[i]);
		TEST_ASSERT_(res>=0, 1, "test group - install ok");
		
		if(!res)
		{
			// reset group
			set_send_delay(group, 0);
			revoke_delayed_messages(group);
			char msg[2];
			while (deliver_message(group, msg, 2));  // empty message queue
		}
		
		i++;
	}
	lgroup_destroy(group);
	
	// initialize global variables
	int load = 8;
	pthread_t *tid = calloc(load, sizeof(pthread_t));
	log = fopen("logs/test_stress.log", "w");
	
	// spawn stressers
	for (int i=0; i < load-1; i++)
	{
		int *arg = malloc(sizeof(int)); *arg = i;
		if((res = pthread_create(&(tid[i]), NULL, &stresser, arg)))
			printf("Can't create thread %d: %s.\n", i, strerror(res));
	}
	
	// spawn awaker
	if((res = pthread_create(&(tid[load-1]), NULL, &awaker, NULL)))
			printf("Can't create thread %d: %s.\n", load-1, strerror(res));
	
	sleep(duration);
	stop = 1;
	
	// join threads
	for (int i=0; i<load; i++)
		pthread_join(tid[i], NULL);
	
	fclose(log);
}
