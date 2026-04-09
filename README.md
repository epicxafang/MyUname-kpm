# MyUname-kpm

基于 [KernelPatch](https://github.com/bmax121/KernelPatch) 的 KPM，用于伪造 `uname` 返回的内核版本号和构建时间。

## 技术原理

MyUname 通过 **before 回调** 拦截 `uname` 系统调用，在原始调用执行前直接向用户空间写入伪造数据并设置 `skip_origin=1`，完全阻止真实调用执行：

```
用户空间调用 uname(buf)
    ↓
[before 回调] 从 init_uts_ns 拷贝真实 utsname → 替换 release / version → compat_copy_to_user 写入 buf → skip_origin=1
    ↓
原始 syscall 被跳过
    ↓
调用方收到伪造后的版本信息
```

模块加载后通过运行时定位 `.kpm.info` section 中的 `description` 字段实现**描述实时更新**，APatch/FolkPatch 卡片即时反映当前伪装状态。

## 使用方法

### 图形界面（APatch / FolkPatch）

在模块卡片的**控制（齿轮图标）**中输入以下命令：

| 命令 | 说明 |
|------|------|
| `set R:5.15.0 T:#1-SMP Mon Apr 4 12:00:00 UTC 2026` | 同时设置版本号和构建时间 |
| `set R:6.6.666` | 仅设置版本号 |
| `set T:Mon Jan 1 00:00:00 UTC 2026` | 仅设置构建时间 |
| `status` | 查看当前伪装状态 |
| `clear` | 清除所有伪装值，恢复真实信息 |

可随时修改参数，无需重启。模块卡片上的描述会实时更新显示当前状态。

### 命令行

```bash
# 加载模块（可选传入初始参数）
sc_kpm_load key ./MyUname.kpm "set R:5.15.0 T:#1-SMP Mon Apr  4 12:00:00 UTC 2026"

# 运行时控制
sc_kpm_control key "MyUname" "set R:4.19.0"
sc_kpm_control key "MyUname" "status"
sc_kpm_control key "MyUname" "clear"
```

## 注意事项

> **推荐使用嵌入模式并传入初始参数。** `uname` 的返回值常被应用启动时缓存，若模块在系统运行中途加载，已缓存真实版本信息的应用将绕过伪装，仅对后续调用生效。

## 构建

```bash
make            # Release 构建
make debug      # Debug 构建（含 MYUNAME_DEBUG 诊断日志）
```

产物: `MyUname.kpm`

## 许可证

[AGPLv3](LICENSE)
