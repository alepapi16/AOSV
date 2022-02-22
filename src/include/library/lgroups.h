
#ifndef	_LGROUPS_H
#define	_LGROUPS_H


struct lgroup_t;


/**
 * Initializes and returns an lgroup to be passed 
 * as parameter for future API function calls.
 * 
 * @return lgroup_t*
 */
struct lgroup_t* lgroup_init(void);

/**
 * Finalizes the lgroup.
 * 
 * @param lgroup
 */
void lgroup_destroy(struct lgroup_t *lgroup);


// --------------  MESSAGING-RELATED OPERATIONS -------------- //

/** 
 * Posts a message to the group-shared queue.
 * 
 * @param lgroup
 * @param msg
 * @return 
 *		amount of bytes written
 *		-1: group is not installed
 *		-2: write fail, check errno
 */
int publish_message(struct lgroup_t *lgroup, char *msg);

/** 
 * Delivers a message from the group-shared queue.
 * 
 * @param lgroup
 * @param buf
 * @param size
 * @return 
 *		amount of bytes read
 *		-1: group is not installed
 *		-2: read fail, check errno
 */
int deliver_message(struct lgroup_t *lgroup, char *buf, unsigned long size);

/**
 * Sets group's delay.
 * 
 * @param lgroup, previously installed
 * @param delay, in milliseconds
 * @return 
 *		0: success
 *		-1: group is not installed
 *		-2: ioctl fail, check errno
 */
int set_send_delay(struct lgroup_t *lgroup, unsigned int delay);

/**
 * Revokes delayed messages.
 * 
 * @param lgroup
 * @return 
 *		0: success
 *		-1: group is not installed
 *		-2: ioctl fail, check errno
 */
int revoke_delayed_messages(struct lgroup_t *lgroup);


// -------------- BARRIER OPERATIONS -------------- //

/**
 * Goes to sleep on a group's barrier.
 * 
 * The function returns after the first 
 * awake_barrier on the same installed group.
 * 
 * @param lgroup, previously installed
 * @return 
 *		0: success
 *		-1: group is not installed
 *		-2: sleep fail, check errno
 */
int sleep_on_barrier(struct lgroup_t *lgroup);

/**
 * Awakes the group's barrier.
 * 
 * @param lgroup, previously installed
 * @return
 *		2: no one is sleeping, alarm is not set
 *		1: alarm was already on
 *		0: alarm is set
 *		-1: group is not installed
 *		-2: ioctl fail, check errno
 */
int awake_barrier(struct lgroup_t *lgroup);


// --------------  CONTROL OPERATIONS -------------- //

/**
 * Installs a new group into the system.
 * 
 * @param lgroup, previously initialized
 * @param group_id
 * @return
 *		1: group is correctly installed
 *		0: group was already installed
 *		-1: module not installed
 *		-2: groups resource unavailable
 *		-3: INSTALL_GROUP ioctl failed
 *		-4: group was already installed, but didn't open
 *		-5: group is correctly installed, but didn't open
 */
int install_group(struct lgroup_t *group, char *group_id);

/**
 * Reads current group's max_message_size.
 * 
 * @param lgroup, previously installed
 * @param size
 * @return 
 *		0: success
 *		-1: group is not installed
 *		-2: sysfs open fail
 *		-3: sysfs read fail
 */
int get_max_message_size(struct lgroup_t *lgroup, unsigned long *size);

/**
 * Writes the current group's max_message_size.
 * 
 * @param lgroup, previously installed
 * @param size
 * @return 
 *		0: success
 *		-1: missing superuser privileges
 *		-2: group is not installed
 *		-3: sysfs open fail
 *		-4: sysfs write fail
 */
int set_max_message_size(struct lgroup_t *lgroup, unsigned long size);

/**
 * Reads current group's max_storage_size.
 * 
 * @param lgroup, previously installed
 * @param size
 * @return 
 *		0: success
 *		-1: group is not installed
 *		-2: sysfs open fail
 *		-3: sysfs read fail
 */
int get_max_storage_size(struct lgroup_t *lgroup, unsigned long *size);

/**
 * Writes the current group's max_storage_size.
 * 
 * @param lgroup, previously installed
 * @param size
 * @return 
 *		0: success
 *		-1: missing superuser privileges
 *		-2: group is not installed
 *		-3: sysfs open fail
 *		-4: sysfs write fail
 */
int set_max_storage_size(struct lgroup_t *lgroup, unsigned long size);

#endif /* lgroups.h */
