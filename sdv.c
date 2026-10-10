#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <hw/inout.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <sched.h>

/* GPIO BASE */
#define GPIO_BASE 0xFE200000
#define BLOCK_SIZE 4096

#define GPFSEL0 (0x00/4)
#define GPFSEL1 (0x04/4)
#define GPFSEL2 (0x08/4)
#define GPSET0  (0x1C/4)
#define GPCLR0  (0x28/4)
#define GPLEV0  (0x34/4)

/* Motor (L298N) */
#define IN1  (1<<17) //Pin 11
#define ENAB (1<<18) //Pin 12  -- shared ENA + ENB, tied together on the driver
#define IN2  (1<<27) //Pin 13
#define IN3  (1<<22) //Pin 15
#define IN4  (1<<24) //Pin 18

/* Ultrasonic */
#define TRIG  (1<<5) //Pin 29
#define ECHO_PIN (1<<6) //Pin 31

/* Indicators */
#define GREEN_LED (1<<16) //Pin 36
#define RED_LED   (1<<20) //Pin 38
#define LASER     (1<<25) //Pin 22
#define BLUE_LED  (1<<23) //Pin 16  -- NEW: caution/slow-zone indicator

#define SERIAL_PORT "/dev/ser1" // HC-05 UART

/* ---- Speed / distance thresholds (tune here) ----
 * d <= 20            -> STOP
 * 20 < d <= 35        -> SLOW  (Blue LED)
 * d > 35              -> NORMAL (Green LED)
 */
#define DIST_STOP_CM    20.0   // <= this: full stop, red LED + laser
#define DIST_SLOW_CM    35.0   // <= this (and > STOP): slow zone, blue LED
#define DUTY_SLOWZONE   30    // % duty when 20 < d <= 35
#define DUTY_CLEARZONE  50    // % duty when d > 35

/* Shared */
volatile uint32_t *gpio;
double dis = 50;
pthread_mutex_t lock;

