#include <sys/ioctl.h>

#define TEST_NO_MAIN
#include "acutest.h"

#include "groups.h"
#include "utils.h"


void test_install_group(void) {
	int res = -1;
	char exp_devname[100], exp_devpath[100], udev_pevpath[100];
	
	int fd_lkm_group = open("/dev/group0", O_RDWR);
	TEST_ASSERT_(fd_lkm_group!=-1, 1, "group0 installed");
	
	sprintf(exp_devname, "group%d", next_group()); // next available group
	
	// group first install
	char *group_id;
	struct group_t group = {.id = "\0", .devname = "\0"};
	do {
		group_id = rand_string(32);
		strncpy(group.id, group_id, 32);
		free(group_id);
		res = ioctl(fd_lkm_group, INSTALL_GROUP, &group);
		TEST_ASSERT_(errno != EDQUOT, 0, "group resource available");
		TEST_CHECK_(res >= 0, 0, "find unused group: %d", res);
	} while (res==0);
	TEST_CHECK_(res == 1 && !strcmp(group.devname, exp_devname), 0, 
			"first install, got: %s, exp: %s.",
			group.devname, exp_devname);
	
	sprintf(exp_devpath, "/dev/%s", exp_devname);
	sprintf(udev_pevpath, "/dev/synch/%s", exp_devname);
	
	// give udevd time to do its job
	msleep(UDEV_WAIT);
	
	// open /dev/synch/devname
	int fd_test_group = open(udev_pevpath, O_RDWR);
	TEST_ASSERT_(fd_test_group != -1, 1, "/dev/synch/%s: %s", group.devname, strerror(errno));
	
	// group second install
	res = ioctl(fd_lkm_group, INSTALL_GROUP, &group);
	TEST_CHECK_(res == 0 && !strcmp(group.devname, exp_devname), 0, "second install");

	close(fd_lkm_group);
	close(fd_test_group);
}
