/*
 * SC3103 Exercise 5 - Exercise A: User Space GPIO Access
 * gpio.c - Blink Green/Yellow LEDs and monitor Push Button for Red LED
 *
 * Pibrella board GPIO mapping:
 *   GPIO4  - Green LED
 *   GPIO17 - Yellow LED
 *   GPIO27 - Red LED
 *   GPIO11 - Push Button
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

int main(int argc, char *argv[])
{
    int fd0 = open("/dev/gpiochip0", O_RDWR);
    if (fd0 < 0) {
        perror("Failed to open /dev/gpiochip0");
        return EXIT_FAILURE;
    }

    // Get chip information
    struct gpiochip_info cinfo;
    ioctl(fd0, GPIO_GET_CHIPINFO_IOCTL, &cinfo);
    fprintf(stdout, "GPIO chip 0: %s, \"%s\", %u lines\n",
            cinfo.name, cinfo.label, cinfo.lines);

    // ---- Exercise 3.1: Blink Green and Yellow LEDs ----

    struct gpiohandle_request req_GY;   // Green and Yellow
    struct gpiohandle_data data_GY;     // data bits

    memset(&req_GY, 0, sizeof(req_GY));
    req_GY.lines = 2;
    req_GY.lineoffsets[0] = 4;         // GPIO4 - Green LED
    req_GY.lineoffsets[1] = 17;        // GPIO17 - Yellow LED
    req_GY.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req_GY.default_values[0] = 1;      // Green ON initially
    req_GY.default_values[1] = 0;      // Yellow OFF initially
    strcpy(req_GY.consumer_label, "green_yellow_leds");

    if (ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_GY) < 0) {
        perror("Failed to get line handle for Green/Yellow LEDs");
        close(fd0);
        return EXIT_FAILURE;
    }

    data_GY.values[0] = 1;  // Green ON
    data_GY.values[1] = 0;  // Yellow OFF

    printf("Blinking Green and Yellow LEDs 5 times...\n");
    for (int i = 0; i < 5; ++i) {
        ioctl(req_GY.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_GY);
        usleep(1000000);  // sleep 1 second
        data_GY.values[0] = !data_GY.values[0];  // toggle Green
        data_GY.values[1] = !data_GY.values[1];  // toggle Yellow
    }

    close(req_GY.fd);  // release Green/Yellow line handle

    // ---- Exercise 3.2: Monitor Push Button and Blink Red LED ----

    // Set up Red LED (GPIO27) as output
    struct gpiohandle_request req_Red;
    struct gpiohandle_data data_Red;

    memset(&req_Red, 0, sizeof(req_Red));
    req_Red.lines = 1;
    req_Red.lineoffsets[0] = 27;       // GPIO27 - Red LED
    req_Red.flags = GPIOHANDLE_REQUEST_OUTPUT;
    req_Red.default_values[0] = 0;     // Red OFF initially
    strcpy(req_Red.consumer_label, "red_led");

    if (ioctl(fd0, GPIO_GET_LINEHANDLE_IOCTL, &req_Red) < 0) {
        perror("Failed to get line handle for Red LED");
        close(fd0);
        return EXIT_FAILURE;
    }

    // Set up Push Button (GPIO11) as input with event monitoring
    struct gpioevent_request req_Btn;

    memset(&req_Btn, 0, sizeof(req_Btn));
    req_Btn.lineoffset = 11;           // GPIO11 - Push Button
    req_Btn.handleflags = GPIOHANDLE_REQUEST_INPUT;
    req_Btn.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
    strcpy(req_Btn.consumer_label, "push_button");

    if (ioctl(fd0, GPIO_GET_LINEEVENT_IOCTL, &req_Btn) < 0) {
        perror("Failed to get event handle for Push Button");
        close(req_Red.fd);
        close(fd0);
        return EXIT_FAILURE;
    }

    printf("Monitoring Push Button... Press button to blink Red LED.\n");
    printf("Press Ctrl+C to exit.\n");

    // Poll for button press events
    while (1) {
        struct gpioevent_data event;
        ssize_t rd = read(req_Btn.fd, &event, sizeof(event));
        if (rd == sizeof(event)) {
            printf("Button pressed! Blinking Red LED...\n");
            // Blink Red LED 3 times
            for (int i = 0; i < 3; ++i) {
                data_Red.values[0] = 1;  // Red ON
                ioctl(req_Red.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_Red);
                usleep(200000);  // 200ms
                data_Red.values[0] = 0;  // Red OFF
                ioctl(req_Red.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data_Red);
                usleep(200000);  // 200ms
            }
        }
    }

    close(req_Btn.fd);
    close(req_Red.fd);
    close(fd0);

    exit(EXIT_SUCCESS);
}
