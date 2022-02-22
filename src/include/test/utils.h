
#ifndef UTILS_H
#define UTILS_H

#define UDEV_WAIT 200  // ms
#define TEST_DELAY 700  // ms
#define TEST_EPSILON 200  // ms

/**
 * Allocates and returns a pointer to a random string 
 * of size @param size ('\0' included).
 * @param size
 * @return 
 */
char *rand_string(size_t size);

/**
 * Returns the number of the next available group
 * number in '/dev'
 * @return 
 */
int next_group();

/**
 * Sleeps msecs milliseconds.
 * @param msecs
 * @return 
 */
int msleep(long msecs);

#endif /* utils.h */
