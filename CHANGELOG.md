# Changelog

All notable changes to this project will be documented in this file.

## [0.3.0] - 2026-04-10

### Added
- 新增 `hook` / `write` 命令，支持 hook 拦截和内核内存直写两种模式切换
- `status` 输出新增当前模式字段

### Changed
- 命令处理从 if-else 链改为表驱动（`cmd_table[]`），各命令拆分为独立 handler 函数
- 提取 `ensure_orig_saved()`、`parse_set_values()`、`parse_cmd()` 消除重复逻辑
- `switch_to_hook()` 内置 hook 安装，与 `switch_to_memwrite()` 对称
- `clear` 在 memwrite 模式下先恢复原始值

## [0.2.0] - 2026-04-10

### Changed
- **Hook 回调从 after 迁移至 before** — 在 `uname` 系统调用执行前直接拦截并返回伪造数据（`skip_origin=1`），完全阻止真实调用执行，消除 TOCTOU 竞态条件
- **新增模块描述实时更新** — 通过运行时定位 `.kpm.info` section 中的 description 字段并原地覆写，APatch/FolkPatch 卡片可即时反映当前伪装状态
- **参数格式简化** — 从 `set release=<str> time=<str>` 改为 `set R:<release> T:<time>`，输入更精简
