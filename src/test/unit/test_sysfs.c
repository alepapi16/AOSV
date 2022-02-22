#define TEST_NO_MAIN
#include "acutest.h"

#include "utils.h"
#include "lgroups.h"


void test_sysfs(void)
{
	TEST_ASSERT_(geteuid()==0, 0, "root privileges");
	
	unsigned long new_message_size = 200, new_storage_size = 2100;
	unsigned long old_message_size, old_storage_size;
	
	// installing test group
	struct lgroup_t *test_group = lgroup_init();
	
	int res = install_group(test_group, "sysfs");
	TEST_ASSERT_(res!=-2, 0, "sysfs group availability");
	TEST_ASSERT_(res>=0, 0, "sysfs group install");
	
	// group already existed
	if(!res)
	{
		// reset group
		set_send_delay(test_group, 0);
		revoke_delayed_messages(test_group);
		char msg[2];
		while (deliver_message(test_group, msg, 2));  // empty message queue
	}
	
	// read max_message_size
	res = get_max_message_size(test_group, &old_message_size);
	TEST_ASSERT_(!res, 0,"read max_message_size: %s", strerror(errno));
	TEST_ASSERT_(old_message_size == 100, 0, "max_message_size: read %lu, expected %s.", old_message_size, "100");
	
	// change max_message_size
	res = set_max_message_size(test_group, new_message_size);
	TEST_ASSERT_(!res, 0, "write max_message_size: %s", strerror(errno));
	
	// read max_storage_size
	res = get_max_storage_size(test_group, &old_storage_size);
	TEST_ASSERT_(!res, 0,"read max_storage_size: %s", strerror(errno));
	TEST_ASSERT_(old_storage_size == 10000, 0, "max_storage_size: read %lu, expected %s.", old_storage_size, "10000");
	
	// change max_storage_size
	res = set_max_storage_size(test_group, new_storage_size);
	TEST_ASSERT_(!res, 0, "write max_storage_size: %s", strerror(errno));
	
	// violate max_message_size
	char *buf = rand_string(new_message_size+1);
	res = publish_message(test_group, buf);
	free(buf);
	TEST_ASSERT_(res==-2, 0, "violating max_message_size");
	
	// don't violate max_message_size
	buf = rand_string(new_message_size);
	res = publish_message(test_group, buf);
	TEST_ASSERT_(res==new_message_size, 0, "don't violate max_message_size: %s", strerror(errno));
	
	// saturate max storage size
	for(int i=1; i<new_storage_size/new_message_size; i++)
	{
		res = publish_message(test_group, buf);
		TEST_ASSERT_(res==new_message_size, 0, 
				"saturating group: msg%d, written: %d, %s", 
				i, res, strerror(errno));
	}
	
	// violate max_storage_size
	res = publish_message(test_group, buf);
	free(buf);
	TEST_ASSERT_(res==-2, 0, "violate max_message_size: %s", strerror(errno));
	
	// restore old size parameters
	set_max_message_size(test_group, old_message_size);
	set_max_storage_size(test_group, old_storage_size);
	
	lgroup_destroy(test_group);
}
