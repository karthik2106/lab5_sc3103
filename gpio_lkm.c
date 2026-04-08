/*
 * SC3103 Exercise 5 - Exercise B (4.2): GPIO Device Driver LKM
 * gpio_lkm.c - Loadable Kernel Module for GPIO access
 *
 * Toggles Green LED (GPIO4) on Push Button (GPIO11) press via interrupt.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

static unsigned int ledGreen = 4;       // GPIO4 connected to Green LED
static unsigned int pushButton = 11;    // GPIO11 connected to Push Button
static unsigned int irqNumber;          // share IRQ num within file
static bool ledOn = 0;                  // used to toggle state of LED

// The GPIO IRQ Handler function
static irq_handler_t rpi_gpio_isr(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    ledOn = !ledOn;                         // toggle the LED state
    gpio_set_value(ledGreen, ledOn);        // set LED accordingly
    printk(KERN_ALERT "GPIO Interrupt! LED state: %d\n", ledOn);
    return (irq_handler_t) IRQ_HANDLED;     // announce IRQ handled
}

// The LKM exit function
static void __exit rpi_gpio_exit(void)
{
    gpio_set_value(ledGreen, 0);        // turn the LED off
    gpio_unexport(ledGreen);            // unexport the LED GPIO
    gpio_free(ledGreen);                // free the LED GPIO
    gpio_unexport(pushButton);          // unexport the Button GPIO
    gpio_free(pushButton);              // free the Button GPIO
    free_irq(irqNumber, NULL);          // free the IRQ number, no *dev_id
    printk(KERN_ALERT "Goodbye from the GPIO LKM!\n");
}

// The LKM initialization function
static int __init rpi_gpio_init(void)
{
    int result = 0;
    printk(KERN_ALERT "Initializing the GPIO LKM\n");

    ledOn = true;                                   // default for LED is ON

    // Set up Green LED GPIO
    gpio_request(ledGreen, "sysfs");                // request for LED GPIO
    gpio_direction_output(ledGreen, ledOn);          // set output mode, turn on LED
    gpio_export(ledGreen, false);                    // export to sysfs

    // Set up Push Button GPIO
    gpio_request(pushButton, "sysfs");               // request for push Button GPIO
    gpio_direction_input(pushButton);                // set up as input
    gpio_set_debounce(pushButton, 1000);             // debounce delay of 1000ms
    gpio_export(pushButton, false);                  // export to sysfs

    // Map push button GPIO to IRQ number
    irqNumber = gpio_to_irq(pushButton);
    printk(KERN_ALERT "Button mapped to IRQ: %d\n", irqNumber);

    // Request an interrupt line
    result = request_irq(irqNumber,                  // interrupt number requested
                         (irq_handler_t) rpi_gpio_isr, // ISR handler function
                         IRQF_TRIGGER_RISING,        // trigger on rising edge
                         "rpi_gpio_handler",         // used in /proc/interrupts
                         NULL);                      // *dev_id for shared interrupt lines

    if (result != 0) {
        printk(KERN_ALERT "Failed to request IRQ: %d\n", result);
    }

    return result;
}

module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SC3103");
MODULE_DESCRIPTION("GPIO LKM for RPi with Pibrella board");
MODULE_VERSION("V1");
