#include <compiler.h>
#include <kpmodule.h>
#include <common.h>
#include <hook.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <syscall.h>
#include <kputils.h>
#include <linux/kernel.h>
#include <uapi/asm-generic/unistd.h>

KPM_NAME("MyUname");
KPM_VERSION("0.2.0");
KPM_LICENSE("AGPLv3");
KPM_AUTHOR("时汐安");
KPM_DESCRIPTION("Spoof uname release and version");

#define FIELD_LEN       65
#define RESP_BUF_SIZE   256
#define UTSNAME_SIZE    (FIELD_LEN * 6)
#define UTS_NS_SCAN_RANGE 128
#define SCAN_RANGE      16384
#define NAME_TAG        "name="
#define NAME_PATTERN     NAME_TAG "MyUname"
#define DESC_TAG        "description="
#define DESC_TAG_LEN    12

struct new_utsname {
	char sysname[FIELD_LEN];
	char nodename[FIELD_LEN];
	char release[FIELD_LEN];
	char version[FIELD_LEN];
	char machine[FIELD_LEN];
	char domainname[FIELD_LEN];
};

static char fake_release[FIELD_LEN];
static char fake_version[FIELD_LEN];
static int  fake_active;

static void *uts_ns = NULL;
static int  uts_name_offset = 0;

static char *desc_data = NULL;
static int  desc_cap = 0;

#ifdef MYUNAME_DEBUG
static char prch(unsigned char c) { return (c >= 0x20 && c < 0x7f) ? c : '.'; }
#endif

static void locate_desc(void)
{
	uintptr_t anchor = (uintptr_t)&fake_release[0];
	uintptr_t lo = (anchor > SCAN_RANGE) ? anchor - SCAN_RANGE : 0;
	uintptr_t p;
	size_t nplen = sizeof(NAME_PATTERN) - 1;

	for (p = anchor; p > lo; p--) {
		if (memcmp((void *)p, NAME_PATTERN, nplen) == 0) {
			goto found_name;
		}
	}
	logke("[MyUname] cannot locate .kpm.info");
	return;

found_name:;
	uintptr_t p2;

	for (p2 = p; p2 > lo + DESC_TAG_LEN; p2--) {
		if (memcmp((void *)(p2 - DESC_TAG_LEN), DESC_TAG, DESC_TAG_LEN) == 0) {
			desc_data = (char *)p2;
			char *next = desc_data;
			while (*next && next < desc_data + KPM_DESCRIPTION_LEN)
				next++;
			desc_cap = (int)(next - desc_data);
			if (desc_cap <= 0)
				desc_cap = KPM_DESCRIPTION_LEN;
			logkd("[MyUname] desc cap=%d @%llx", desc_cap,
				(unsigned long long)desc_data);
			return;
		}
	}
	logke("[MyUname] desc tag not found before name");
}

