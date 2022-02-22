#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/hashtable.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/xxhash.h>

#include "groups.h"


// -------------- TYPES DEFINITION -------------- //

// published unit of information
struct msg_t {
	struct list_head node;  // node within gdev->published_list
	char *text;
	size_t size;
};

// delayed unit of information
struct delayed_msg_t {
	struct group_dev_t *gdev;
	struct list_head node;  // node within gdev->delayed_list
	
	struct msg_t *msg;  // the message to be published
	int is_revoked;
	int is_flushed;
	struct timer_list timer;  // linux kernel timer
};

// kernel level representation of a group
struct group_dev_t {

	// read/write information
	unsigned long size;  // of both published and delayed messages
	unsigned long max_msg_size;
	unsigned long max_strg_size;
	spinlock_t size_lock;
	
	struct list_head published_list;
	spinlock_t published_list_lock;
	
	atomic_t delay;  // ms
	
	struct list_head delayed_list;
	spinlock_t delayed_list_lock;

	
	// barrier information
	int sleepers;
	int wakeup;
	spinlock_t barrier_lock;
	wait_queue_head_t sleeping_wq;
	
	
	// device, group and sysfs information
	struct device* dev;
	struct cdev cdev;
	struct group_t *group;
	struct hlist_node hnode;  // groups_htbl hnode
	
	struct kobj_attribute max_msg_size_attr;
	struct kobj_attribute max_strg_size_attr;
};


// -------------- LOOKASIDE CACHES -------------- //

struct kmem_cache *msg_cache;
struct kmem_cache *delayed_msg_cache;
struct kmem_cache *group_dev_cache;


// -------------- SUPPORTED FOPS SIGNATURES -------------- //

int group_open(struct inode *inode, struct file *filp);
int group_release(struct inode *inode, struct file *filp);
ssize_t group_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t group_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
long group_ioctl(struct file* filp, unsigned int cmd, unsigned long arg);
int group_flush(struct file* filp, fl_owner_t id);

struct file_operations group_fops = {
	.owner = THIS_MODULE,
	.open = group_open,
	.release = group_release,
	.read = group_read,
	.write = group_write,
	.unlocked_ioctl = group_ioctl,
	.flush = group_flush
};


// ------------- GLOBAL VARIABLES -------------------- //

DECLARE_HASHTABLE(groups_htbl, 8);  // hashtable for group_dev_t(s) management
static spinlock_t htbl_lock;

static struct class *class = NULL;
static int major;
static unsigned long range = 2000;
static unsigned long groups = 0;  // amount of currently active groups
static spinlock_t install_lock;


// ------------- GARBAGE COLLECTOR -------------------- //

static void published_work_fn(struct work_struct *work);
DECLARE_WORK(published_work, published_work_fn);

static void delayed_work_fn(struct work_struct *work);
DECLARE_WORK(delayed_work, delayed_work_fn);

static struct workqueue_struct *groups_wq;

// Published messages, deallocations deferred work
static struct list_head *active_published_list;
static struct list_head *next_published_list;
spinlock_t published_work_lock;

static void published_work_fn(struct work_struct *work)
{	
	// the work_struct handler is being executed
	struct msg_t *msg;
	
	// safe: the list is surely non-empty (struct_work was queued)
	struct list_head *aux = next_published_list;
	
	// atomically swap active and next lists
	spin_lock_bh(&published_work_lock);
	next_published_list = active_published_list;
	spin_unlock_bh(&published_work_lock);
	
	active_published_list = aux;
	
	// remove all published messages
	list_for_each(aux, active_published_list)
	{
		msg = container_of(aux, struct msg_t, node);
		aux = aux->prev;
		list_del(&msg->node);
		vfree(msg->text);
		kmem_cache_free(msg_cache, msg);
	}
}

// Delayed messages, deallocations deferred work
static struct list_head *active_delayed_list;
static struct list_head *next_delayed_list;
spinlock_t delayed_work_lock;

