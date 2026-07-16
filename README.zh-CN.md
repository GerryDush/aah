# aaa

[English](README.md) | 中文

[![构建与发布](https://github.com/GerryDush/aah/actions/workflows/build-release.yml/badge.svg)](https://github.com/GerryDush/aah/actions/workflows/build-release.yml)

`aaa` 是一个使用 C 编写的轻量级 SSH 服务器管理命令行工具。它可以把 `ssh -p 2222 admin@192.168.1.100` 这样的完整 SSH 命令，简化成 `aaa production` 这样简短、容易记忆的命令。

## 项目简介

管理多台 SSH 服务器时，逐一记住主机、用户名和端口并不方便。`aaa` 可以用自定义名称保存这些连接信息。通常只需要：

1. 使用 `aaa add` 添加一次服务器。
2. 使用 `aaa list` 查看已保存的服务器。
3. 以后通过名称连接，例如 `aaa production`；也可以通过列表序号连接，例如 `aaa 1`。

`aaa` 会调用系统的 `ssh` 命令完成实际连接，因此现有的 SSH 密钥、SSH Agent 和 `~/.ssh/config` 仍然有效。它还可以请求 HTTP URL，并从 URL 的重定向目标中动态获取 SSH 主机和端口。

## 功能

- 使用主机、端口和用户名添加或更新 SSH 服务器。
- 可选保存密码，实现自动密码认证。
- 从 HTTP 重定向 URL 动态解析 SSH 连接地址。
- 通过服务器名称或数字序号连接。
- 支持完整命令和单字母缩写。
- 将连接信息保存在 `~/.aah/servers`。
- 支持 IPv4、主机名和 IPv6 地址。
- 支持在 Windows、macOS 和 Linux 上构建。

## 环境要求

- 支持 C11 的编译器
- CMake 4.2 或更高版本
- `ssh`
- 使用重定向 URL 时需要 `curl`

## 构建

```sh
cmake -S . -B build
cmake --build build
```

在 macOS 和 Linux 上，生成的可执行文件位于 `build/aaa`。Windows 使用默认 Visual Studio 生成器时，可执行文件位于 `build/Release/aaa.exe`。

以下示例假设 `aaa` 已添加到 `PATH` 环境变量中。

## 快速开始

```sh
# 保存服务器
aaa add production --user admin --host 192.168.1.100 --port 2222

# 查看全部已保存的服务器
aaa list

# 通过名称连接
aaa production
```

添加服务器后，最后一条命令相当于执行：

```sh
ssh -p 2222 admin@192.168.1.100
```

## 使用示例

使用主机和端口添加服务器：

```sh
aaa add production --user admin --host 192.168.1.100 --port 2222
```

指定主机时，默认用户为 `root`，默认端口为 `22`：

```sh
aaa add production --host example.com
```

使用重定向 URL 添加服务器：

```sh
aaa add dynamic --user admin --url https://ssh.example.com
```

添加使用密码认证的服务器：

```sh
aaa add legacy --user admin --host example.com --password 'your-password'
```

保存密码后，`aaa` 会通过内置的 OpenSSH AskPass 助手自动将密码提供给 `ssh`，不需要安装 `sshpass`。如需删除已保存的密码，可在更新服务器时传入空值：`--password ''`。

列出已配置的服务器：

```sh
aaa list
```

通过名称或序号连接：

```sh
aaa connect production
aaa production
aaa connect 1
aaa 1
```

通过名称或序号删除服务器：

```sh
aaa remove production
aaa remove 1
```

显示帮助信息：

```sh
aaa help
```

### 首字母简写

每个命令都可以简写为首字母：

```sh
# 添加
aaa a production --user admin --host example.com --port 22 --password 'your-password'

# 列表
aaa l

# 连接
aaa c production

# 删除
aaa r production
aaa r 1

# 帮助
aaa h
```

连接时还可以省略 `connect` 命令，进一步简化：

```sh
aaa production
aaa 1
```

## 配置文件

服务器数据保存在本地文件：

```text
~/.aah/servers
```

配置重定向 URL 后，`aaa` 会在连接前请求该 URL，并从重定向目标中提取 SSH 主机和端口。如果请求失败，并且之前已成功解析过地址，则会使用最近保存的主机和端口作为备用连接地址。

### 密码安全

密码以明文形式保存在 `~/.aah/servers` 中。配置目录和文件的权限会分别限制为当前用户可访问的 `0700` 和 `0600`，但仍建议尽可能使用 SSH 密钥。通过命令行输入密码还可能使密码保留在 Shell 历史记录中。

Windows 无法读取 `HOME` 时，配置文件保存在 `%USERPROFILE%/.aah/servers`。POSIX 的 `0700` 和 `0600` 权限模式仅适用于 macOS 和 Linux。

## 自动发布

每次推送到 `main` 或创建 Pull Request 时，GitHub Actions 都会在 Windows、macOS 和 Linux 上构建并测试项目。推送版本标签后，会自动创建 GitHub Release，上传三个平台的压缩包以及 `SHA256SUMS.txt` 校验文件：

```sh
git tag v0.1.0
git push origin v0.1.0
```
