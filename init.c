/*
 * XAGA no-libc C init - minimal:
 *   1. mount devtmpfs/proc/sysfs
 *   2. list disks (/dev/sd*)
 *   3. mount nvdata (read-only), copy the WiFi NVRAM blob into the rootfs
 *   4. mount BOOT_PARTITION and switch_root to /sbin/init
 *
 * Build:
 *   aarch64-linux-gnu-gcc -nostdlib -static -fno-stack-protector \
 *       -fno-builtin -ffreestanding -Os -o root/init init.c
 *
 * aarch64 syscall numbers:
 *   mount=40 umount2=39 openat=56 close=57 read=63 write=64 mkdirat=34
 *   chdir=49 chroot=51 nanosleep=101 getdents64=217 execve=221
 */

#ifndef BOOT_PARTITION
#define BOOT_PARTITION "/dev/sdc86"
#endif

/* nvdata partition on xaga (mt6895), see /dev/disk/by-partlabel/nvdata */
#ifndef NVDATA_PARTITION
#define NVDATA_PARTITION "/dev/sdc13"
#endif

/* WiFi NVRAM blob lives on nvdata; the driver requests it from
 * /lib/firmware/mediatek/mt6895/WIFI on the rootfs.
 */
#define NVDATA_WIFI_SRC "/nvdata/APCFG/APRDEB/WIFI"
#define WIFI_DST_INITRAMFS "/lib/firmware/mediatek/mt6895/WIFI"
#define WIFI_DST_ROOTFS "/newroot/lib/firmware/mediatek/mt6895/WIFI"

/*
 * Firmware that is requested after switch_root and so must also be present on
 * the real rootfs (not just the initramfs tmpfs):
 *  - WiFi/connectivity blobs loaded by the deferred mt6895 connectivity
 *    power-on,
 *  - Novatek touchscreen firmware needed at runtime resume.
 * They all ship in the initramfs /lib/firmware; mirror them onto rootfs.
 */
static const char *const ROOTFS_FW_FILES[] = {
    "BT_FW.cfg",
    "soc7_0_ram_bt_1b_t_1_hdr.bin",
    "soc7_0_ram_wmmcu_1b_t_1_hdr.bin",
    "wifi.cfg",
    "conninfra.cfg",
    "soc7_0_ram_mcu_1b_t_1_hdr.bin",
    "WIFI_RAM_CODE_soc7_0_1b_t_1.bin",
    "novatek_nt36672e_l16_fw01.bin",
    "novatek_nt36672e_l16_fw02.bin",
};

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define O_NONBLOCK 0x800