static void delayed_work_fn(struct work_struct *work)
{
	// the work_struct handler is being executed
	struct delayed_msg_t *delayed_msg;
	
	// safe: the list is surely non-empty (struct_work was queued)
	struct list_head *aux = next_delayed_list;
	
	// atomically swap active and next lists
	spin_lock_bh(&delayed_work_lock);
	next_delayed_list = active_delayed_list;
	spin_unlock_bh(&delayed_work_lock);
	
	active_delayed_list = aux;
	
	// remove all delayed messages
	list_for_each(aux, active_delayed_list)
	{
		delayed_msg = container_of(aux, struct delayed_msg_t, node);
		aux = aux->prev;
		list_del(&delayed_msg->node);
		
		if(delayed_msg->is_revoked)
		{
			// remove the embedded "published message"
			vfree(delayed_msg->msg->text);
			kmem_cache_free(msg_cache, delayed_msg->msg);
		}
		
		kmem_cache_free(delayed_msg_cache, delayed_msg);
	}
}


// ------------- SYSFS FUNCTIONS ----------------- //

ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, 
		char *buf) {
	int res = 0;
	
	if(!strcmp(attr->attr.name, "max_message_size"))
	{
		struct group_dev_t* gdev = container_of(attr, struct group_dev_t, max_msg_size_attr);
		spin_lock(&gdev->size_lock);
		res = sprintf(buf, "%lu", gdev->max_msg_size)+1;
		spin_unlock(&gdev->size_lock);
	}
	else if(!strcmp(attr->attr.name, "max_storage_size"))
	{
		struct group_dev_t* gdev = container_of(attr, struct group_dev_t, max_strg_size_attr);
		spin_lock(&gdev->size_lock);
		res = sprintf(buf, "%lu", gdev->max_strg_size)+1;
		spin_unlock(&gdev->size_lock);
	}
	
	return res;
}

ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, 
		const char *buf, size_t count) {
	
	if(!strcmp(attr->attr.name, "max_message_size"))
	{
		unsigned long tmp;
		struct group_dev_t* gdev = container_of(attr, struct group_dev_t, max_msg_size_attr);
		if(sscanf(buf, "%lu", &tmp))
		{
			spin_lock(&gdev->size_lock);
			gdev->max_msg_size = tmp;
			spin_unlock(&gdev->size_lock);
		}
	}
	else if(!strcmp(attr->attr.name, "max_storage_size"))
	{
		unsigned long tmp;
		struct group_dev_t* gdev = container_of(attr, struct group_dev_t, max_strg_size_attr);
		if(sscanf(buf, "%lu", &tmp))
		{
			spin_lock(&gdev->size_lock);
			gdev->max_strg_size = tmp;
			spin_unlock(&gdev->size_lock);
		}
	}
	
	return count;
}


// ------------- AUXILIARY FUNCTIONS ----------------- //

