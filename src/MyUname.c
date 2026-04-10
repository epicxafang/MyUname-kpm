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
KPM_VERSION("0.3.0");
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

enum { MODE_HOOK, MODE_MEMWRITE };

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
static char orig_release[FIELD_LEN];
static char orig_version[FIELD_LEN];
static int  fake_active;
static int  orig_saved;
static int  cur_mode = MODE_HOOK;

static void *uts_ns = NULL;
static int  uts_name_offset = 0;

static char *release_ptr;
static char *version_ptr;

static char *desc_data = NULL;
static int  desc_cap = 0;

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
		const char *tag = (cur_mode == MODE_MEMWRITE) ? "[mem]" : "[hook]";

		if (has_rel && has_ver)
			n = snprintf(desc_data, desc_cap,
				"R:\xe2\x9c\x85 T:\xe2\x9c\x85 %s", tag);
		else if (has_rel)
			n = snprintf(desc_data, desc_cap,
				"R:\xe2\x9c\x85 %s", tag);
		else if (has_ver)
			n = snprintf(desc_data, desc_cap,
				"T:\xe2\x9c\x85 %s", tag);
		else
			n = snprintf(desc_data, desc_cap, "(active) %s", tag);
	} else {
		n = snprintf(desc_data, desc_cap, "(idle)");
	}

	logkd("[MyUname] wrote desc='%s' len=%d cap=%d",
		desc_data, n, desc_cap);
}

static void apply_memwrite(void)
{
	if (!uts_ns || !fake_active)
		return;

	if (fake_release[0])
		memcpy(release_ptr, fake_release, strlen(fake_release) + 1);
	else
		release_ptr[0] = '\0';

	if (fake_version[0])
		memcpy(version_ptr, fake_version, strlen(fake_version) + 1);
	else
		version_ptr[0] = '\0';

	logki("[MyUname] memwritten release='%s' version='%s'",
		fake_release, fake_version);
}

static void restore_original(void)
{
	if (!uts_ns || !orig_saved)
		return;

	memcpy(release_ptr, orig_release, FIELD_LEN);
	memcpy(version_ptr, orig_version, FIELD_LEN);

	logki("[MyUname] restored original release='%s' version='%s'",
		orig_release, orig_version);
}

static void before_uname(hook_fargs1_t *args, void *udata);

static void switch_to_hook(void)
{
	if (cur_mode == MODE_HOOK)
		return;

	if (fake_active && orig_saved)
		restore_original();

	cur_mode = MODE_HOOK;
	logki("[MyUname] mode -> hook");
}

static void switch_to_memwrite(void)
{
	if (cur_mode == MODE_MEMWRITE)
		return;

	unhook_syscalln(__NR_uname, before_uname, NULL);

	if (fake_active) {
		if (!orig_saved) {
			memcpy(orig_release, release_ptr, FIELD_LEN);
			memcpy(orig_version, version_ptr, FIELD_LEN);
			orig_saved = 1;
			logki("[MyUname] original saved: release='%s' version='%s'",
				orig_release, orig_version);
		}
		apply_memwrite();
	}

	cur_mode = MODE_MEMWRITE;
	logki("[MyUname] mode -> memwrite");
}

