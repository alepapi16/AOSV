#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lgroups.h"
#include "utils.h"


#define	INSTALL 1
#define WRITER 2
#define READER 3
#define AWAKER 4
#define SLEEPER 5
#define SHOW_HELP 6
#define SET_DELAY 7
#define REVOKE 8
#define SYSFS_READ 9
#define SYSFS_WRITE 10
#define READ 11
#define WRITE 12
#define SLEEP 13
#define AWAKE 14
#define ERROR -1


static void show_help(void)
{
	printf("Usage:\n\t./user.out <command> <group<N> [...]>\nCommands:\n"
				"\t./user.out install <group_id>\n"	
				"\t./user.out set_send_delay <group_id> <delay(ms)>\n"
				"\t./user.out revoke <group_id>\n"
				"\t./user.out sysfs_read <group_id> <attr_name>\n"
				"\tsudo ./user.out sysfs_write <group_id> <attr_name> <attr_value>\n"
				"\t./user.out read <group_id>\n"
				"\t./user.out write <group_id> <message>\n"
				"\t./user.out sleep <group_id>\n"
				"\t./user.out awake <group_id>\n"
				"\t./user.out reader <group_id> <sleep(us)>\n"
				"\t./user.out writer <group_id> <sleep(us)>\n"
				"\t./user.out sleeper <group_id> <sleep(us)> <#threads>\n"
				"\t./user.out awaker <group_id> <sleep(us)>\n");
}

static void* sleeper(void *arg)
{
	int i = *((int*)arg), wait = *((int*)arg+1);
	struct lgroup_t *lgroup = *((struct lgroup_t**)arg+2);
	char* buf = calloc(2*i, sizeof(char));
	
	// print format
	for(int j = 0; j<2*i; j+=2)
	{
		buf[j] = '\t';
		buf[j+1] = '\t';
	}

	printf("%sT%d: initialized.\n", buf, i);
	fflush(stdout);

	// sleeping loop
	while(1)
	{
		printf("%sT%d: to sleep..\n", buf, i);
		fflush(stdout);

		if(sleep_on_barrier(lgroup) < 0)
		{
			exit(EXIT_FAILURE);
		}

		printf("%sT%d: woken up!\n", buf, i);
		fflush(stdout);

		// the waiting time may be up to 4 times the wait parameter
		usleep(wait * (rand() % 4 + 1));
	}
}

