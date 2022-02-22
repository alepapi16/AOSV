#include <sys/ioctl.h>
#include <math.h>

#define TEST_NO_MAIN
#include "acutest.h"

#include "groups.h"
#include "utils.h"


void test_max_install(void)
{
	int res = -1, fd_lkm_group = open("/dev/group0", O_RDWR);
	TEST_ASSERT_(fd_lkm_group!=-1, 1, "group0 installed");
	
	unsigned long groups = (unsigned long) next_group();
	struct group_t group;
	char exp_devname[32], *buf;
	
	while(groups < 2000)  // 2000 = kernel-level information
	{
		// install new group
		buf = rand_string(32);
		strcpy(group.id, buf);
		free(buf);
		res = ioctl(fd_lkm_group, INSTALL_GROUP, &group);
		sprintf(exp_devname, "group%lu", groups++);
		TEST_ASSERT_(!strcmp(group.devname, exp_devname), 0,
				"expected: %s, got: %s, ioctl res = %d.",
				exp_devname, group.devname, res);
	}
	
	buf = rand_string(32);
	strcpy(group.id, buf);
	free(buf);
	res = ioctl(fd_lkm_group, INSTALL_GROUP, &group);
	TEST_CHECK_(res == -1 && errno == EDQUOT, 0, 
			"last install: new devname: %s, ioctl res = %d.", group.devname, res);
	
	close(fd_lkm_group);
}