static int gdev_init(struct group_dev_t** gdev_pp, struct group_t *group, dev_t dev, uint64_t hkey)
{
	struct kobj_attribute msg_kobj_attr = __ATTR(max_message_size, S_IRUGO | S_IWUSR, sysfs_show, sysfs_store);
	struct kobj_attribute strg_kobj_attr = __ATTR(max_storage_size, S_IRUGO | S_IWUSR, sysfs_show, sysfs_store);
	char devname[32];
	struct group_dev_t *gdev;
	int err = 0;
	
	// allocate new group_dev_t
	if (!(gdev = kmem_cache_alloc(group_dev_cache, GFP_KERNEL)))
	{
		printk(KERN_ERR "%s/group%ld: could not initialize a new group_dev_t.\n", KBUILD_MODNAME, groups);
		return -ENOMEM;
	}
	memset(gdev, 0, sizeof(struct group_dev_t));

	// create struct device
	snprintf(devname, 32, "group%ld", groups);
	gdev->dev = device_create(class, NULL, dev, NULL, devname);
	if (IS_ERR(gdev->dev))
	{
		err = PTR_ERR(gdev->dev);
		printk(KERN_ERR "%s/group%ld: error creating device, major=%d, minor=%ld.\n",
			KBUILD_MODNAME, groups, major, groups);
		goto failed_devicecreate;
	}
	
	// create new char device
	cdev_init(&gdev->cdev, &group_fops);
	gdev->cdev.owner = THIS_MODULE;
	if ((err = cdev_add(&gdev->cdev, dev, 1)))
	{
		printk(KERN_ERR "%s/group%ld: error adding group_dev_t cdev.\n", KBUILD_MODNAME, groups);
		goto failed_addcdev;
	}
	
	// expose sysfs attributes
	gdev->max_msg_size_attr = msg_kobj_attr;
	if ((err = sysfs_create_file(&gdev->dev->kobj, &gdev->max_msg_size_attr.attr))) 
	{
		printk(KERN_ERR "%s/group%ld: error exposing max_message_size in sysfs.\n", KBUILD_MODNAME, groups);
		goto failed_sysfs_msg;
	}
	gdev->max_strg_size_attr = strg_kobj_attr;
	if ((err = sysfs_create_file(&gdev->dev->kobj, &gdev->max_strg_size_attr.attr))) 
	{
		printk(KERN_ERR "%s/group%ld: error exposing max_storage_size in sysfs.\n", KBUILD_MODNAME, groups);
		goto failed_sysfs_storage;
	}

	// initialize group_t
	strncpy(group->devname, devname, 32);
	gdev->group = group;

	// initialize r/w parameters
	spin_lock_init(&gdev->size_lock);
	gdev->size = 0;
	gdev->max_msg_size = 100;
	gdev->max_strg_size = 10000;

	spin_lock_init(&gdev->published_list_lock);
	INIT_LIST_HEAD(&gdev->published_list);
	
	// delayed write
	atomic_set(&gdev->delay, 0);
	
	spin_lock_init(&gdev->delayed_list_lock);
	INIT_LIST_HEAD(&gdev->delayed_list);

	// initialize barrier parameters
	spin_lock_init(&gdev->barrier_lock);
	gdev->sleepers = 0;
	gdev->wakeup = 0;
	init_waitqueue_head(&gdev->sleeping_wq);
	
	// atomically publish gdev into groups_htbl
	spin_lock(&htbl_lock);
	hash_add(groups_htbl, &gdev->hnode, hkey);
	spin_unlock(&htbl_lock);

	// propagate gdev address
	if(gdev_pp) *gdev_pp = gdev;

	// publish dev in kernel ring buffer
	printk(KERN_INFO "%s: <id=%s>, device registered with major=%d, minor=%d.\n",
		KBUILD_MODNAME, gdev->group->id, MAJOR(dev), MINOR(dev));

	return 0;

failed_sysfs_storage:
	sysfs_remove_file(&gdev->dev->kobj, &gdev->max_msg_size_attr.attr);
failed_sysfs_msg:
	cdev_del(&gdev->cdev);
failed_addcdev:
	device_destroy(class, dev);
failed_devicecreate:
	kmem_cache_free(group_dev_cache, gdev);
	return err;
}

void timer_callback(struct timer_list *t)
{
	struct delayed_msg_t *delayed_msg = from_timer(delayed_msg, t, timer);
	struct group_dev_t* gdev = delayed_msg->gdev;
	
	// enqueue msg
	spin_lock_bh(&gdev->published_list_lock);
	list_add_tail(&delayed_msg->msg->node, &gdev->published_list);
	spin_unlock_bh(&gdev->published_list_lock);

	// delete delayed_msg
	spin_lock_bh(&gdev->delayed_list_lock);
	// being flushed?
	if(delayed_msg->is_flushed)
	{
		// don't unlink nor deallocate the node, return
		spin_unlock_bh(&gdev->delayed_list_lock);
		return;
	}
	else
	{
		// unlink and deallocate
		list_del(&delayed_msg->node);
		spin_unlock_bh(&gdev->delayed_list_lock);

		kmem_cache_free(delayed_msg_cache, delayed_msg);
	}
}


// ------------- FILE OPERATIONS --------------------- //

int group_open(struct inode *inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev, struct group_dev_t, cdev);
	return 0;
}

int group_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t group_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos) {
	struct group_dev_t *gdev = filp->private_data;
	struct msg_t* msg;
	size_t n;
	
	// atomically dequeue message
	spin_lock_bh(&gdev->published_list_lock);
	if (list_empty(&gdev->published_list))
	{
		spin_unlock_bh(&gdev->published_list_lock);
		return 0; // EOF
	}
	msg = list_first_entry(&gdev->published_list, struct msg_t, node);
	list_del(&msg->node);
	spin_unlock_bh(&gdev->published_list_lock);
	
	// data transfers
	n = min(msg->size, count);
	if (copy_to_user(buf, msg->text, n))
	{
		// in case of errors, recover atomically re-enqueuing the message
		spin_lock_bh(&gdev->published_list_lock);
		list_add(&msg->node, &gdev->published_list);
		spin_unlock_bh(&gdev->published_list_lock);
		return -EFAULT;
	}
	
	// atomically decrease group size
	spin_lock(&gdev->size_lock);
	gdev->size -= msg->size;
	spin_unlock(&gdev->size_lock);
	
	// atomically defer deallocation
	spin_lock_bh(&published_work_lock);
	list_add_tail(&msg->node, next_published_list);
	spin_unlock_bh(&published_work_lock);
	
	queue_work(groups_wq, &published_work);
	
	return n;
}

