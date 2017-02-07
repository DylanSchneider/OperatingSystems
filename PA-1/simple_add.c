#include <linux/kernel.h>
#include <linux/linkage.h>

asmlinkage long sys_simple_add(int n1, int n2, int* result) {
    
    printk("This function will add the numbers %d and %d \n", n1, n2);
    *result = n1 + n2;
    printk("The result is: %d \n", *result);
    return 0;
    
}

