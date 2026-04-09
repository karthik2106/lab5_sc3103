/*
 * SC3103 - Timed GPIO Alert System (User Space)
 * alert_user.c
 *
 * Pibrella board GPIO mapping:
 *   GPIO4  - Green LED
 *   GPIO17 - Yellow LED
 *   GPIO27 - Red LED
 *   GPIO18 - Buzzer
 *   GPIO11 - Push Button
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

#define GPIO_GREEN  4
#define GPIO_YELLOW 17
#define GPIO_RED    27
#define GPIO_BUZZER 18
#define GPIO_BUTTON 11

static int chip_fd;
static volatile int buzzer_on = 0;

/* Request a single GPIO line as output, return the line fd */
static int request_output(unsigned int offset, const char *label, int default_val)
{
    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lines = 1;
    req.lineoffsets[0] = offset;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req.default_values[0] = default_val;
    strncpy(req.consumer_label, label, sizeof(req.consumer_label) - 1);

    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror(label);
        return -1;
    }
    return req.fd;
}

/* Set an output line to a value */
static void set_output(int fd, int value)
{
    struct gpiohandle_data data;
    data.values[0] = value;
    ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}

/* Buzzer thread: toggles GPIO18 at ~2kHz to produce a square wave */
static void *buzzer_thread(void *arg)
{
    int fd = *(int *)arg;
    while (1) {
        if (buzzer_on) {
            set_output(fd, 1);
            usleep(250);  /* ~2kHz tone */
            set_output(fd, 0);
            usleep(250);
        } else {
            usleep(1000);
        }
    }
    return NULL;
}

/* Request a GPIO line for rising-edge events, return the event fd */
static int request_button_event(unsigned int offset, const char *label)
{
    struct gpioevent_request req;
    memset(&req, 0, sizeof(req));
    req.lineoffset = offset;
    req.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    strncpy(req.consumer_label, label, sizeof(req.consumer_label) - 1);

    if (ioctl(chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req) < 0) {
        perror(label);
        return -1;
    }
    return req.fd;
}

int main(void)
{
    chip_fd = open("/dev/gpiochip0", O_RDWR);
    if (chip_fd < 0) {
        perror("Failed to open /dev/gpiochip0");
        return EXIT_FAILURE;
    }

    srand(time(NULL));

    int fd_green  = request_output(GPIO_GREEN,  "green_led",  0);
    int fd_yellow = request_output(GPIO_YELLOW, "yellow_led", 0);
    int fd_red    = request_output(GPIO_RED,    "red_led",    0);
    int fd_buzzer = request_output(GPIO_BUZZER, "buzzer",     0);
    int fd_button = request_button_event(GPIO_BUTTON, "button");

    if (fd_green < 0 || fd_yellow < 0 || fd_red < 0 ||
        fd_buzzer < 0 || fd_button < 0) {
        fprintf(stderr, "Failed to set up GPIO lines\n");
        close(chip_fd);
        return EXIT_FAILURE;
    }

    /* Start buzzer thread */
    pthread_t bz_thread;
    pthread_create(&bz_thread, NULL, buzzer_thread, &fd_buzzer);

    printf("Timed GPIO Alert System started. Press Ctrl+C to exit.\n");

    while (1) {
        /* Step 1: Green LED ON, random delay 1-10 seconds */
        int delay = (rand() % 10) + 1;
        printf("Green LED ON. Waiting %d seconds...\n", delay);
        set_output(fd_green, 1);
        sleep(delay);

        /* Step 2: After delay, buzzer ON and yellow LED ON */
        printf("Yellow LED ON, Buzzer ON. Waiting for button press...\n");
        set_output(fd_green, 0);
        set_output(fd_yellow, 1);
        buzzer_on = 1;  /* start square wave */

        /* Step 3: Drain any leftover button events, then wait for new press */
        struct gpioevent_data event;
        struct pollfd pfd = { .fd = fd_button, .events = POLLIN };
        while (poll(&pfd, 1, 0) > 0) {
            read(fd_button, &event, sizeof(event));
        }
        read(fd_button, &event, sizeof(event));

        /* Button pressed: buzzer OFF, red LED ON for 2 seconds */
        printf("Button pressed! Buzzer OFF, Red LED ON for 2 seconds.\n");
        buzzer_on = 0;  /* stop square wave */
        set_output(fd_buzzer, 0);
        set_output(fd_yellow, 0);
        set_output(fd_red, 1);
        sleep(2);

        /* Step 4: Reset and start new cycle */
        set_output(fd_red, 0);
        printf("Cycle complete. Restarting...\n\n");
    }

    close(fd_button);
    close(fd_green);
    close(fd_yellow);
    close(fd_red);
    close(fd_buzzer);
    close(chip_fd);

    return EXIT_SUCCESS;
}
