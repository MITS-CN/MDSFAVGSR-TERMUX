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