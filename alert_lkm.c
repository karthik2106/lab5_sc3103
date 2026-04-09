/*
 * SC3103 - Timed GPIO Alert System (Loadable Kernel Module)
 * alert_lkm.c
 *
 * Pibrella board GPIO mapping:
 *   GPIO4  - Green LED
 *   GPIO17 - Yellow LED
 *   GPIO27 - Red LED
 *   GPIO18 - Buzzer
 *   GPIO11 - Push Button
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/random.h>

#define GPIO_GREEN  4
#define GPIO_YELLOW 17
#define GPIO_RED    27
#define GPIO_BUZZER 18
#define GPIO_BUTTON 11

static unsigned int irq_number;

/* State machine:
 *   STATE_GREEN  - Green LED on, waiting for random delay
 *   STATE_ALERT  - Yellow LED + buzzer on, waiting for button press
 *   STATE_RED    - Red LED on for 2 seconds after button press
 */
enum alert_state { STATE_GREEN, STATE_ALERT, STATE_RED };
static enum alert_state state = STATE_GREEN;

static struct timer_list delay_timer;
static struct timer_list red_timer;

/* Forward declarations */
static void start_green_phase(void);

/* Called when the random delay expires: transition to alert phase */
static void delay_timer_callback(struct timer_list *t)
{
    /* Turn off green, turn on yellow + buzzer */
    gpio_set_value(GPIO_GREEN, 0);
    gpio_set_value(GPIO_YELLOW, 1);
    gpio_set_value(GPIO_BUZZER, 1);
    state = STATE_ALERT;
    printk(KERN_INFO "alert_lkm: Alert phase - yellow LED and buzzer ON\n");
}

/* Called 2 seconds after button press: reset cycle */
static void red_timer_callback(struct timer_list *t)
{
    gpio_set_value(GPIO_RED, 0);
    printk(KERN_INFO "alert_lkm: Cycle complete. Restarting...\n");
    start_green_phase();
}

/* Start a new cycle: green LED on with random delay */
static void start_green_phase(void)
{
    unsigned int delay;

    /* All LEDs and buzzer off */
    gpio_set_value(GPIO_GREEN, 0);
    gpio_set_value(GPIO_YELLOW, 0);
    gpio_set_value(GPIO_RED, 0);
    gpio_set_value(GPIO_BUZZER, 0);

    /* Random delay 1-10 seconds */
    get_random_bytes(&delay, sizeof(delay));
    delay = (delay % 10) + 1;

    printk(KERN_INFO "alert_lkm: Green phase - waiting %u seconds\n", delay);
    gpio_set_value(GPIO_GREEN, 1);
    state = STATE_GREEN;

    mod_timer(&delay_timer, jiffies + msecs_to_jiffies(delay * 1000));
}

/* Button press interrupt handler */
static irq_handler_t button_isr(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    if (state != STATE_ALERT)
        return (irq_handler_t) IRQ_HANDLED;

    /* Turn off buzzer and yellow, turn on red */
    gpio_set_value(GPIO_BUZZER, 0);
    gpio_set_value(GPIO_YELLOW, 0);
    gpio_set_value(GPIO_RED, 1);
    state = STATE_RED;

    printk(KERN_INFO "alert_lkm: Button pressed! Red LED ON for 2 seconds\n");

    /* Start 2 second timer for red LED */
    mod_timer(&red_timer, jiffies + msecs_to_jiffies(2000));

    return (irq_handler_t) IRQ_HANDLED;
}

static int __init alert_init(void)
{
    int result;

    printk(KERN_INFO "alert_lkm: Initializing Timed GPIO Alert System\n");

    /* Request GPIOs */
    gpio_request(GPIO_GREEN, "green_led");
    gpio_direction_output(GPIO_GREEN, 0);
    gpio_export(GPIO_GREEN, false);

    gpio_request(GPIO_YELLOW, "yellow_led");
    gpio_direction_output(GPIO_YELLOW, 0);
    gpio_export(GPIO_YELLOW, false);

    gpio_request(GPIO_RED, "red_led");
    gpio_direction_output(GPIO_RED, 0);
    gpio_export(GPIO_RED, false);

    gpio_request(GPIO_BUZZER, "buzzer");
    gpio_direction_output(GPIO_BUZZER, 0);
    gpio_export(GPIO_BUZZER, false);

    gpio_request(GPIO_BUTTON, "button");
    gpio_direction_input(GPIO_BUTTON);
    gpio_set_debounce(GPIO_BUTTON, 300);
    gpio_export(GPIO_BUTTON, false);

    /* Set up button interrupt */
    irq_number = gpio_to_irq(GPIO_BUTTON);
    result = request_irq(irq_number,
                         (irq_handler_t) button_isr,
                         IRQF_TRIGGER_RISING,
                         "alert_button_handler",
                         NULL);
    if (result != 0) {
        printk(KERN_ALERT "alert_lkm: Failed to request IRQ: %d\n", result);
        return result;
    }

    /* Set up timers */
    timer_setup(&delay_timer, delay_timer_callback, 0);
    timer_setup(&red_timer, red_timer_callback, 0);

    /* Start the first cycle */
    start_green_phase();

    return 0;
}

static void __exit alert_exit(void)
{
    /* Clean up timers */
    del_timer_sync(&delay_timer);
    del_timer_sync(&red_timer);

    /* Turn everything off */
    gpio_set_value(GPIO_GREEN, 0);
    gpio_set_value(GPIO_YELLOW, 0);
    gpio_set_value(GPIO_RED, 0);
    gpio_set_value(GPIO_BUZZER, 0);

    /* Free GPIOs */
    gpio_unexport(GPIO_GREEN);
    gpio_free(GPIO_GREEN);
    gpio_unexport(GPIO_YELLOW);
    gpio_free(GPIO_YELLOW);
    gpio_unexport(GPIO_RED);
    gpio_free(GPIO_RED);
    gpio_unexport(GPIO_BUZZER);
    gpio_free(GPIO_BUZZER);
    gpio_unexport(GPIO_BUTTON);
    gpio_free(GPIO_BUTTON);

    free_irq(irq_number, NULL);

    printk(KERN_INFO "alert_lkm: Module removed\n");
}

module_init(alert_init);
module_exit(alert_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SC3103");
MODULE_DESCRIPTION("Timed GPIO Alert System LKM for RPi with Pibrella board");
MODULE_VERSION("V1");
