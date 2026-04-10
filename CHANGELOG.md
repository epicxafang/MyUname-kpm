# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-04-10

### Added
- **双模式运行** — 新增 `hook`（syscall 拦截）和 `write`（直接覆写内核内存）两种模式，通过 `hook` / `write` 命令切换；模块描述实时显示当前模式标签 `[hook]` / `[mem]`

### Changed
- **命令分发重构为表驱动架构** — `mu_ctl` 从扁平 if-else 链改为 `cmd_table[]` 分发表，各命令独立为 `cmd_*` handler 函数，新增命令只需添加一条表项
- **提取公共逻辑消除重复** — `ensure_orig_saved()` 统一管理原始值备份，`parse_set_values()` 独立 R:/T: 参数解析，`parse_cmd()` 封装命令词法分析
- **`switch_to_hook()` 自包含化** — 内置 `hook_syscalln()` 安装，与 `switch_to_memwrite()` 完全对称
- **`clear` 命令** — memwrite 模式下先恢复原始值再清除参数
- **`status` 输出增强** — 新增 `mode=hook/memwrite` 字段

## [0.2.0] - 2026-04-10

### Changed
- **Hook 回调从 after 迁移至 before** — 在 `uname` 系统调用执行前直接拦截并返回伪造数据（`skip_origin=1`），完全阻止真实调用执行，消除 TOCTOU 竞态条件
- **新增模块描述实时更新** — 通过运行时定位 `.kpm.info` section 中的 description 字段并原地覆写，APatch/FolkPatch 卡片可即时反映当前伪装状态
- **参数格式简化** — 从 `set release=<str> time=<str>` 改为 `set R:<release> T:<time>`，输入更精简