/* -------- Ultrasonic -------- */
double get_distance()
{
    struct timespec start,end;
    gpio[GPCLR0]=TRIG;
    usleep(2);
    gpio[GPSET0]=TRIG;
    usleep(10);
    gpio[GPCLR0]=TRIG;

    clock_gettime(CLOCK_MONOTONIC,&start);
    while(!(gpio[GPLEV0]&ECHO_PIN))
    {
        clock_gettime(CLOCK_MONOTONIC,&end);
        double t=(end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
        if(t>0.03) return 999;
    }

    clock_gettime(CLOCK_MONOTONIC,&start);
    while(gpio[GPLEV0]&ECHO_PIN)
    {
        clock_gettime(CLOCK_MONOTONIC,&end);
        double t=(end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
        if(t>0.03) return 999;
    }

    clock_gettime(CLOCK_MONOTONIC,&end);
    double t=(end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
    return (t*34300)/2;
}

/* -------- SENSOR THREAD -------- */
void *sensor_thread(void *arg)
{
    double d_fil=50;
    while(1)
    {
        double d_raw=get_distance();
        if(d_raw!=999)
        {
            d_fil=0.4*d_fil+0.6*d_raw;
            pthread_mutex_lock(&lock);
            dis=d_fil;
            pthread_mutex_unlock(&lock);
        }
        usleep(50000);
    }
}

/* -------- MOTOR THREAD -------- */
void *motor_thread(void *arg)
{
    while(1)
    {
        double d;
        pthread_mutex_lock(&lock);
        d=dis;
        pthread_mutex_unlock(&lock);

        int duty;
        if(d<=DIST_STOP_CM)
            duty=0;                 // <= 20 cm -> stop
        else if(d<=DIST_SLOW_CM)
            duty=DUTY_SLOWZONE;     // 20 < d <= 35 cm -> slow (30%)
        else
            duty=DUTY_CLEARZONE;    // > 35 cm -> normal (40%)

        if(duty==0)
        {
            /* STOP zone: Red + Laser ON, Green/Blue OFF */
            gpio[GPSET0]=RED_LED|LASER;
            gpio[GPCLR0]=GREEN_LED|BLUE_LED;
        }
        else if(d<=DIST_SLOW_CM)
        {
            /* SLOW zone: Blue LED ON, everything else OFF */
            gpio[GPSET0]=BLUE_LED;
            gpio[GPCLR0]=RED_LED|LASER|GREEN_LED;
        }
        else
        {
            /* NORMAL zone: Green LED ON, everything else OFF */
            gpio[GPSET0]=GREEN_LED;
            gpio[GPCLR0]=RED_LED|LASER|BLUE_LED;
        }

        /* Forward direction on both motors: IN1/IN3 high, IN2/IN4 low */
        gpio[GPSET0]=IN1|IN3;
        gpio[GPCLR0]=IN2|IN4;

        for(int i=0;i<20;i++)
        {
            if(duty==0)
            {
                gpio[GPCLR0]=ENAB;
                usleep(10000);
            }
            else
            {
                int on=(10000*duty)/100;
                int off=10000-on;
                gpio[GPSET0]=ENAB;
                usleep(on);
                gpio[GPCLR0]=ENAB;
                usleep(off);
            }
        }
    }
}

/* -------- COMM THREAD (sends status to ESP32 via HC-05) -------- */
void *comm_thread(void *arg)
{
    int fd;
    struct termios options;

    fd=open(SERIAL_PORT,O_RDWR|O_NOCTTY);
    if(fd==-1)
    {
        printf("ERROR: Failed to open %s (check wiring/port name)\n",SERIAL_PORT);
        pthread_exit(NULL);
    }

    if(tcgetattr(fd,&options)!=0)
    {
        printf("ERROR: tcgetattr failed on %s\n",SERIAL_PORT);
        close(fd);
        pthread_exit(NULL);
    }

    cfsetispeed(&options,B9600);
    cfsetospeed(&options,B9600);
    options.c_cflag|=(CLOCAL|CREAD);
    options.c_cflag&=~PARENB;   // no parity
    options.c_cflag&=~CSTOPB;   // 1 stop bit
    options.c_cflag&=~CSIZE;
    options.c_cflag|=CS8;       // 8 data bits
    options.c_lflag&=~(ICANON|ECHO|ECHOE|ISIG); // raw mode

    if(tcsetattr(fd,TCSANOW,&options)!=0)
    {
        printf("ERROR: tcsetattr failed on %s\n",SERIAL_PORT);
        close(fd);
        pthread_exit(NULL);
    }

    printf("HC-05 UART opened successfully on %s at 9600 baud\n",SERIAL_PORT);

    while(1)
    {
        double d;
        pthread_mutex_lock(&lock);
        d=dis;
        pthread_mutex_unlock(&lock);

        char msg[150];
        if(d<=DIST_STOP_CM)
            sprintf(msg,"ID:A1206;MODE:NORMAL;STATE:OBSTACLE\n");
        else if(d<=DIST_SLOW_CM)
            sprintf(msg,"ID:A1206;MODE:NORMAL;STATE:TRAFFIC\n");
        else
            sprintf(msg,"ID:A1206;MODE:NORMAL;STATE:CLEAR\n");

        int n=write(fd,msg,strlen(msg));
        if(n<0)
            printf("ERROR: write to %s failed\n",SERIAL_PORT);
        else
            printf("Sent (%d bytes): %s",n,msg);

        usleep(200000);
    }
}

/* -------- MAIN -------- */
int main()
{
    pthread_t t1,t3,t4;
    struct sched_param param;
    pthread_attr_t attr;

    ThreadCtl(_NTO_TCTL_IO,0);
    pthread_mutex_init(&lock,NULL);

    void *gpio_ptr=mmap_device_memory(NULL,BLOCK_SIZE,PROT_READ|PROT_WRITE|PROT_NOCACHE,0,GPIO_BASE);
    gpio=(uint32_t*)gpio_ptr;

    /* GPIO CONFIG -- all pins below set as outputs; ECHO (GPIO6) stays input */
    gpio[GPFSEL0]&=~(7<<15);
    gpio[GPFSEL0]|=(1<<15);   // GPIO5  TRIG   -> output
    gpio[GPFSEL0]&=~(7<<18);  // GPIO6  ECHO   -> input (left cleared)

    gpio[GPFSEL1]&=~(7<<18);
    gpio[GPFSEL1]|=(1<<18);   // GPIO16 GREEN_LED -> output

    gpio[GPFSEL1]&=~(7<<21);
    gpio[GPFSEL1]|=(1<<21);   // GPIO17 IN1    -> output

    gpio[GPFSEL1]&=~(7<<24);
    gpio[GPFSEL1]|=(1<<24);   // GPIO18 ENAB   -> output

    gpio[GPFSEL2]&=~(7<<0);
    gpio[GPFSEL2]|=(1<<0);    // GPIO20 RED_LED -> output

    gpio[GPFSEL2]&=~(7<<6);
    gpio[GPFSEL2]|=(1<<6);    // GPIO22 IN3    -> output

    gpio[GPFSEL2]&=~(7<<9);
    gpio[GPFSEL2]|=(1<<9);    // GPIO23 BLUE_LED -> output  (NEW, Physical Pin 16)

    gpio[GPFSEL2]&=~(7<<12);
    gpio[GPFSEL2]|=(1<<12);   // GPIO24 IN4    -> output

    gpio[GPFSEL2]&=~(7<<15);
    gpio[GPFSEL2]|=(1<<15);   // GPIO25 LASER  -> output

    gpio[GPFSEL2]&=~(7<<21);
    gpio[GPFSEL2]|=(1<<21);   // GPIO27 IN2    -> output

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr,SCHED_FIFO);
    pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED);

    param.sched_priority=30;
    pthread_attr_setschedparam(&attr,&param);
    pthread_create(&t1,&attr,sensor_thread,NULL);

    param.sched_priority=20;
    pthread_attr_setschedparam(&attr,&param);
    pthread_create(&t3,&attr,motor_thread,NULL);

    param.sched_priority=10;
    pthread_attr_setschedparam(&attr,&param);
    pthread_create(&t4,&attr,comm_thread,NULL);

    pthread_join(t1,NULL);
    pthread_join(t3,NULL);
    pthread_join(t4,NULL);
    return 0;
}