#define MS_RDONLY 1
#define MS_NOSUID 2
#define MS_NOEXEC 8
#define MS_MOVE 0x2000
#define MS_RELATIME 0x200000
#define MS_STRICTATIME 0x1000000

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct dirent64 {
    unsigned long long d_ino;
    long long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static long ksys(long n, long a, long b, long c, long d, long e)
{
    register long x0 asm("x0") = a;
    register long x1 asm("x1") = b;
    register long x2 asm("x2") = c;
    register long x3 asm("x3") = d;
    register long x4 asm("x4") = e;
    register long x8 asm("x8") = n;
    asm volatile("svc 0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x8)
                 : "memory");
    return x0;
}

static long mount_(const char *src, const char *tgt, const char *fstype,
                   unsigned long flags, const void *data)
{
    return ksys(40, (long)src, (long)tgt, (long)fstype, flags, (long)data);
}

static long umount2_(const char *tgt, long flags)
{
    return ksys(39, (long)tgt, flags, 0, 0, 0);
}

static long openat(long dirfd, const char *path, long flags, long mode)
{
    return ksys(56, dirfd, (long)path, flags, mode, 0);
}

static long close_(long fd)
{
    return ksys(57, fd, 0, 0, 0, 0);
}

static long read_(long fd, void *buf, unsigned long len)
{
    return ksys(63, fd, (long)buf, len, 0, 0);
}

static long write_(long fd, const void *buf, unsigned long len)
{
    return ksys(64, fd, (long)buf, len, 0, 0);
}

static long mkdirat(long dirfd, const char *path, long mode)
{
    return ksys(34, dirfd, (long)path, mode, 0, 0);
}

static long chdir_(const char *path)
{
    return ksys(49, (long)path, 0, 0, 0, 0);
}

static long chroot_(const char *path)
{
    return ksys(51, (long)path, 0, 0, 0, 0);
}

static long nanosleep(const struct timespec *req, struct timespec *rem)
{
    return ksys(101, (long)req, (long)rem, 0, 0, 0);
}

static long getdents64(long fd, void *buf, unsigned long len)
{
    return ksys(217, fd, (long)buf, len, 0, 0);
}

static long execve_(const char *path, const char **argv, const char **envp)
{
    return ksys(221, (long)path, (long)argv, (long)envp, 0, 0);
}

static long strlen_(const char *s)
{
    long n = 0;
    while (s[n])
        n++;
    return n;
}

/* Concatenate a + b into out (max bytes incl. NUL). */
static void concat(char *out, const char *a, const char *b, long max)
{
    long n = 0;
    long i;

    for (i = 0; a[i] && n < max - 1; i++)
        out[n++] = a[i];
    for (i = 0; b[i] && n < max - 1; i++)
        out[n++] = b[i];
    out[n] = '\0';
}

static int starts_with(const char *s, const char *prefix)
{
    int i;
    for (i = 0; prefix[i]; i++) {
        if (s[i] != prefix[i])
            return 0;
    }
    return 1;
}

static int exists(const char *path)
{
    long fd = openat(AT_FDCWD, path, O_RDONLY, 0);
    if (fd < 0)
        return 0;
    close_(fd);
    return 1;
}

static long g_kmsg_fd = -1;

static void kmsg(const char *s)
{
    if (g_kmsg_fd >= 0)
        write_(g_kmsg_fd, s, strlen_(s));
}

static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, 0);
}

/* Wait up to ~5s for a block device node to appear. */
static void wait_for_dev(const char *path)
{
    int i;

    for (i = 0; i < 50; i++) {
        if (exists(path))
            return;
        sleep_ms(100);
    }
}

/* Create every directory component of an absolute path (ignore EEXIST). */
static void mkdir_p(const char *path)
{
    char buf[256];
    long n = 0;
    long i;

    if (path[0] != '/')
        return;
    buf[n++] = '/';

    for (i = 1; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            if (n > 1) {
                buf[n] = '\0';
                mkdirat(AT_FDCWD, buf, 0755);
            }
            buf[n++] = path[i];  /* keep the separator */
            continue;
        }
        buf[n++] = path[i];
    }
    if (n > 1) {
        buf[n] = '\0';
        mkdirat(AT_FDCWD, buf, 0755);
    }
}

/* Copy one file. Returns 0 on success, nonzero on failure. */
static int copy_file(const char *src, const char *dst)
{
    unsigned char buf[4096];
    long fd_in, fd_out, got, w;

    fd_in = openat(AT_FDCWD, src, O_RDONLY, 0);
    if (fd_in < 0)
        return -1;

    fd_out = openat(AT_FDCWD, dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd_out < 0) {
        close_(fd_in);
        return -1;
    }

    while ((got = read_(fd_in, buf, sizeof(buf))) > 0) {
        long off = 0;
        while (off < got) {
            w = write_(fd_out, buf + off, got - off);
            if (w <= 0) {
                close_(fd_in);
                close_(fd_out);
                return -1;
            }
            off += w;
        }
    }

    close_(fd_in);
    close_(fd_out);
    return 0;
}

/*
 * Mount nvdata read-only and copy the WiFi NVRAM blob out of it.  The copy on
 * the mounted rootfs (/newroot) is the important one, because the WiFi driver
 * may load only after switch_root and the initramfs tmpfs is gone by then.
 * Also place it in the initramfs tree for any early retry before the pivot.
 */
