
#ifndef _GROUPS_H
#define _GROUPS_H


// userspace identification information for a group
struct group_t
{
	char id[32];
	
	// fmt="group%lu" -> size = 26: 5(group)+20(ULONG_MAX)+1(\0)
	char devname[26];
};


// unused magic number
// check 'https://www.kernel.org/doc/Documentation/ioctl/ioctl-number.txt'
#define _IOC_MAGIC 'x'

#define SLEEP_ON_BARRIER				_IO(_IOC_MAGIC, 1)
#define AWAKE_BARRIER					_IO(_IOC_MAGIC, 2)
#define INSTALL_GROUP					_IOWR(_IOC_MAGIC, 3, struct group_t*)
#define SET_SEND_DELAY					_IOW(_IOC_MAGIC, 4, unsigned long*)
#define REVOKE_DELAYED_MESSAGES			_IO(_IOC_MAGIC, 5)

#define _IOC_MAX 5

#endif /* groups.h */
