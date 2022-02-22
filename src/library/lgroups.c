#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "groups.h"
#include "lgroups.h"


// time to wait after installing a new group to let
// udevd applying the rules for symlink creation
#define UDEV_SLEEP 200000 //ms

// udev trials before returning errors to users
#define UDEV_TRIALS 3

char *udev_folder = "/dev/synch/";

// file descriptor for the initial LKM group
int fd_group0 = -1;


struct lgroup_t {
	struct group_t __group;
	int __fd;
};

static const struct group_t EmptyGroup;

struct lgroup_t* lgroup_init()
{
	struct lgroup_t *lgroup = malloc(sizeof(struct lgroup_t));
	lgroup->__group = EmptyGroup;
	lgroup->__fd = -1;
	return lgroup;
}

void lgroup_destroy(struct lgroup_t *lgroup)
{
	if(!lgroup)
		return;
	
	if(lgroup->__fd != -1)
		close(lgroup->__fd);
	
	free(lgroup);
}


// -------------- IOCTL-RELATED OPERATIONS -------------- //

int install_group(struct lgroup_t *lgroup, char *group_id)
{
	int res;
	
	// check group0
	if(fd_group0 == -1)  // not yet opened?
	{
		char pathname[100];
		sprintf(pathname, "%s%s", udev_folder, "group0");
		// try opening group0
		if((fd_group0 = open(pathname, O_RDWR))==-1)  // won't open
		{
			//perror("lgroups.install_group: open group0"); 
			return -1;
		}
	}
	
	// lgroup not finalized yet
	if(lgroup->__fd != -1)
	{
		res = close(lgroup->__fd);
		lgroup->__fd = -1;
	}
	
	// lgroup not installed yet
	if(lgroup->__fd == -1)
	{
		strncpy(lgroup->__group.id, group_id, 32);
		
		// INSTALL_GROUP ioctl syscall
		if((res = ioctl(fd_group0, INSTALL_GROUP, &lgroup->__group)) < 0)
		{
			if(errno == EDQUOT)
				return -2;
			
			//perror("lgroups.install_group: INSTALL_GROUP ioctl:");
			return -3;
		}
		
		// open device file
		int n = strlen(udev_folder) + sizeof(lgroup->__group.devname), opened = 0;
		char pathname[n]; 
		sprintf(pathname, "%s%s", udev_folder, lgroup->__group.devname);
		if((lgroup->__fd = open(pathname, O_RDWR))!=-1)
		{
			opened = 1;
		}
		else
		{
			// delayed trials
			for(int m = 1; !opened && m < UDEV_TRIALS-1; m++)
			{
				// give udev time to do its job
				usleep(UDEV_SLEEP);
				if((lgroup->__fd = open(pathname, O_RDWR))!=-1)
					opened = 1;
			}

			// return error if necessary
			if(!opened)
			{
				//fprintf(stderr, "lgroups.install_group: opening %s: %s.\n", pathname, strerror(errno));
				return -4-res;
			}
		}
		
	}
	
	return res;
}

int set_send_delay(struct lgroup_t *lgroup, unsigned int delay)
{
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// SET_SEND_DELAY ioctl syscall
	if (ioctl(lgroup->__fd, SET_SEND_DELAY, &delay))
	{
		//fprintf(stderr, "lgroups.set_send_delay.ioctl : %s.\n", strerror(errno));
		return -2;
	}
	
	return 0;
}

int revoke_delayed_messages(struct lgroup_t *lgroup)
{
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// REVOKE_DELAYED_MESSAGES ioctl syscall
	if (ioctl(lgroup->__fd, REVOKE_DELAYED_MESSAGES)) {
		//fprintf(stderr, "lgroups.revoke_delayed_messages.ioctl : %s.\n", strerror(errno));
		return -2;
	}
	
	return 0;
}


// -------------- READ/WRITE OPERATIONS -------------- //

int publish_message(struct lgroup_t *lgroup, char *msg)
{
	int res;
	
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// write syscall
	if((res = write(lgroup->__fd, msg, strlen(msg)+1)) < 0)
	{
		//fprintf(stderr, "lgroups.publish_message.write : %s.\n", strerror(errno));
		return -2;
	}
	
	return res;
}

int deliver_message(struct lgroup_t *lgroup, char *buf, unsigned long size)
{
	int res;
	
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// read syscall
	buf[0] = '\0';
	if((res = read(lgroup->__fd, buf, size)) < 0)
	{
		//fprintf(stderr, "lgroups.deliver_message.read : %s.\n", strerror(errno));
		return -2;
	}
	
	return res;
}


// -------------- BARRIER OPERATIONS -------------- //