static void copy_wifi_from_nvdata(void)
{
    kmsg("CINIT: nvdata: waiting for ");
    kmsg(NVDATA_PARTITION);
    kmsg("\n");
    wait_for_dev(NVDATA_PARTITION);

    mkdirat(AT_FDCWD, "/nvdata", 0755);
    if (mount_(NVDATA_PARTITION, "/nvdata", "ext4", MS_RDONLY, 0) != 0) {
        kmsg("CINIT: nvdata mount failed (continuing without WiFi NVRAM)\n");
        return;
    }

    if (!exists(NVDATA_WIFI_SRC)) {
        kmsg("CINIT: nvdata WIFI blob not found (continuing)\n");
    } else {
        mkdir_p("/newroot/lib/firmware/mediatek/mt6895");
        mkdir_p("/lib/firmware/mediatek/mt6895");
        if (copy_file(NVDATA_WIFI_SRC, WIFI_DST_ROOTFS) == 0)
            kmsg("CINIT: copied WiFi NVRAM to rootfs\n");
        else
            kmsg("CINIT: failed to copy WiFi NVRAM to rootfs\n");
        if (copy_file(NVDATA_WIFI_SRC, WIFI_DST_INITRAMFS) == 0)
            kmsg("CINIT: copied WiFi NVRAM to initramfs\n");
        else
            kmsg("CINIT: failed to copy WiFi NVRAM to initramfs\n");
    }

    umount2_("/nvdata", 0);
}

/*
 * Some firmware is only requested after switch_root (WiFi power-on, touch
 * runtime resume), so it must be on the real rootfs, not only in the
 * initramfs tmpfs.  Mirror the blobs from /lib/firmware (initramfs) to
 * /newroot/lib/firmware (rootfs).
 */
static void copy_firmware_to_rootfs(void)
{
    char src[128], dst[128], msg[160];
    int i, nfiles;

    mkdir_p("/newroot/lib/firmware");
    nfiles = (int)(sizeof(ROOTFS_FW_FILES) / sizeof(ROOTFS_FW_FILES[0]));

    for (i = 0; i < nfiles; i++) {
        concat(src, "/lib/firmware/", ROOTFS_FW_FILES[i], sizeof(src));
        concat(dst, "/newroot/lib/firmware/", ROOTFS_FW_FILES[i], sizeof(dst));

        concat(msg, "CINIT: copying fw ", ROOTFS_FW_FILES[i], sizeof(msg));
        kmsg(msg);

        if (copy_file(src, dst) == 0) {
            concat(msg, "CINIT: copied fw ", ROOTFS_FW_FILES[i], sizeof(msg));
            kmsg(msg);
        } else {
            concat(msg, "CINIT: failed fw ", ROOTFS_FW_FILES[i], sizeof(msg));
            kmsg(msg);
        }
    }

    kmsg("CINIT: firmware mirror done\n");
}

/* Wait up to ~5s for any /dev/sd* to appear, then list them. */
static void list_disks(void)
{
    unsigned char buf[4096];
    long fd, got, off;
    int i, count = 0;

    for (i = 0; i < 50; i++) {
        if (exists("/dev/sda") || exists("/dev/sdb") ||
            exists("/dev/sdc") || exists("/dev/sdd"))
            break;
        sleep_ms(100);
    }

    kmsg("CINIT: disks:\n");
    fd = openat(AT_FDCWD, "/dev", O_RDONLY, 0);
    if (fd < 0) {
        kmsg("CINIT: /dev open failed\n");
        return;
    }

    while ((got = getdents64(fd, buf, sizeof(buf))) > 0) {
        for (off = 0; off < got; off += ((struct dirent64 *)(buf + off))->d_reclen) {
            struct dirent64 *d = (struct dirent64 *)(buf + off);
            char line[64];
            long n = 0, j;

            if (d->d_reclen == 0)
                break;
            if (!starts_with(d->d_name, "sd"))
                continue;
            for (j = 0; d->d_name[j] && n < (long)sizeof(line) - 2; j++)
                line[n++] = d->d_name[j];
            line[n++] = '\n';
            kmsg("CINIT:   ");
            write_(g_kmsg_fd, line, n);
            count++;
        }
    }
    close_(fd);

    if (count == 0)
        kmsg("CINIT: no sd disks found\n");
}