ssize_t group_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos) {
	struct group_dev_t *gdev = filp->private_data;
	struct delayed_msg_t* delayed_msg;
	struct msg_t* msg;
	int err = 0;
	unsigned int delay = 0;
	
	// terminator char only is not a valid message
	if(count <= 1) return -EBADMSG;

	// prepare message
	if (!(msg = kmem_cache_alloc(msg_cache, GFP_KERNEL))) return -ENOMEM;
	msg->size = count;
	if (!(msg->text = vmalloc(count)))
	{
		err = -ENOMEM;
		goto failed_textalloc;
	}
	if (copy_from_user(msg->text, buf, count))
	{
		err = -EFAULT;
		goto failed_copyfromuser;
	}
	INIT_LIST_HEAD(&msg->node);
	
	// atomic size checks
	spin_lock(&gdev->size_lock);
	if(count > gdev->max_msg_size)  // message ok?
	{
		spin_unlock(&gdev->size_lock);
		err = -EMSGSIZE;
		goto failed_message_size;
	}
	else if(count + gdev->size > gdev->max_strg_size)  // storage ok?
	{
		spin_unlock(&gdev->size_lock);
		err = -ENOSPC;
		goto failed_storage_size;
	}
	gdev->size += msg->size;
	spin_unlock(&gdev->size_lock);
	
	// atomically read delay
	delay = (unsigned int) atomic_read(&gdev->delay);

	
	if(!delay)  // immediate operating mode?
	{
		// atomically append msg
		spin_lock_bh(&gdev->published_list_lock);
		list_add_tail(&msg->node, &gdev->published_list);
		spin_unlock_bh(&gdev->published_list_lock);
	}
	else
	{
		// prepare delayed_msg
		if (!(delayed_msg = kmem_cache_alloc(delayed_msg_cache, GFP_KERNEL)))
		{
			err = -ENOMEM;
			goto failed_timeralloc;
		}
		delayed_msg->gdev = gdev;
		delayed_msg->msg = msg;
		delayed_msg->is_revoked = 0;
		delayed_msg->is_flushed = 0;
		INIT_LIST_HEAD(&delayed_msg->node);
		timer_setup(&delayed_msg->timer, timer_callback, 0);
		delayed_msg->timer.expires = jiffies + msecs_to_jiffies(delay);

		// atomically append delayed_msg
		spin_lock_bh(&gdev->delayed_list_lock);
		list_add_tail(&delayed_msg->node, &gdev->delayed_list);
		add_timer(&delayed_msg->timer); // activate timer
		spin_unlock_bh(&gdev->delayed_list_lock);
	}
	
	return count;

failed_timeralloc:
	// atomically decrease gdev->size
	spin_lock(&gdev->size_lock);
	gdev->size -= msg->size;
	spin_unlock(&gdev->size_lock);
failed_storage_size:
failed_message_size:
failed_copyfromuser:
	vfree(msg->text);
failed_textalloc:
	kmem_cache_free(msg_cache, msg);
	return err;
}

