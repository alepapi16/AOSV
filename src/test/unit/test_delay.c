#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


void test_delay(void)
{
	int res = -1;
	
	// installing test group
	struct lgroup_t *test_group = lgroup_init();
	
	res = install_group(test_group, "delay");
	TEST_ASSERT_(res>=0, 1, "test group - install ok");
	
	// group already existed
	if(!res)
	{
		// reset group
		revoke_delayed_messages(test_group);
		char msg[2];
		while (deliver_message(test_group, msg, 2));  // empty message queue
	}
	
	// set delay
	unsigned int delay = TEST_DELAY;
	res = set_send_delay(test_group, delay);
	TEST_ASSERT_(res==0, 0, "ioctl");
	
	// delayed write
	char *msg_wr = rand_string(30);
	res = publish_message(test_group, msg_wr);
	free(msg_wr);
	TEST_ASSERT_(res==30, 0, "delayed write");
	
	// first early read should fail
	char *msg_rd = calloc(30, sizeof(char));
	res = deliver_message(test_group, msg_rd, 30);
	TEST_ASSERT_(res==0, 0, "first early read, exp: %d, got: %d", 0, res);

	msleep(TEST_DELAY - TEST_EPSILON);  // wait

	// second early read should fail
	res = deliver_message(test_group, msg_rd, 30);
	TEST_ASSERT_(res==0, 0, "second early read, exp: %d, got: %d", 0, res);
	
	msleep(2 * TEST_EPSILON);  // wait
	
	// delayed read should work
	res = deliver_message(test_group, msg_rd, 30);
	TEST_ASSERT_(res==30, 0, "delayed read, exp: %d, got: %d", 30, res);
	
	lgroup_destroy(test_group);
}