int main(int argc, char** argv)
{	
	if (argc < 2)
	{
		printf("Not enough arguments. Type '%s -h' for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	// Parse command
	int cmd;
	{
	if (strcmp(argv[1], "install") == 0) cmd = INSTALL;
	else if (strcmp(argv[1], "set_send_delay") == 0) cmd = SET_DELAY;
	else if (strcmp(argv[1], "revoke") == 0) cmd = REVOKE;
	else if (strcmp(argv[1], "sysfs_read") == 0) cmd = SYSFS_READ;
	else if (strcmp(argv[1], "sysfs_write") == 0) cmd = SYSFS_WRITE;
	else if (strcmp(argv[1], "write") == 0) cmd = WRITE;
	else if (strcmp(argv[1], "read") == 0) cmd = READ;
	else if (strcmp(argv[1], "awake") == 0) cmd = AWAKE;
	else if (strcmp(argv[1], "sleep") == 0) cmd = SLEEP;
	else if (strcmp(argv[1], "writer") == 0) cmd = WRITER;
	else if (strcmp(argv[1], "reader") == 0) cmd = READER;
	else if (strcmp(argv[1], "awaker") == 0) cmd = AWAKER;
	else if (strcmp(argv[1], "sleeper") == 0) cmd = SLEEPER;
	else if (strcmp(argv[1], "-h") == 0){
		show_help();
		exit(EXIT_SUCCESS);
	}
	else {
		printf("Command not valid. Type '%s -h' for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	}
	
	if (argc < 3)
	{
		printf("Not enough arguments. Type '%s -h' for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	struct lgroup_t *lgroup = lgroup_init();
	
	if(cmd != INSTALL && install_group(lgroup, argv[2]) < 0)
	{
		goto error;
	}
	
	// execute command
	switch (cmd) 
	{
		case INSTALL:
		{
			int err = 0;
			
			if ((err = install_group(lgroup,argv[2]))<0)
			{
				printf("The group couldn't be installed.\n");
				goto error;
			}
			else if(!err)
			{
				printf("Group '%s' already existing.\n", argv[2]);
			}
			else
			{
				printf("Group '%s' installed.\n", argv[2]);
			}
			break;
		}
		
		case SET_DELAY:
		{
			unsigned int delay;

			if (argc != 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}

			delay = atoi(argv[3]);
			if (set_send_delay(lgroup, delay))
			{
				printf("Delay couldn't be set.\n");
				goto error;
			}
			printf("Delay set at %.2f s.\n", (float)delay/1000);
			break;
		}

		case REVOKE:
		{
			if (revoke_delayed_messages(lgroup))
			{
				printf("Couldn't revoke messages.\n");
				goto error;
			}
			printf("Revoked '%s' delayed messages.\n", argv[2]);
			break;
		}
		
		case SYSFS_WRITE:
		{
			if(argc < 5)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			if(!strcmp(argv[3], "max_message_size"))
			{
				if(set_max_message_size(lgroup, strtoul(argv[4], NULL, 10)) < 0)
				{
					printf("Couldn't set the attribute.\n");
					goto error;
				}
				printf("Attribute max_message_size set to %s.\n", argv[4]);
			}
			else if(!strcmp(argv[3], "max_storage_size"))
			{
				if(set_max_storage_size(lgroup, strtoul(argv[4], NULL, 10)) < 0)
				{
					printf("Couldn't set the attribute.\n");
					goto error;
				}
				printf("Attribute max_storage_size set to %s.\n", argv[4]);
			}
			else
			{
				printf("Wrong attribute name.\n");
				goto error;
			}
			
			break;
		}
		
		case SYSFS_READ:
		{
			if(argc < 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			unsigned long attr;
			
			if(!strcmp(argv[3], "max_message_size"))
			{
				if(get_max_message_size(lgroup, &attr) < 0)
				{
					printf("Couldn't read the attribute.\n");
					goto error;
				}
			}
			else if(!strcmp(argv[3], "max_storage_size"))
			{
				if(get_max_storage_size(lgroup, &attr) < 0)
				{
					printf("Couldn't read the attribute.\n");
					goto error;
				}
			}
			else
			{
				printf("Wrong attribute name.\n");
				goto error;
			}
			printf("%lu\n", attr);
			
			break;
		}
		
		case READER:
		{
			if (argc != 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			int wait = atoi(argv[3]);
			int res = 0, i=0, len = 100;
			char* buf;
			
			while(++i){ // neverending loop
				buf = calloc(len, sizeof(char));

				if((res = deliver_message(lgroup, buf, len)) < 0)
				{
					printf("Read operation failed.\n");
					goto error;
				}
				else if(res == 0){
					printf("Read%d: %s - size %d B.\n", i, buf, res);
					free(buf);
					usleep(1000000);  // 1 s
					continue;
				}
				printf("Read%d: %s - size %d B.\n", i, buf, res-1);
				free(buf);
				usleep(wait);
			}
			break;
		}
		
		case WRITER:
		{
			if (argc != 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			int wait = atoi(argv[3]);
			int res = 0, len = 0, i = 0;
			char* buf;

			while (++i)
			{
				len = rand() % 20 + 2; // at least 1 B, terminator excluded
				buf = rand_string(len);

				if ((res = publish_message(lgroup, buf)) < 0)
				{
					printf("Write operation failed.\n");
					goto error;
				}

				printf("Write%d: %s - size %d B.\n", i, buf, res);
				free(buf);
				usleep(wait);
			}
			
			break;
		}
		
		case SLEEPER:
		{
			pthread_t* tid;
			
			if (argc != 5)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			int wait = atoi(argv[3]);
			int load = atoi(argv[4]);
			
			tid = calloc(load, sizeof(pthread_t));

			for (int i=0; i < load; i++)
			{
				void *j = malloc(3 * sizeof(void *)); 
				*((int*)j) = i;
				*((int*)j+1) = wait;
				*((struct lgroup_t**)j+2) = lgroup;

				int err;
				if((err = pthread_create(&(tid[i]), NULL, &sleeper, j)))
				{
					printf("Can't create thread %d: %s.\n", i, strerror(err));
				}
			}

			for (int i=0; i<load; i++)
			{
				pthread_join(tid[i], NULL);
			}

			break;
		}
		
		case AWAKER:
		{
			if (argc != 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}
			
			int wait = atoi(argv[3]), i = 0, res = 0;
			
			printf("Awaker: initialized.\n");
			
			while(++i)
			{
				printf("%d: ", i);
				
				if((res = awake_barrier(lgroup)) < 0){
					goto error;
				}
				else if(res==1) printf("Alarm was already on!\n");
				else if(res==0) printf("Alarm was turned on!!\n");
				else printf("No one's sleeping... not turning on the alarm.\n");

				usleep(wait);
			}

			break;
		}
		
		case READ:
		{
			if (argc != 3)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}

			int res = 0, len = 100;
			char buf[len];

			if((res = deliver_message(lgroup, buf, len)) < 0) 
			{
				printf("Read operation failed.\n");
				goto error;
			}
			
			printf("Read: '%s' - size %d B.\n", buf, res);

			break;
		}
		
		case WRITE:
		{
			if (argc != 4)
			{
				printf("Wrong number of arguments. Type '%s -h' for help.\n", argv[0]);
				goto error;
			}

			if(publish_message(lgroup, argv[3]) < 0)
			{
				printf("Write operation failed.\n");
				goto error;
			}
			
			printf("Written '%s' to %s.\n", argv[3], argv[2]);

			break;
		}
		
		case SLEEP:
		{
			printf("Going to sleep..\n");
			fflush(stdout);
			
			if(sleep_on_barrier(lgroup) < 0)
			{
				printf("Sleep operation failed.\n");
				goto error;
			}
			
			printf("Woken up!\n");
			fflush(stdout);
			
			break;
		}
		
		case AWAKE:
		{
			int res = 0;

			if((res = awake_barrier(lgroup)) < 0)
			{
				printf("Awake operation failed.\n");
				goto error;
			}
			else if(res==1) printf("Alarm was already on!\n");
			else if(res==0) printf("Alarm was turned on!!\n");
			else printf("No one's sleeping... not turning on the alarm.\n");

			break;
		}
	}
	
	lgroup_destroy(lgroup);
	exit(EXIT_SUCCESS);
	
error:
	lgroup_destroy(lgroup);
	exit(EXIT_FAILURE);
}
