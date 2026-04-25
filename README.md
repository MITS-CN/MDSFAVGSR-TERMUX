# MDSFAVGSR-TERMUX

在 Termux 中模拟 Windows 命令行环境。

## 这是什么

一个运行于 Termux 的命令行工具集，提供 `diskpart`、`tasklist`、`taskkill`、`netsh`、`systeminfo` 等 Windows 风格命令的模拟实现。

同时包含一套自动化环境部署脚本，处理 Termux 的初始配置、依赖安装、插件部署和权限修复，适合开荒阶段使用。

## 安装

```bash
git clone https://github.com/MITS-CN/MDSFAVGSR-TERMUX.git
cd MDSFAVGSR-TERMUX
bash install.sh
```

## 已实现的功能

部分已支持的命令：

- `diskpart` — 磁盘信息查看
- `tasklist` — 进程列表
- `taskkill` — 终止进程
- `netsh` — 网络配置信息
- `systeminfo` — 系统信息概览
- `ver` / `version` / `winver` — 版本信息
- `help` — 帮助信息
- `cls`、`dir`、`del` 等基础命令的 Shell 封装

更多细节见 `data/bin/cpp/` 和 `data/bin/order/` 目录。

## 环境要求

- Android 7.0 及以上
- Termux（最新版）
- 网络连接（安装过程中需下载依赖）
- 存储权限

## 当前状态

项目处于活跃开发中，功能可能随时调整。尚未发布正式 Release 版本。

## 许可

本项目未指定许可证。使用前请自行评估风险。