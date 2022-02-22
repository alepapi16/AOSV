#include "acutest.h"

void test_install_group(void);
void test_rw_fifo(void);
void test_delay(void);
void test_flush(void);
void test_sysfs(void);
void test_max_install(void);
void test_barrier(void);
void test_revoke(void);
void test_stress(void);

TEST_LIST = {
	{"install group", test_install_group},
	{"r/w FIFO order", test_rw_fifo},
	{"delayed operating mode", test_delay},
	{"sysfs attributes", test_sysfs},
	{"barrier", test_barrier},
	{"revoke delayed messages", test_revoke},
	{"flush", test_flush},
	{"stress 10s", test_stress},
	{"max installs", test_max_install},
	{0}
};
