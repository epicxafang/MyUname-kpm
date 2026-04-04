# MyUname-kpm

基于 [KernelPatch](https://github.com/bmax121/KernelPatch) 的 KPM，用于伪造 `uname` 返回的内核版本号和构建时间。

## 技术原理

MyUname 通过 **syscall hook** 拦截 `uname` 系统调用，在原始结果返回后替换 `release` 和 `version` 字段：

```
用户空间调用 uname(buf)
    ↓
内核 uname syscall 执行，填充 struct new_utsname
    ↓
[after 回调] 从用户空间拷回 buf → 替换 release / version 字段 → 写回用户空间
    ↓
调用方收到伪造后的版本信息
```

## 使用方法

```bash
# 加载并设置伪造值（release + version）
sc_kpm_load key ./MyUname.kpm "set 5.15.0 #1-SMP Mon Apr  4 12:00:00 UTC 2026"

# 运行时动态修改
sc_kpm_control key "MyUname" "set 4.19.0 #1-LTS"
# 返回: "ok release='4.19.0' version='#1-LTS'"

# 查看当前状态
sc_kpm_control key "MyUname" "status"
# 返回: "active=1 release='4.19.0' version='#1-LTS'"

# 恢复真实值
sc_kpm_control key "MyUname" "clear"

# 卸载
sc_kpm_unload key "MyUname"
```

## 构建

```bash
make
```

产物: `MyUname.kpm`

## 许可证

[AGPLv3](LICENSE)
