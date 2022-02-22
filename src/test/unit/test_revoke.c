#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


void test_revoke(void)
{
	int res = -1;
	
	// installing test group
	struct lgroup_t *test_group = lgroup_init();
	res = install_group(test_group, "revoke");
	TEST_ASSERT_(res!=-2, 0, "revoke group availability");
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
	int n = 10, len = 30;
	char *msg_wr;
	for(int i = 0; i<n; i++)
	{
		msg_wr = rand_string(len);
		res = publish_message(test_group, msg_wr);
		free(msg_wr);
		TEST_CHECK_(res>0, 0, "delayed write");
	}
	
	// first early read should fail
	char *msg_rd = calloc(len, sizeof(char));
	res = deliver_message(test_group, msg_rd, len);
	TEST_ASSERT_(res==0, 0, "first early read, exp: %d, got: %d", 0, res);
	
	msleep(TEST_DELAY-TEST_EPSILON);  // wait
	
	// second early read should fail too
	res = deliver_message(test_group, msg_rd, len);
	TEST_ASSERT_(res==0, 0, "second early read, exp: %d, got: %d", 0, res);
	
	// revoke delayed
	revoke_delayed_messages(test_group);
	
	msleep(2*TEST_EPSILON);  // wait
	
	// delayed read should not work either
	res = deliver_message(test_group, msg_rd, len);
	TEST_ASSERT_(res==0, 0, "delayed read, exp: %d, got: %d", 0, res);
	
	lgroup_destroy(test_group);
}
