# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-04-09

### Changed
- **Hook 回调从 after 迁移至 before** — 在 `uname` 系统调用执行前直接拦截并返回伪造数据（`skip_origin=1`），完全阻止真实调用执行，消除 TOCTOU 竞态条件
- **新增模块描述实时更新** — 通过运行时定位 `.kpm.info` section 中的 description 字段并原地覆写，APatch/FolkPatch 卡片可即时反映当前伪装状态
- **参数格式简化** — 从 `set release=<str> time=<str>` 改为 `set R:<release> T:<time>`，输入更精简

### Fixed
- 修复 APatch/FolkPatch 模块卡片描述首字符丢失问题 — `DESC_TAG_LEN` 从 11 修正为 12