long group_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct group_dev_t *gdev = filp->private_data;
	int res = 0;
	
	// verifying cmd
	if (_IOC_TYPE(cmd) != _IOC_MAGIC || _IOC_NR(cmd) <= 0 || _IOC_NR(cmd) > _IOC_MAX)
	{
		return -ENOIOCTLCMD;
	}
	
	// dispatching command
	switch (cmd)
	{
		case SLEEP_ON_BARRIER:
		{
			spin_lock(&gdev->barrier_lock);

			gdev->sleepers++;

			// until alarm is off, keep sleeping
			while(!gdev->wakeup)
			{
				spin_unlock(&gdev->barrier_lock);

				if(wait_event_interruptible(gdev->sleeping_wq, gdev->wakeup))
				{
					spin_lock(&gdev->barrier_lock);
					// last sleeper shall turn off the alarm
					if (!(--gdev->sleepers)) gdev->wakeup = 0;
					spin_unlock(&gdev->barrier_lock);
					return -ERESTARTSYS;
				}

				spin_lock(&gdev->barrier_lock);
			}

			// last sleeper shall turn off the alarm
			if(!(--gdev->sleepers)) gdev->wakeup = 0;

			spin_unlock(&gdev->barrier_lock);
			break;
		}

		/* returns: 
		 * 0 if the alarm is set
		 * 1 if alarm was already on
		 * 2 if no one was sleeping (alarm is not set) */
		case AWAKE_BARRIER:
		{
			spin_lock(&gdev->barrier_lock);

			// no one sleeping, not setting the alarm
			if(!gdev->sleepers)
			{
				res = 2;
				spin_unlock(&gdev->barrier_lock);
				break;
			}

			// alarm already on, not setting the alarm
			if(gdev->wakeup)
			{
				res = 1;
				spin_unlock(&gdev->barrier_lock);
				break;
			}

			gdev->wakeup = 1;
			wake_up_interruptible(&gdev->sleeping_wq);
			spin_unlock(&gdev->barrier_lock);
			break;
		}
		
		/* returns: 
		 * 0 if the device already existed
		 * 1 if it is installed */
		case INSTALL_GROUP:
		{
			struct group_t *k_group = kmalloc(sizeof(struct group_t), GFP_KERNEL),
					*u_group = (struct group_t*) arg;
			struct group_dev_t* gdev;
			uint64_t hkey;
			int exists = 0;

			if (copy_from_user(k_group->id, u_group->id, 32))
			{
				return -EFAULT;
			}
			
			hkey = xxh64(k_group->id, strnlen(k_group->id, 32), 1);
			
			// first groups_htbl query
			spin_lock(&htbl_lock);
			hash_for_each_possible(groups_htbl, gdev, hnode, hkey)
			{
				if (!strcmp(gdev->group->id, k_group->id))
				{
					exists = 1;
					break;
				}
			}
			spin_unlock(&htbl_lock);

			if (!exists)
			{
				// first query failed
				int err = 0;
				dev_t dev;
				res = 1;
				
				// group requires installation
				spin_lock(&install_lock);
				
				// second groups_htbl query
				hash_for_each_possible(groups_htbl, gdev, hnode, hkey)
				{
					if (!strcmp(gdev->group->id, k_group->id))
					{
						exists = 1;
						break;
					}
				}
				
				if(!exists)
				{
					// prereserved minor availability
					if(groups+1 >= range)
					{
						spin_unlock(&install_lock);
						return -EDQUOT;
					}
					
					// second query failed -> installation
					dev = MKDEV(major, ++groups);
					if ((err = gdev_init(&gdev, k_group, dev, hkey)))
					{
						groups--;
						spin_unlock(&install_lock);
						return err;
					}
				}
				spin_unlock(&install_lock);
			}

			// return pathname to userspace
			copy_to_user(u_group->devname, gdev->group->devname, 32);

			break;
		}
		
		case SET_SEND_DELAY:
		{
			unsigned int delay;

			if (copy_from_user(&delay, (unsigned long*) arg, sizeof(unsigned int)))
				return -EFAULT;

			atomic_set(&gdev->delay, (int) delay);
			break;
		}
		
		case REVOKE_DELAYED_MESSAGES:
		{
			struct list_head* ptr;
			struct delayed_msg_t* delayed_msg;

			spin_lock_bh(&gdev->delayed_list_lock);	
			list_for_each(ptr, &gdev->delayed_list)
			{
				delayed_msg = container_of(ptr, struct delayed_msg_t, node);
					
				if((delayed_msg->is_revoked = del_timer(&delayed_msg->timer)))
				{
					ptr = ptr->prev;
					list_del(&delayed_msg->node);
					
					spin_lock(&gdev->size_lock);
					gdev->size -= delayed_msg->msg->size;
					spin_unlock(&gdev->size_lock);
					
					// atomically defer deallocation
					spin_lock_bh(&delayed_work_lock);
					list_add_tail(&delayed_msg->node, next_delayed_list);
					spin_unlock_bh(&delayed_work_lock);
				}
			}
			spin_unlock_bh(&gdev->delayed_list_lock);
			queue_work(groups_wq, &delayed_work);
			break;
		}

		default:
		{
			res = -1;
		}
	}

	return res;
}

