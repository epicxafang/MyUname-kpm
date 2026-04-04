#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <hook.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <syscall.h>
#include <kputils.h>
#include <linux/kernel.h>
#include <uapi/asm-generic/unistd.h>

KPM_NAME("MyUname");
KPM_VERSION("0.1.0");
KPM_LICENSE("AGPLv3");
KPM_AUTHOR("时汐安");
KPM_DESCRIPTION("Spoof uname release and version");

#define FIELD_LEN       65
#define RESP_BUF_SIZE   256
#define UTSNAME_SIZE    (FIELD_LEN * 6)
#define OFF_RELEASE     (FIELD_LEN * 2)
#define OFF_VERSION     (FIELD_LEN * 3)

static char fake_release[FIELD_LEN];
static char fake_version[FIELD_LEN];
static int  fake_active;

static void (*do_kfree)(const void *) = NULL;
static void *(*do_memdup_user)(const void __user *, size_t) = NULL;

static long mu_ctl(const char *args, char *__user out_msg, int outlen)
{
    char resp[RESP_BUF_SIZE];
    int pos;

    if (!args || !*args) {
        pos = snprintf(resp, RESP_BUF_SIZE,
            "cmds: set|clear|status\n"
            "  set <release> <version>\n"
            "  clear\n"
            "  status");
        goto out;
    }

    if (!strcmp(args, "status")) {
        pos = snprintf(resp, RESP_BUF_SIZE, "active=%d release='%s' version='%s'",
                       fake_active,
                       fake_active ? fake_release : "(real)",
                       fake_active ? fake_version : "(real)");
        goto out;
    }

    if (!strcmp(args, "clear")) {
        memset(fake_release, 0, sizeof(fake_release));
        memset(fake_version, 0, sizeof(fake_version));
        fake_active = 0;
        pos = snprintf(resp, RESP_BUF_SIZE, "cleared");
        goto out;
    }

    if (!strncmp(args, "set ", 4)) {
        const char *rel = args + 4;
        while (*rel == ' ' || *rel == '\t') rel++;
        const char *sep = strchr(rel, ' ');
        if (!sep || sep == rel) {
            pos = snprintf(resp, RESP_BUF_SIZE, "error: bad args");
            goto out;
        }
        const char *ver = sep + 1;
        while (*ver == ' ' || *ver == '\t') ver++;

        int rlen = (int)(sep - rel);
        if (rlen >= FIELD_LEN) {
            pos = snprintf(resp, RESP_BUF_SIZE, "error: release too long");
            goto out;
        }
        memcpy(fake_release, rel, rlen);
        fake_release[rlen] = '\0';
        strncpy(fake_version, ver, FIELD_LEN - 1);
        fake_version[FIELD_LEN - 1] = '\0';
        fake_active = 1;

        pos = snprintf(resp, RESP_BUF_SIZE, "ok release='%s' version='%s'",
                       fake_release, fake_version);
        goto out;
    }

    pos = snprintf(resp, RESP_BUF_SIZE, "unknown cmd: %s", args);
out:
    if (out_msg && outlen > 0)
        compat_copy_to_user(out_msg, resp, pos + 1);
    return 0;
}

static void after_uname(hook_fargs1_t *args, void *udata)
{
    char *kbuf;
    void __user *ubuf;

    (void)udata;
    if (!fake_active || (long)args->ret < 0) return;

    ubuf = (void __user *)syscall_argn((hook_fargs0_t *)args, 0);
    if (!ubuf) return;

    kbuf = do_memdup_user(ubuf, UTSNAME_SIZE);
    if (IS_ERR(kbuf)) return;

    if (fake_release[0])
        strncpy(kbuf + OFF_RELEASE, fake_release, FIELD_LEN - 1);
    if (fake_version[0])
        strncpy(kbuf + OFF_VERSION, fake_version, FIELD_LEN - 1);

    compat_copy_to_user(ubuf, kbuf, UTSNAME_SIZE);
    do_kfree(kbuf);
}

static long mu_init(const char *args, const char *event, void *__user reserved)
{
    hook_err_t err;

    (void)event;
    (void)reserved;

    logkd("[MyUname] INIT kpver=0x%x kver=0x%x", kpver, kver);

    do_kfree = (void *)kallsyms_lookup_name("kfree");
    do_memdup_user = (void *)kallsyms_lookup_name("memdup_user");
    if (!do_kfree || !do_memdup_user) {
        logke("[MyUname] kfree/memdup_user not found");
        return -ENOSYS;
    }

    if (args && args[0])
        mu_ctl(args, NULL, 0);

    err = hook_syscalln(__NR_uname, 1, NULL, after_uname, NULL);
    if (err) {
        logke("[MyUname] hook uname failed: %d", err);
        return err;
    }

    logkd("[MyUname] READY");
    return 0;
}

static long mu_exit(void *__user reserved)
{
    (void)reserved;
    unhook_syscalln(__NR_uname, NULL, after_uname);
    memset(fake_release, 0, sizeof(fake_release));
    memset(fake_version, 0, sizeof(fake_version));
    fake_active = 0;
    logkd("[MyUname] EXIT");
    return 0;
}

KPM_INIT(mu_init);
KPM_CTL0(mu_ctl);
KPM_EXIT(mu_exit);