int sleep_on_barrier(struct lgroup_t *lgroup)
{
	int res;
	
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// SLEEP_ON_BARRIER ioctl syscall
	if((res = ioctl(lgroup->__fd, SLEEP_ON_BARRIER)) < 0)
	{
		//fprintf(stderr, "lgroups.sleep_on_barrier.ioctl : %s.\n", strerror(errno));
		return -2;
	}
	
	return res;
}

int awake_barrier(struct lgroup_t *lgroup)
{
	int res;
	
	// check if group was correctly installed
	if(!lgroup->__fd == -1)
	{
		return -1;
	}
	
	// AWAKE_BARRIER ioctl syscall
	if((res = ioctl(lgroup->__fd, AWAKE_BARRIER)) < 0)
	{
		//fprintf(stderr, "lgroups.awake_barrier.ioctl : %s.\n", strerror(errno));
		return -2;
	}
	
	return res;
}


// -------------- SYSFS-RELATED OPERATIONS -------------- //

int get_max_message_size(struct lgroup_t *lgroup, unsigned long *size)
{
	// sysfs open
	char sys_path[100];
	int sys_fd = 0;
	snprintf(sys_path, 100, "/sys/class/groups/%s/max_message_size", lgroup->__group.devname);
	if ((sys_fd = open(sys_path, O_RDONLY))==-1)
	{
		//fprintf(stderr, "lgroups.get_max_message_size: sysfs.open '%s': %s.\n", sys_path, strerror(errno));
		return -2;
	}
	
	// sysfs read
	int err;
	char buf[100];
	if((err = read(sys_fd, buf, 100)) < 0)
	{
		//fprintf(stderr, "lgroups.get_max_message_size: sysfs.read '%s': %s.\n", sys_path, strerror(errno));
		return -3;
	}
	close(sys_fd);
	
	*size = strtoul(buf, NULL, 10);
	
	return 0;
}

int set_max_message_size(struct lgroup_t *lgroup, unsigned long size)
{
	// non root?
	if(geteuid()!=0)
	{
		//fprintf(stderr, "lgroups.set_max_message_size: run as superuser.\n");
		return -1;
	}
	
	// sysfs open
	char sys_path[100];
	int sys_fd = 0;
	snprintf(sys_path, 100, "/sys/class/groups/%s/max_message_size", lgroup->__group.devname);
	if ((sys_fd = open(sys_path, O_WRONLY))==-1) {
		//fprintf(stderr, "lgroups.set_max_message_size: sysfs.open '%s': %s.\n", sys_path, strerror(errno));
		return -3;
	}
	
	// sysfs write
	char buf[100];
	sprintf(buf, "%lu", size);
	if(!write(sys_fd, buf, strlen(buf)+1))
	{
		//fprintf(stderr, "lgroups.set_max_message_size: sysfs.write '%s': %s.\n", sys_path, strerror(errno));
		return -4;
	}
	close(sys_fd);
	
	return 0;
}

int get_max_storage_size(struct lgroup_t *lgroup, unsigned long *size)
{
	// sysfs open
	char sys_path[100];
	int sys_fd = 0;
	snprintf(sys_path, 100, "/sys/class/groups/%s/max_storage_size", lgroup->__group.devname);
	if ((sys_fd = open(sys_path, O_RDONLY))==-1)
	{
		//fprintf(stderr, "lgroups.get_max_storage_size: sysfs.open '%s': %s.\n", sys_path, strerror(errno));
		return -2;
	}
	
	// sysfs read
	int err;
	char buf[100];
	if((err = read(sys_fd, buf, 100)) < 0)
	{
		//fprintf(stderr, "lgroups.get_max_storage_size: sysfs.read '%s': %s.\n", sys_path, strerror(errno));
		return -3;
	}
	close(sys_fd);
	
	*size = strtoul(buf, NULL, 10);
	
	return 0;
}

int set_max_storage_size(struct lgroup_t *lgroup, unsigned long size)
{
	// non root?
	if(geteuid()!=0)
	{
		//fprintf(stderr, "lgroups.set_max_storage_size: run as superuser.\n");
		return -1;
	}
	
	// sysfs open
	char sys_path[100];
	int sys_fd = 0;
	snprintf(sys_path, 100, "/sys/class/groups/%s/max_storage_size", lgroup->__group.devname);
	if ((sys_fd = open(sys_path, O_WRONLY))==-1) {
		//fprintf(stderr, "lgroups.set_max_storage_size: sysfs.open '%s': %s.\n", sys_path, strerror(errno));
		return -3;
	}
	
	// sysfs write
	char buf[100];
	sprintf(buf, "%lu", size);
	if(!write(sys_fd, buf, strlen(buf)+1))
	{
		//fprintf(stderr, "lgroups.set_max_storage_size: sysfs.write '%s': %s.\n", sys_path, strerror(errno));
		return -4;
	}
	close(sys_fd);
	
	return 0;
}
