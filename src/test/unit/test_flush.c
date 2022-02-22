#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


#define BUF_SIZE 10
static char **buf;
static int rd_idx = BUF_SIZE-1,  wr_idx = BUF_SIZE-1;

void test_flush(void)
{
	int res = -1;
	
	// installing test group
	struct lgroup_t *test_group = lgroup_init();
	res = install_group(test_group, "flush");
	TEST_ASSERT_(res!=-2, 0, "flush group availability");
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
	TEST_CHECK_(res==0, 0, "ioctl");
	
	// delayed writes
	int n = 10, len = 30;
	char *msg_wr;
	buf = calloc(BUF_SIZE, sizeof(char*));
	for(int i = 0; i<n; i++)
	{
		msg_wr = rand_string(len);
		res = publish_message(test_group, msg_wr);
		buf[wr_idx--] = msg_wr;
		TEST_CHECK_(res>0, 0, "delayed write");
	}
	
	// first early read should fail
	char *msg_rd = calloc(len, sizeof(char));
	res = deliver_message(test_group, msg_rd, len);
	TEST_CHECK_(res==0, 0, "first early read, exp: %d, got: %d", 0, res);
	
	// flush
	install_group(test_group, "flush");
	
	// early reads should work
	char *exp;
	for(int i = 0; i<n; i++)
	{
		exp = buf[rd_idx--];
		res = deliver_message(test_group, msg_rd, len);
		TEST_CHECK_(res==len, 0, "read flushed msg, exp: %d, got: %d", len, res);
		TEST_ASSERT_(!strcmp(msg_rd, exp), 0, "reading what was expected");
		free(exp);
	}
	res = deliver_message(test_group, msg_rd, len);
	TEST_CHECK_(res==0, 0, "extra read, exp: %d, got: %d", 0, res);
	
	lgroup_destroy(test_group);
}
