#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum { CAPACITY = 4 };
typedef struct { long long ms; int valid, ticks; } Sample;
typedef struct { Sample rows[CAPACITY]; size_t used; int missing; } Logger;
static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping = 1; }

/* TMP102 default mode: 12-bit signed temperature, 1/16 degree per bit. */
static int decode(unsigned char high, unsigned char low) {
    int raw = (high << 4) | (low >> 4);
    return raw >= 2048 ? raw - 4096 : raw;
}

static int read_tmp102(int fd, int *ticks) {
    unsigned char reg = 0, bytes[2] = {0};
    struct i2c_msg msgs[] = {
        {.addr=0x48,.len=1,.buf=&reg},
        {.addr=0x48,.flags=I2C_M_RD,.len=2,.buf=bytes}
    };
    struct i2c_rdwr_ioctl_data transfer = {.msgs=msgs,.nmsgs=2};
    if (ioctl(fd,I2C_RDWR,&transfer) != 2) return 0;
    *ticks = decode(bytes[0],bytes[1]);
    return 1;
}

static FILE *serial_output(const char *path) {
    int fd = open(path,O_WRONLY | O_NOCTTY);
    if (fd < 0) return NULL;
    struct termios t;
    if (tcgetattr(fd,&t) == 0) {
        t.c_iflag = 0; t.c_oflag = 0; t.c_lflag = 0;
        t.c_cflag = CS8 | CLOCAL | CREAD;
        if (!cfsetospeed(&t,B115200) && !cfsetispeed(&t,B115200)
            && tcsetattr(fd,TCSANOW,&t) == 0) {
            FILE *out = fdopen(fd,"w");
            if (out) return out;
        }
    }
    int saved = errno; close(fd); errno = saved;
    return NULL;
}

static int flush(Logger *log, FILE *out) {
    for (size_t i = 0; i < log->used; ++i) {
        Sample s = log->rows[i];
        const char *status = s.valid ? (log->missing ? "recovered" : "ok") : "missing";
        if (s.valid) fprintf(out,"%lld,%s,%.4f\n",s.ms,status,s.ticks/16.0);
        else fprintf(out,"%lld,%s,\n",s.ms,status);
        log->missing = !s.valid;
    }
    log->used = 0;
    return fflush(out) || ferror(out);
}

static long long now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now)) return -1;
    return (long long)now.tv_sec*1000 + now.tv_nsec/1000000;
}

int main(int argc, char **argv) {
    int demo = argc == 2 && !strcmp(argv[1],"--demo");
    int hardware = (argc == 3 || argc == 5) && !strcmp(argv[1],"--i2c");
    if ((!demo && !hardware) || (argc == 5 && strcmp(argv[3],"--uart"))) {
        fprintf(stderr,"Usage: sensorlog --demo | --i2c BUS [--uart DEVICE]\n");
        return 2;
    }
    int bus = -1;
    FILE *out = stdout;
    if (hardware && (bus=open(argv[2],O_RDWR)) < 0) { perror(argv[2]); return 1; }
    if (argc==5 && !(out=serial_output(argv[4]))) { perror(argv[4]); close(bus); return 1; }
    struct sigaction sa = {0};
    sa.sa_handler = stop;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);
    Logger log = {0};
    int failed = 0;
    long long start = now_ms();
    if (start < 0) { perror("clock"); failed=1; }
    fputs("elapsed_ms,status,temperature_c\n",out);
    for (unsigned long i=0; !failed && !stopping && (!demo || i<8); ++i) {
        Sample s = {0};
        if (demo) {
            static const unsigned char words[8][2] = {
                {0x19,0x00},{0x19,0x10},{0,0},{0,0},
                {0xff,0x00},{0x00,0x00},{0x00,0x10},{0x01,0x00}
            };
            s.ms = (long long)i*1000;
            s.valid = i!=2 && i!=3;
            s.ticks = decode(words[i][0],words[i][1]);
        } else {
            long long sample_time = now_ms();
            if (sample_time < 0) { perror("clock"); failed=1; break; }
            s.ms = sample_time-start;
            s.valid = read_tmp102(bus,&s.ticks);
        }
        log.rows[log.used++] = s;
        if (log.used==CAPACITY) failed = flush(&log,out);
        if (!demo && !stopping && !failed) sleep(1);
    }
    if (!failed) failed = flush(&log,out);
    if (out!=stdout && fclose(out)) failed=1;
    if (bus>=0) close(bus);
    if (failed) fprintf(stderr,"Sampling or output failed.\n");
    return failed ? 1 : 0;
}
