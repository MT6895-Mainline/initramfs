#define DEVICE_QQCANDY
#define INIT_TEST
#define BOOT_PARTITION "/dev/sdc80"
#include "../init.c"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static char paths[64][256];
static int next_fd, writes, board_wrong, partition_wrong, write_short;
static int readback_wrong, link_failure, directory_failure, unrelated_only;
static int directory_reads;

static void reset(void)
{
    next_fd = 0;
    writes = board_wrong = partition_wrong = write_short = 0;
    readback_wrong = link_failure = directory_failure = unrelated_only = 0;
    directory_reads = 0;
}

static long reply(long buffer, long capacity, const char *value, size_t length)
{
    assert(length <= (size_t)capacity);
    memcpy((void *)buffer, value, length);
    return (long)length;
}

long test_syscall(long n, long a, long b, long c, long d, long e)
{
    (void)e;
    if (n == 56) {
        assert(++next_fd < 64);
        snprintf(paths[next_fd], sizeof(paths[next_fd]), "%s", (const char *)b);
        return next_fd;
    }
    if (n == 57)
        return 0;
    if (n == 63) {
        if (!strcmp(paths[a], "/proc/device-tree/compatible")) {
            const char value[] = "oplus,qqcandy\0mediatek,mt6895\0";
            if (board_wrong)
                return reply(b, c, "xiaomi,xaga", 12);
            return reply(b, c, value, sizeof(value));
        }
        if (strstr(paths[a], "/uevent")) {
            const char *value = partition_wrong ? "PARTNAME=boot_a\n" : "PARTNAME=userdata\n";
            return reply(b, c, value, strlen(value));
        }
        assert(strstr(paths[a], "/power/control"));
        return reply(b, c, readback_wrong ? "auto\n" : "on\n", readback_wrong ? 5 : 3);
    }
    if (n == 64) {
        assert(strstr(paths[a], "/power/control"));
        assert(c == 2 && !memcmp((void *)b, "on", 2));
        writes++;
        return write_short ? 1 : 2;
    }
    if (n == 217) {
        struct dirent64 *first = (void *)b;
        struct dirent64 *second = (void *)(b + 32);
        if (directory_failure)
            return -5;
        if (directory_reads++)
            return 0;
        memset((void *)b, 0, 64);
        first->d_reclen = second->d_reclen = 32;
        strcpy(first->d_name, "0:0:0:0");
        strcpy(second->d_name, "1:0:0:0");
        return 64;
    }
    if (n == 78) {
        const char *value = "../../../devices/platform/other/host1/1:0:0:0";
        if (link_failure)
            return -5;
        if (!unrelated_only && strstr((void *)b, "0:0:0:0"))
            value = "../../../devices/platform/112b0000.ufshci/host0/0:0:0:0";
        return reply(c, d, value, strlen(value));
    }
    assert(!"unexpected syscall");
    return -1;
}

int main(void)
{
    reset();
    assert(!keep_ufs_awake() && writes == 2);
    reset(); board_wrong = 1;
    assert(keep_ufs_awake() && writes == 0);
    reset(); partition_wrong = 1;
    assert(keep_ufs_awake() && writes == 0);
    reset(); write_short = 1;
    assert(keep_ufs_awake() && writes == 1);
    reset(); readback_wrong = 1;
    assert(keep_ufs_awake() && writes == 1);
    reset(); link_failure = 1;
    assert(keep_ufs_awake());
    reset(); directory_failure = 1;
    assert(keep_ufs_awake());
    reset(); unrelated_only = 1;
    assert(keep_ufs_awake() && writes == 1);
    puts("8 qqcandy power-policy cases passed; unrelated controller untouched");
    return 0;
}
