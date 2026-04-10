# MyUname-kpm

基于 [KernelPatch](https://github.com/bmax121/KernelPatch) 的 KPM，用于伪造 `uname` 返回的内核版本号和构建时间。

## 使用方法

### 图形界面（APatch / FolkPatch）

在模块卡片的**控制（齿轮图标）**中输入命令：

| 命令 | 说明 |
|------|------|
| `set R:5.15.0 T:#1-SMP Mon Apr 4 12:00:00 UTC 2026` | 设置版本号和构建时间 |
| `set R:6.6.666` / `set T:Mon Jan 1 ...` | 仅设置其中一项 |
| `hook` / `write` | 切换模式（默认 hook，见下方说明） |
| `status` | 查看当前状态和模式（`dmesg \| grep MyUname`） |
| `clear` | 清除伪装，恢复真实信息 |

### 复合命令

命令可串联执行，例如：

| 命令 | 效果 |
|------|------|
| `write set R:6.0 T:#1 SMP` | 切换到 memwrite 并设置值 |
| `set R:5.0 clear status` | 设值 → 立即清除 → 查看状态 |
| `clear write set R:9.9` | 清除后切换到 memwrite 设新值 |

可随时修改，无需重启，卡片描述实时更新。

### 命令行

```bash
sc_kpm_load key ./MyUname.kpm "write set R:5.15.0 T:#1-SMP Mon Apr 4 12:00:00 UTC 2026"
sc_kpm_control key "MyUname" "set R:4.19.0"
sc_kpm_control key "MyUname" "write"       # 切换至 memwrite 模式
sc_kpm_control key "MyUname" "hook"        # 切回 hook 模式
sc_kpm_control key "MyUname" "clear"
```

## 模式说明

| 模式 | 原理 | 优点 | 缺点 |
|------|------|------|------|
| **hook**（默认） | 拦截 `uname` syscall，返回伪造数据 | 安全隔离，内核数据不受影响，极端值无风险 | 微小性能开销（纳秒级，不可感知） |
| **write** (memwrite) | 直接覆写 `init_uts_ns` 内存 | 零调用开销，`/proc/sys/kernel/osrelease` 等路径也被欺骗 | 极端值可能影响内核内部逻辑；仅影响初始 UTS namespace |

> 默认使用 hook 模式即可满足绝大多数场景。仅在需要欺骗非 syscall 路径时使用 write 模式。

## 注意事项

> 推荐使用嵌入模式并传入初始参数。`uname` 返回值常被应用启动时缓存，中途加载模块仅对后续调用生效。

## 构建

```bash
make          # Release
make debug    # Debug（含 MYUNAME_DEBUG 诊断日志）
```

产物: `MyUname.kpm`

## 许可证

[AGPLv3](LICENSE)