static void update_desc(void)
{
	int n;

	if (!desc_data || desc_cap <= 0)
		return;

	memset(desc_data, 0, desc_cap);

	if (fake_active) {
		int has_rel = (fake_release[0] != '\0');
		int has_ver = (fake_version[0] != '\0');

		if (has_rel && has_ver)
			n = snprintf(desc_data, desc_cap,
				"R:\xe2\x9c\x85 T:\xe2\x9c\x85");
		else if (has_rel)
			n = snprintf(desc_data, desc_cap,
				"R:\xe2\x9c\x85");
		else if (has_ver)
			n = snprintf(desc_data, desc_cap,
				"T:\xe2\x9c\x85");
		else
			n = snprintf(desc_data, desc_cap, "(active)");
	} else {
		n = snprintf(desc_data, desc_cap, "(idle)");
	}

	if (n >= desc_cap)
		n = desc_cap - 1;
	if (n >= 0)
		desc_data[n] = '\0';
	logkd("[MyUname] wrote desc='%s' len=%d cap=%d",
		desc_data, n, desc_cap);

#ifdef MYUNAME_DEBUG
	{
		char *s = desc_data - 32;
		int i;
		logkd("[MyUname] === DUMP .kpm.info (%llx) ===",
			(unsigned long long)desc_data);
		for (i = 0; i < 128; i += 16) {
			logkd("[MyUname] +%03d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x |%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c|",
				i,
				(unsigned char)s[i+0], (unsigned char)s[i+1],
				(unsigned char)s[i+2], (unsigned char)s[i+3],
				(unsigned char)s[i+4], (unsigned char)s[i+5],
				(unsigned char)s[i+6], (unsigned char)s[i+7],
				(unsigned char)s[i+8], (unsigned char)s[i+9],
				(unsigned char)s[i+10], (unsigned char)s[i+11],
				(unsigned char)s[i+12], (unsigned char)s[i+13],
				(unsigned char)s[i+14], (unsigned char)s[i+15],
				prch(s[i+0]), prch(s[i+1]),
				prch(s[i+2]), prch(s[i+3]),
				prch(s[i+4]), prch(s[i+5]),
				prch(s[i+6]), prch(s[i+7]),
				prch(s[i+8]), prch(s[i+9]),
				prch(s[i+10]), prch(s[i+11]),
				prch(s[i+12]), prch(s[i+13]),
				prch(s[i+14]), prch(s[i+15]));
		}

		s = desc_data;
		logkd("[MyUname] --- kernel get_modinfo sim ---");
		{
			char *base = s;
			while (base > s - 256 && memcmp(base, "name=", 5))
				base--;
			unsigned long sz = 512;
			char *p = base;
			char *found = NULL;
			const char tag[] = "description";
			int tlen = 11, step = 0;
			for (; p && sz > 0; step++) {
				logkd("[MyUname] step%d @%llx '%.20s'",
					step, (unsigned long long)p, p);
				if (strncmp(p, tag, tlen) == 0 && p[tlen] == '=') {
					found = p + tlen + 1;
					logkd("[MyUname] FOUND val='%s' @%llx off=%lld",
						found, (unsigned long long)found,
						(long long)(found - desc_data));
					break;
				}
				while (*p) { p++; if (--sz <= 0) break; }
				if (sz <= 0) break;
				while (!*p) { p++; if (--sz <= 0) break; }
			}
			if (!found)
				logke("[MyUname] NOT FOUND after %d steps", step);
		}
	}
#endif
}

static long mu_ctl(const char *args, char *__user out_msg, int outlen)
{
	char resp[RESP_BUF_SIZE];
	int pos;

	if (!args || !*args) {
		pos = snprintf(resp, RESP_BUF_SIZE, "unknown cmd");
		goto out;
	}

	if (!strcmp(args, "status")) {
		pos = snprintf(resp, RESP_BUF_SIZE,
			"active=%d release='%s' version='%s' desc=%s",
			fake_active,
			fake_active && fake_release[0] ? fake_release : "(real)",
			fake_active && fake_version[0] ? fake_version : "(real)",
			desc_data ? "ok" : "n/a");
		goto out;
	}

	if (!strcmp(args, "clear")) {
		memset(fake_release, 0, sizeof(fake_release));
		memset(fake_version, 0, sizeof(fake_version));
		fake_active = 0;
		update_desc();
		pos = snprintf(resp, RESP_BUF_SIZE, "cleared");
		goto out;
	}

	if (!strncmp(args, "set ", 4) || !strncmp(args, "set\t", 4)) {
		const char *p = args + 4;
		while (*p == ' ' || *p == '\t') p++;
		if (!*p) {
			pos = snprintf(resp, RESP_BUF_SIZE,
				"error: usage: set R:<release> T:<version>");
			goto out;
		}

		char tmp_rel[FIELD_LEN] = {0};
		char tmp_ver[FIELD_LEN] = {0};
		int has_rel = 0, has_ver = 0;

		while (*p) {
			if ((*p == 'R' || *p == 'T') && *(p + 1) == ':') {
				int is_rel = (*p == 'R');
				p += 2;
				char *dst = is_rel ? tmp_rel : tmp_ver;
				int vlen = 0;
				while (*p && !((*p == 'R' || *p == 'T') && *(p + 1) == ':')) {
					if (vlen < FIELD_LEN - 1)
						dst[vlen++] = *p;
					p++;
				}
				dst[vlen] = '\0';
				if (is_rel) has_rel = 1; else has_ver = 1;
			} else {
				pos = snprintf(resp, RESP_BUF_SIZE,
					"error: bad token '%c', use R:<rel> T:<ver>", *p);
				goto out;
			}
		}

		if (!has_rel && !has_ver) {
			pos = snprintf(resp, RESP_BUF_SIZE,
				"error: no value specified");
			goto out;
		}

		if (has_rel)
			memcpy(fake_release, tmp_rel, strlen(tmp_rel) + 1);
		if (has_ver)
			memcpy(fake_version, tmp_ver, strlen(tmp_ver) + 1);
		fake_active = 1;
		update_desc();

		pos = snprintf(resp, RESP_BUF_SIZE,
			"ok release='%s' version='%s'",
			has_rel ? fake_release : "(unchanged)",
			has_ver ? fake_version : "(unchanged)");
		goto out;
	}

	pos = snprintf(resp, RESP_BUF_SIZE, "unknown cmd: %s", args);
out:
	if (pos >= RESP_BUF_SIZE)
		pos = RESP_BUF_SIZE - 1;
	if (out_msg && outlen > 0)
		compat_copy_to_user(out_msg, resp, pos + 1);
	return 0;
}

