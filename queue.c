#include <linux/poll.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/mutex.h>

MODULE_AUTHOR("Giulio Zingrillo");
MODULE_DESCRIPTION("Misc queue device");
MODULE_LICENSE("GPL");

extern int my_strlen(char stringa[]);
extern int period, max_elems;
extern unsigned int elems;
extern struct list_head head;
struct node{
    char value[31];
    struct list_head kl;
};
extern struct mutex my_mutex;
extern wait_queue_head_t my_waitqueue;
static int my_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int my_close(struct inode *inode, struct file *file)
{
    return 0;
}

ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
    struct list_head *l, *tmp;
    struct node *n;
    int i =0, err, res;
    mutex_lock(&my_mutex);
    if(list_empty(&head)){
        printk("Queue is empty\n");
        mutex_unlock(&my_mutex);
        return 0;
    }
    printk("Printing items in the queue...");
    list_for_each_safe(l, tmp, &head){
        n = list_entry(l, struct node, kl);
        printk("Value: %s\n", n->value);
        if(i==0){//only deletes the first element
            i=1;
            res = my_strlen(n->value);
            err = copy_to_user(buf, n->value, res);//copy of the first element in buffer
            buf[res] = '\0';//add the end of string
            //using copy to user function, as it is already implemented
            if (err) {//check the success of the copy
                mutex_unlock(&my_mutex);
                return -EFAULT;
            }
            list_del(l);
            kfree(n);
            (elems)--;
        }
        printk("Sleeping for %d ms...\n", period);//the task sleeps for the set period
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(period * (HZ/1000));
        if (signal_pending(current)){
            mutex_unlock(&my_mutex);
            return -ERESTARTSYS;
        }
        printk("Process awake\n");
    }
    printk("First element successfully deleted. Elements are now %d\n", elems);
    if(elems == max_elems-1)
        wake_up_interruptible(&my_waitqueue);
    mutex_unlock(&my_mutex);
    return res+1;//returning the first element of the queue: +1 is present because of the '\0' char
}

static ssize_t my_write(struct file *file, const char __user * buf, size_t count, loff_t *ppos){
    int err, len, res;
    struct node *n;
    mutex_lock(&my_mutex);
    //checks the input length
    if (count > 31){
        //control in the input length
        mutex_unlock(&my_mutex);
        return -1;
    }
    else
        len = count;
    //adds the task to a waitqueue if the queue has the max number of elements
    while(elems==max_elems){
        mutex_unlock(&my_mutex);
        res = wait_event_interruptible(my_waitqueue, (elems<max_elems));
        //signal handling
        if(res<0)
            return res;
        mutex_lock(&my_mutex);
    }
    n = kmalloc(sizeof(struct node), GFP_KERNEL);
    //kmalloc handling
    if (n == NULL) {
        printk("Failed to allocate memory\n");
        mutex_unlock(&my_mutex);
        return -1;
    }
    memset(n->value, '\0', 31);//initializing buffer
    err = copy_from_user(n->value, buf, 30);
    //copy handling
    if (err) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }
    list_add_tail(&(n->kl), &head);
    (elems)++;
    printk("A new element was added to the queue! Elements are now %d.\n", elems);
    mutex_unlock(&my_mutex);
    return len;
}


static struct file_operations my_fops = {
        .owner =        THIS_MODULE,
        .read =         my_read,
        .open =         my_open,
        .write =        my_write,
        .release =      my_close,
#if 0
        .poll =         my_poll,
        .fasync =       my_fasync,
#endif
};

struct miscdevice queue_device = {
        MISC_DYNAMIC_MINOR, "queue_device", &my_fops
};

