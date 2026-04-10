# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-04-10

### Added
- **双模式支持** — 新增 `hook`（默认）和 `write`（memwrite）两种工作模式，可通过命令随时切换
  - `hook` 模式：通过 syscall before-hook 拦截 uname，返回伪造数据，内核数据源不受影响
  - `write` 模式：直接覆写 `init_uts_ns` 内的 `release`/`version` 内存，所有读取路径均可见伪造值
- **复合命令链** — 所有命令支持尾部串联执行，如 `write set R:5.0 T:#1 SMP`、`clear write set R:6.0`、`set R:5.0 clear status`
- **原始值自动备份** — 首次进入 memwrite 模式写入时自动保存原始值，`clear` 或卸载模块时自动恢复，避免内核状态残留
- **模式标签** — 模块描述实时显示当前模式：`[hook]` 或 `[mem]`

### Changed
- **值解析器重写** — 以 `R:` / `T:` 前缀作为分隔符（替代空格），值内可包含空格（如构建时间字符串）
- **空格自动裁剪** — 值的前导和尾部空白字符自动去除
- **容错提升** — 值后的未知字符静默停止解析（不再报 bad token 错误），支持 `set R:5.0 clear` 等写法
- **参数解析安全化** — 命令参数先复制到栈缓冲区再解析，避免修改 `const char *` 的未定义行为
- **memwrite 空值处理** — 设为空串时清零对应字段（而非无操作）
- **重复注册防护** — 已处于 hook 模式时再次输入 `hook` 不触发重复注册
- **`status` 输出新增当前模式字段**

### Removed
- 调试模式下 `.kpm.info` 十六进制转储代码块

## [0.2.0] - 2026-04-10

### Changed
- **Hook 回调从 after 迁移至 before** — 在 `uname` 系统调用执行前直接拦截并返回伪造数据（`skip_origin=1`），完全阻止真实调用执行，消除 TOCTOU 竞态条件
- **新增模块描述实时更新** — 通过运行时定位 `.kpm.info` section 中的 description 字段并原地覆写，APatch/FolkPatch 卡片可即时反映当前伪装状态
- **参数格式简化** — 从 `set release=<str> time=<str>` 改为 `set R:<release> T:<time>`，输入更精简