static void before_uname(hook_fargs1_t *args, void *udata)
{
	struct new_utsname tmp;
	void __user *ubuf;
	int ret;

	(void)udata;
	if (!fake_active)
		return;

	ubuf = (void __user *)syscall_argn((hook_fargs0_t *)args, 0);
	if (!ubuf)
		return;

	memcpy(&tmp, (char *)uts_ns + uts_name_offset, UTSNAME_SIZE);

	if (fake_release[0])
		memcpy(tmp.release, fake_release, strlen(fake_release) + 1);
	if (fake_version[0])
		memcpy(tmp.version, fake_version, strlen(fake_version) + 1);

	ret = compat_copy_to_user(ubuf, &tmp, UTSNAME_SIZE);
	args->ret = ret > 0 ? 0 : -EFAULT;
	args->skip_origin = 1;
}

static long mu_init(const char *args, const char *event, void *__user reserved)
{
	hook_err_t err;
	int i;

	(void)event;
	(void)reserved;

	logkd("[MyUname] INIT kpver=0x%x kver=0x%x", kpver, kver);

	locate_desc();

	uts_ns = (void *)kallsyms_lookup_name("init_uts_ns");
	if (!uts_ns) {
		logke("[MyUname] init_uts_ns not found");
		return -ENOSYS;
	}

	for (i = 0; i < UTS_NS_SCAN_RANGE; i++) {
		if (memcmp((char *)uts_ns + i, "Linux", 6) == 0) {
			uts_name_offset = i;
			break;
		}
	}

	if (memcmp((char *)uts_ns + uts_name_offset, "Linux", 6) != 0) {
		logke("[MyUname] cannot find utsname in init_uts_ns");
		uts_ns = NULL;
		return -ENOSYS;
	}

	logkd("[MyUname] uts_name_offset=%d", uts_name_offset);

	if (args && args[0])
		mu_ctl(args, NULL, 0);

	err = hook_syscalln(__NR_uname, 1, before_uname, NULL, NULL);
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

	fake_active = 0;
	unhook_syscalln(__NR_uname, before_uname, NULL);

	memset(fake_release, 0, sizeof(fake_release));
	memset(fake_version, 0, sizeof(fake_version));
	uts_ns = NULL;
	uts_name_offset = 0;
	desc_data = NULL;
	desc_cap = 0;
	logkd("[MyUname] EXIT");
	return 0;
}

KPM_INIT(mu_init);
KPM_CTL0(mu_ctl);
KPM_EXIT(mu_exit);