int group_flush(struct file *filp, fl_owner_t id)
{
	struct group_dev_t *gdev = filp->private_data;
	struct delayed_msg_t* delayed_msg;
	struct list_head* pos;
	
	spin_lock_bh(&gdev->delayed_list_lock);
	list_for_each(pos, &gdev->delayed_list)
	{
		delayed_msg = container_of(pos, struct delayed_msg_t, node);
		delayed_msg->is_flushed = 1;
		pos = pos->prev;
		list_del(&delayed_msg->node);  // unlink delayed_msg
		spin_unlock_bh(&gdev->delayed_list_lock);

		if(del_timer_sync(&delayed_msg->timer))  // was timer stopped?
		{
			// callback was not executed: append message
			spin_lock_bh(&gdev->published_list_lock);
			list_add_tail(&delayed_msg->msg->node, &gdev->published_list);
			spin_unlock_bh(&gdev->published_list_lock);
		}
		// at this point, we know the callback function:
		// 1) will no longer trigger
		// 2) it's not currently executing
		// --> safely finalize: atomically defer deallocation
		spin_lock_bh(&delayed_work_lock);
		list_add_tail(&delayed_msg->node, next_delayed_list);
		spin_unlock_bh(&delayed_work_lock);

		spin_lock_bh(&gdev->delayed_list_lock);
	}
	spin_unlock_bh(&gdev->delayed_list_lock);
	queue_work(groups_wq, &delayed_work);
	return 0;
}


// ------------- MODULE MANAGEMENT ------------------- //