/* Mount BOOT_PARTITION at /newroot, move it over /, and exec /sbin/init. */
static void boot_rootfs(void)
{
    const char *argv[2], *envp[3];

    kmsg("CINIT: mounting ");
    kmsg(BOOT_PARTITION);
    kmsg("\n");

    mkdirat(AT_FDCWD, "/newroot", 0755);
    if (mount_(BOOT_PARTITION, "/newroot", "ext4", 0, 0) != 0) {
        kmsg("CINIT: mount failed\n");
        for (;;)
            sleep_ms(60000);
    }

    /* The WiFi driver is loaded after switch_root, so both the NVRAM blob
     * and the connectivity firmware must be on the real rootfs (not the
     * initramfs tmpfs).  Copy them out now, before we pivot.
     */
    copy_wifi_from_nvdata();
    copy_firmware_to_rootfs();

    mkdirat(AT_FDCWD, "/newroot/proc", 0755);
    mount_("proc", "/newroot/proc", "proc", MS_NOSUID | MS_NOEXEC, 0);
    mkdirat(AT_FDCWD, "/newroot/sys", 0755);
    mount_("sysfs", "/newroot/sys", "sysfs", MS_NOSUID | MS_NOEXEC, 0);
    mkdirat(AT_FDCWD, "/newroot/dev", 0755);
    mount_("devtmpfs", "/newroot/dev", "devtmpfs",
           MS_NOSUID, "mode=0755");
    mkdirat(AT_FDCWD, "/newroot/run", 0755);
    mount_("tmpfs", "/newroot/run", "tmpfs",
           MS_NOSUID | MS_NOEXEC, 0);
    mkdirat(AT_FDCWD, "/newroot/tmp", 0755);
    mount_("tmpfs", "/newroot/tmp", "tmpfs",
           MS_NOSUID | MS_NOEXEC, 0);
    mkdirat(AT_FDCWD, "/newroot/dev/pts", 0755);
    mount_("devpts", "/newroot/dev/pts", "devpts",
           MS_NOSUID | MS_NOEXEC, 0);

    kmsg("CINIT: switch_root -> /sbin/init\n");
    chdir_("/newroot");
    mount_(".", "/", 0, MS_MOVE, 0);
    chroot_(".");
    chdir_("/");

    argv[0] = "/sbin/init";
    argv[1] = 0;
    envp[0] = "HOME=/";
    envp[1] = "TERM=linux";
    envp[2] = 0;
    execve_("/sbin/init", argv, envp);

    kmsg("CINIT: exec /sbin/init failed\n");
    for (;;)
        sleep_ms(60000);
}

void _start(void)
{
    mkdirat(AT_FDCWD, "/dev", 0755);
    mount_("devtmpfs", "/dev", "devtmpfs",
           MS_NOSUID | MS_STRICTATIME, "mode=0755");

    mkdirat(AT_FDCWD, "/proc", 0755);
    mount_("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC, 0);

    mkdirat(AT_FDCWD, "/sys", 0755);
    mount_("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC, 0);

    g_kmsg_fd = openat(AT_FDCWD, "/dev/kmsg", O_WRONLY, 0);
    if (g_kmsg_fd < 0)
        g_kmsg_fd = 0;

    kmsg("CINIT: minimal init start\n");

    list_disks();
    boot_rootfs();

    /* unreachable */
    for (;;)
        ;
}
