#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#define SHARED_IRQ 17

static int irq = SHARED_IRQ;
static int my_dev_id;
static int irq_counter = 0;

// register parameter that can be read by anyone, but can't be changed
module_param(irq, int, S_IRUGO);


struct timer_list my_timer;


static int previous_irq_counter = 0;

static void timer_function(struct timer_list* tlist) {
	printk(KERN_INFO "Last minute info: delta counter = %d\n", 
			irq_counter - previous_irq_counter);
	previous_irq_counter = irq_counter;
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(60 * 1000));
}


static irqreturn_t my_interrupt(int irq, void *dev_id) {              
	irq_counter++;
	printk(KERN_INFO "In the ISR: counter = %d\n", irq_counter);
	// return IRQ_NONE because we are just observing
	return IRQ_NONE;
}
	

static int __init my_init(void) {
	timer_setup(&my_timer, timer_function, 0);
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(60 * 1000));
	printk(KERN_INFO "Registered timer successfully\n");

	// my_interrupt will be called whenever kernel receives the interrupt below
	if (request_irq(irq, my_interrupt, IRQF_SHARED, "my_interrupt", &my_dev_id)) {
		return -1;
	}
	printk(KERN_INFO "Loading ISR handler on IRQ %d\n", irq);
	printk(KERN_INFO "Initial irq_counter = %d\n", irq_counter);
	return 0;
}


static void __exit my_exit(void) {
	synchronize_irq(irq);
	free_irq(irq, &my_dev_id);
	printk(KERN_INFO "Unloading ISR handler, final irq_counter = %d\n", irq_counter);
}


module_init(my_init);
module_exit(my_exit);
MODULE_AUTHOR("Elisey Sudakov");
MODULE_DESCRIPTION("linux task 2");
MODULE_LICENSE("GPL v2");