static void before_uname(hook_fargs1_t *args, void *udata)
{
	struct new_utsname tmp;
	void __user *ubuf;
	int ret;

	(void)udata;
	if (!fake_active || cur_mode != MODE_HOOK)
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

static long mu_ctl(const char *args, char *__user out_msg, int outlen)
{
	char resp[RESP_BUF_SIZE];
	char buf[RESP_BUF_SIZE];
	int pos;
	char *cmd;
	char *rest;

	if (!args || !*args) {
		pos = snprintf(resp, RESP_BUF_SIZE, "unknown cmd");
		goto out;
	}

	strncpy(buf, args, RESP_BUF_SIZE - 1);
	buf[RESP_BUF_SIZE - 1] = '\0';

	cmd = buf;
	while (*cmd == ' ' || *cmd == '\t') cmd++;
	rest = cmd;
	while (*rest && *rest != ' ' && *rest != '\t') rest++;
	if (*rest) {
		*rest++ = '\0';
		while (*rest == ' ' || *rest == '\t') rest++;
	} else {
		rest = NULL;
	}

	if (!strcmp(cmd, "status")) {
		pos = snprintf(resp, RESP_BUF_SIZE,
			"active=%d mode=%s release='%s' version='%s' desc=%s",
			fake_active,
			cur_mode == MODE_HOOK ? "hook" : "memwrite",
			fake_active && fake_release[0] ? fake_release : "(real)",
			fake_active && fake_version[0] ? fake_version : "(real)",
			desc_data ? "ok" : "n/a");
		if (rest && rest[0])
			goto run_rest;
		goto out;
	}

	if (!strcmp(cmd, "clear")) {
		if (cur_mode == MODE_MEMWRITE && fake_active && orig_saved)
			restore_original();
		fake_active = 0;
		memset(fake_release, 0, sizeof(fake_release));
		memset(fake_version, 0, sizeof(fake_version));
		update_desc();
		pos = snprintf(resp, RESP_BUF_SIZE, "cleared");
		if (rest && rest[0])
			goto run_rest;
		goto out;
	}

	if (!strcmp(cmd, "hook")) {
		hook_err_t err;
		int was_memwrite = (cur_mode == MODE_MEMWRITE);
		switch_to_hook();
		if (was_memwrite) {
			err = hook_syscalln(__NR_uname, 1, before_uname, NULL, NULL);
			if (err)
				logke("[MyUname] hook uname failed: %d", err);
		}
		if (rest && rest[0]) {
			mu_ctl(rest, out_msg, outlen);
			return 0;
		}
		update_desc();
		pos = snprintf(resp, RESP_BUF_SIZE, "ok mode=hook");
		goto out;
	}

	if (!strcmp(cmd, "write")) {
		switch_to_memwrite();
		if (rest && rest[0]) {
			mu_ctl(rest, out_msg, outlen);
			return 0;
		}
		update_desc();
		pos = snprintf(resp, RESP_BUF_SIZE, "ok mode=memwrite");
		goto out;
	}

	if (!strcmp(cmd, "set")) {
		const char *p = rest;

		if (!p || !*p) {
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

				while (*p == ' ' || *p == '\t')
					p++;
				while (*p && !((*p == 'R' || *p == 'T') && *(p + 1) == ':')) {
					if (vlen < FIELD_LEN - 1)
						dst[vlen++] = *p;
					p++;
				}
				while (vlen > 0 && (dst[vlen - 1] == ' ' || dst[vlen - 1] == '\t'))
					vlen--;
				dst[vlen] = '\0';
				if (is_rel) has_rel = 1; else has_ver = 1;
			} else {
				break;
			}
		}

		if (!has_rel && !has_ver) {
			pos = snprintf(resp, RESP_BUF_SIZE,
				"error: no value specified");
			goto out;
		}

		if (cur_mode == MODE_MEMWRITE && !orig_saved) {
			memcpy(orig_release, release_ptr, FIELD_LEN);
			memcpy(orig_version, version_ptr, FIELD_LEN);
			orig_saved = 1;
			logki("[MyUname] original saved: release='%s' version='%s'",
				orig_release, orig_version);
		}

		if (has_rel)
			memcpy(fake_release, tmp_rel, strlen(tmp_rel) + 1);
		if (has_ver)
			memcpy(fake_version, tmp_ver, strlen(tmp_ver) + 1);
		fake_active = 1;

		if (cur_mode == MODE_MEMWRITE)
			apply_memwrite();

		update_desc();

		pos = snprintf(resp, RESP_BUF_SIZE,
			"ok release='%s' version='%s' mode=%s",
			has_rel ? fake_release : "(unchanged)",
			has_ver ? fake_version : "(unchanged)",
			cur_mode == MODE_HOOK ? "hook" : "memwrite");

		while (*p == ' ' || *p == '\t') p++;
		if (*p) {
			rest = (char *)p;
			goto run_rest;
		}
		goto out;
	}

	pos = snprintf(resp, RESP_BUF_SIZE, "unknown cmd: %s", args);
	goto out;

run_rest:
	if (rest && rest[0]) {
		mu_ctl(rest, out_msg, outlen);
		return 0;
	}
out:
	if (pos >= RESP_BUF_SIZE)
		pos = RESP_BUF_SIZE - 1;
	if (out_msg && outlen > 0)
		compat_copy_to_user(out_msg, resp, pos + 1);
	return 0;
}

static long mu_init(const char *args, const char *event, void *__user reserved)
{
	hook_err_t err;
	int i;

	(void)event;
	(void)reserved;

	logkd("[MyUname] INIT kpver=0x%x kver=0x%x mode=hook (default)", kpver, kver);

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

	release_ptr = (char *)uts_ns + uts_name_offset +
		__builtin_offsetof(struct new_utsname, release);
	version_ptr = (char *)uts_ns + uts_name_offset +
		__builtin_offsetof(struct new_utsname, version);

	logkd("[MyUname] uts_name_offset=%d release@%llx version@%llx",
		uts_name_offset,
		(unsigned long long)release_ptr,
		(unsigned long long)version_ptr);

	err = hook_syscalln(__NR_uname, 1, before_uname, NULL, NULL);
	if (err) {
		logke("[MyUname] hook uname failed: %d", err);
		return err;
	}

	if (args && args[0])
		mu_ctl(args, NULL, 0);

	logkd("[MyUname] READY (default: hook mode)");
	return 0;
}

static long mu_exit(void *__user reserved)
{
	(void)reserved;

	if (cur_mode == MODE_MEMWRITE && fake_active && orig_saved)
		restore_original();
	else if (cur_mode == MODE_HOOK)
		unhook_syscalln(__NR_uname, before_uname, NULL);

	fake_active = 0;
	orig_saved = 0;
	cur_mode = MODE_HOOK;
	memset(fake_release, 0, sizeof(fake_release));
	memset(fake_version, 0, sizeof(fake_version));
	memset(orig_release, 0, sizeof(orig_release));
	memset(orig_version, 0, sizeof(orig_version));
	uts_ns = NULL;
	uts_name_offset = 0;
	release_ptr = NULL;
	version_ptr = NULL;
	desc_data = NULL;
	desc_cap = 0;
	logkd("[MyUname] EXIT");
	return 0;
}

KPM_INIT(mu_init);
KPM_CTL0(mu_ctl);
KPM_EXIT(mu_exit);