static int __init initfn(void)
{
	struct group_t *group;
	uint64_t hkey;
	dev_t dev = 0;
	int err;
	
	// global variables
	hash_init(groups_htbl);
	spin_lock_init(&install_lock);
	spin_lock_init(&htbl_lock);
	
	// lookaside caches
	if(!(msg_cache = kmem_cache_create("groups_msg",
		sizeof(struct msg_t), 0, 0, NULL)))
	{
		printk(KERN_ERR "%s: failed to create published msg cache.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_msg_cache;
	}
	
	if(!(delayed_msg_cache = kmem_cache_create("delayed_msg",
		sizeof(struct delayed_msg_t), 0, 0, NULL)))
	{
		printk(KERN_ERR "%s: failed to create delayed msg cache.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_delayed_cache;
	}
	
	if(!(group_dev_cache = kmem_cache_create("group_dev_t", 
		sizeof(struct group_dev_t), 0, SLAB_HWCACHE_ALIGN, NULL)))
	{
		printk(KERN_ERR "%s: failed to create group dev cache.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_group_dev_cache;
	}
	
	// garbage collection structures
	if(!(groups_wq = create_workqueue("groups_wq")))
	{
		printk(KERN_ERR "%s: failed to create workqueue.\n", KBUILD_MODNAME);
		goto failed_create_wq;
	}
	
	if(!(active_published_list = kzalloc(sizeof(struct list_head), GFP_KERNEL)))
	{
		printk(KERN_ERR "%s: no memory for active_published_list.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_active_published;
	}
	INIT_LIST_HEAD(active_published_list);
	
	if(!(next_published_list = kzalloc(sizeof(struct list_head), GFP_KERNEL)))
	{
		printk(KERN_ERR "%s: no memory for next_published_list.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_next_published;
	}
	INIT_LIST_HEAD(next_published_list);
	
	if(!(active_delayed_list = kzalloc(sizeof(struct list_head), GFP_KERNEL)))
	{
		printk(KERN_ERR "%s: no memory for active_delayed_list.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_active_delayed;
	}
	INIT_LIST_HEAD(active_delayed_list);
	
	if(!(next_delayed_list = kzalloc(sizeof(struct list_head), GFP_KERNEL)))
	{
		printk(KERN_ERR "%s: no memory for next_delayed_list.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_next_delayed;
	}
	INIT_LIST_HEAD(next_delayed_list);
	
	spin_lock_init(&published_work_lock);
	spin_lock_init(&delayed_work_lock);
	
	// dynamic major allocation
	if ((err = alloc_chrdev_region(&dev, 0, range, KBUILD_MODNAME)))
	{
		printk(KERN_WARNING "%s: can't get major %d.\n", KBUILD_MODNAME, major);
		goto failed_chrdevreg;
	}
	major = MAJOR(dev);

	// device class creation
	if (IS_ERR(class = class_create(THIS_MODULE, "groups")))
	{
		printk(KERN_ERR "%s: failed to register device class.\n", KBUILD_MODNAME);
		err = PTR_ERR(class);
		goto failed_classreg;
	}
	
	// allocation and initialization of the first group
	if (!(group = kzalloc(sizeof(struct group_t), GFP_KERNEL))) 
	{
		printk(KERN_ERR "%s: failed allocation of a new group.\n", KBUILD_MODNAME);
		err = -ENOMEM;
		goto failed_groupalloc;
	}
	snprintf(group->id, 32, "%s", KBUILD_MODNAME);
	hkey = xxh64(group->id, strlen(group->id), 1);
	if ((err = gdev_init(NULL, group, dev, hkey)))
	{
		goto failed_devreg;
	}
	
	return 0;

failed_devreg:
	kfree(group);
failed_groupalloc:
	class_unregister(class);
	class_destroy(class);
failed_classreg:
	unregister_chrdev_region(dev, 1);
failed_chrdevreg:
	kfree(next_delayed_list);
failed_next_delayed:
	kfree(active_delayed_list);
failed_active_delayed:
	kfree(next_published_list);
failed_next_published:
	kfree(active_published_list);
failed_active_published:
	destroy_workqueue(groups_wq);
failed_create_wq:
	kmem_cache_destroy(group_dev_cache);
failed_group_dev_cache:
	kmem_cache_destroy(delayed_msg_cache);
failed_delayed_cache:
	kmem_cache_destroy(msg_cache);
failed_msg_cache:
	return err;
}

static void __exit exitfn(void)
{
	unsigned long bkt = 0;
	struct group_dev_t *gdev = NULL, *gdev_prev = NULL;
	
	// garbage collection structures
	flush_workqueue(groups_wq);
	destroy_workqueue(groups_wq);
	
	published_work_fn(NULL);  // next_published_list and...
	delayed_work_fn(NULL);  // next_delayed_list, could be non-empty
	
	kfree(next_delayed_list);
	kfree(active_delayed_list);
	kfree(next_published_list);
	kfree(active_published_list);
	
	// destroy all groups
	while(gdev == NULL && bkt < HASH_SIZE(groups_htbl))
	{
		gdev = hlist_entry_safe((&groups_htbl[bkt])->first, struct group_dev_t, hnode);
		// for each entry
		while(gdev)
		{
			struct list_head *pos;
			struct msg_t *msg;
			dev_t dev = gdev->cdev.dev;
			
			sysfs_remove_file(&gdev->dev->kobj, &gdev->max_strg_size_attr.attr);
			sysfs_remove_file(&gdev->dev->kobj, &gdev->max_msg_size_attr.attr);
			cdev_del(&gdev->cdev);
			device_destroy(class, dev);

			// remove unread messages
			list_for_each(pos, &gdev->published_list)
			{
				msg = container_of(pos, struct msg_t, node);
				pos = pos->prev;
				list_del(&msg->node);
				vfree(msg->text);
				kmem_cache_free(msg_cache, msg);
			}

			// delayed messages were flushed on last close syscall
			
			// get new reference to next gdev
			gdev_prev = gdev;
			gdev = hlist_entry_safe((gdev)->hnode.next, struct group_dev_t, hnode);
			
			kfree(gdev_prev->group);
			kmem_cache_free(group_dev_cache, gdev_prev);
		}
		bkt++;
	}
	
	// destroy lookaside caches
	kmem_cache_destroy(msg_cache);
	kmem_cache_destroy(delayed_msg_cache);
	kmem_cache_destroy(group_dev_cache);
	
	unregister_chrdev_region(MKDEV(major,0), range);
	class_destroy(class);
	printk(KERN_INFO "%s (maj=%d): unloaded.\n", KBUILD_MODNAME, major);
}

module_init(initfn);
module_exit(exitfn);


MODULE_AUTHOR("Alessio Papi <papi.1761063@studenti.uniroma1.it>");
MODULE_DESCRIPTION("AOSV 2019/20: Final Project");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
