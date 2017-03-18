#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>
#include<asm/uaccess.h>
#define BUFFER_SIZE 1024
#define MAJOR_NUMBER 240
#define DEV_NAME "simple_character_device"


static char device_buffer[BUFFER_SIZE];

static int openCount = 0, closeCount = 0;


ssize_t simple_char_driver_read (struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
    printk(KERN_ALERT "Reading from device %d.\n", MAJOR_NUMBER);
    //Calculate how much buffer space is left
    int buffer_left = BUFFER_SIZE - *offset;
    int reading;
    if(buffer_left > length)
        reading = length;
    else
        reading = buffer_left;
    
    //Send data from device_buffer -> __user buffer
    copy_to_user(buffer, device_buffer, reading);
    
    return reading;
}



ssize_t simple_char_driver_write (struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
    printk(KERN_ALERT "Writing to device %d.\n", MAJOR_NUMBER);
 
    //Set the offset at the last character to append instead of overwriting
    *offset = strlen(device_buffer);
    //Calculate how much buuffer space is left.
    int buffer_left = BUFFER_SIZE - *offset;
    
    if(*offset >= BUFFER_SIZE)
        return 0;
    if(buffer_left > length)
        buffer_left = length;
    
    //Receive data from __user buffer -> device_buffer
    copy_from_user(device_buffer + *offset, buffer, buffer_left);
    
    if(buffer_left){
        printk(KERN_ALERT "%d bytes written", length);
    }
    
    return buffer_left;

    
	
}


int simple_char_driver_open (struct inode *pinode, struct file *pfile)
{
    printk(KERN_ALERT "Device %d has been opened %d times now.", MAJOR_NUMBER, openCount++);
	return 0;
}


int simple_char_driver_close (struct inode *pinode, struct file *pfile)
{
    printk(KERN_ALERT "Device %d has been closed %d times now.", MAJOR_NUMBER, closeCount++);
	return 0;
}

struct file_operations simple_char_driver_file_operations = {

	.owner = THIS_MODULE,
    .open = simple_char_driver_open,
    .release = simple_char_driver_close, //fs.h has only release(), no close().
    .read = simple_char_driver_read,
    .write = simple_char_driver_write,
};

static int simple_char_driver_init(void)
{
    printk(KERN_ALERT "Initializing device %d.\n", MAJOR_NUMBER);
    register_chrdev(MAJOR_NUMBER, DEV_NAME, &simple_char_driver_file_operations);
	return 0;
}

static int simple_char_driver_exit(void)
{
    printk(KERN_ALERT "Removing device %d.\n", MAJOR_NUMBER, &simple_char_driver_file_operations);
    unregister_chrdev(MAJOR_NUMBER, DEV_NAME);
	return 0;
}
//call init when the module is installed
module_init(simple_char_driver_init);
//call exit when the module is uninstalled
module_exit(simple_char_driver_exit);

