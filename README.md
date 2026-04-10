# MyUname-kpm

基于 [KernelPatch](https://github.com/bmax121/KernelPatch) 的 KPM，用于伪造 `uname` 返回的内核版本号和构建时间。

## 使用方法

### 图形界面（APatch / FolkPatch）

在模块卡片的**控制（齿轮图标）**中输入命令：

| 命令 | 说明 |
|------|------|
| `set R:5.15.0 T:#1-SMP Mon Apr 4 12:00:00 UTC 2026` | 设置版本号和构建时间 |
| `set R:6.6.666` / `set T:Mon Jan 1 ...` | 仅设置其中一项 |
| `hook` | 切换至 hook 模式（默认，拦截 syscall） |
| `write` | 切换至 memwrite 模式（直接覆写内核内存） |
| `status` | 查看当前状态和模式（`dmesg \| grep MyUname`） |
| `clear` | 清除伪装，恢复真实信息 |

可随时修改，无需重启，卡片描述实时更新。

### 命令行

```bash
sc_kpm_load key ./MyUname.kpm "set R:5.15.0 T:#1-SMP Mon Apr 4 12:00:00 UTC 2026"
sc_kpm_control key "MyUname" "set R:4.19.0"
sc_kpm_control key "MyUname" "write"       # 切换至 memwrite 模式
sc_kpm_control key "MyUname" "hook"        # 切回 hook 模式
sc_kpm_control key "MyUname" "clear"
```

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
