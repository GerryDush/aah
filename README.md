# aaa

English | [中文](README.zh-CN.md)

[![Build and Release](https://github.com/GerryDush/aah/actions/workflows/build-release.yml/badge.svg)](https://github.com/GerryDush/aah/actions/workflows/build-release.yml)

`aaa` is a small command-line SSH server manager written in C. It turns a complete SSH command such as `ssh -p 2222 admin@192.168.1.100` into a short, memorable command such as `aaa production`.

## Overview

When you work with several SSH servers, remembering every host, user, and port quickly becomes inconvenient. `aaa` keeps these connection details under a name of your choice. The usual workflow is:

1. Add a server once with `aaa add`.
2. View saved servers with `aaa list`.
3. Connect later using its name, such as `aaa production`, or its list index, such as `aaa 1`.

`aaa` delegates the actual connection to the system `ssh` command, so your existing SSH keys, agent, and `~/.ssh/config` continue to work. It can also request an HTTP URL and obtain a dynamic SSH host and port from the URL's redirect target.

## Features

- Add or update SSH servers with a host, port, and user.
- Optionally save a password for automatic password authentication.
- Resolve an SSH endpoint from an HTTP redirect URL.
- Connect by server name or numeric index.
- Use full commands or single-letter aliases.
- Store connection details in `~/.aah/servers`.
- Support IPv4, hostnames, and IPv6 addresses.
- Build on Windows, macOS, and Linux.

## Requirements

- A C11 compiler
- CMake 4.2 or later
- `ssh`
- `curl` when using redirect URLs

## Build

```sh
cmake -S . -B build
cmake --build build
```

The executable is generated at `build/aaa` on macOS and Linux. With the default Visual Studio generator on Windows, it is generated at `build/Release/aaa.exe`.

The examples below assume that `aaa` is available in your `PATH`.

## Quick start

```sh
# Save a server
aaa add production --user admin --host 192.168.1.100 --port 2222

# See all saved servers
aaa list

# Connect by name
aaa production
```

After adding the server, the last command runs the equivalent of:

```sh
ssh -p 2222 admin@192.168.1.100
```

## Usage examples

Add a server using a host and port:

```sh
aaa add production --user admin --host 192.168.1.100 --port 2222
```

The default user is `root`, and the default port is `22` when a host is provided:

```sh
aaa add production --host example.com
```

Add a server using a redirect URL:

```sh
aaa add dynamic --user admin --url https://ssh.example.com
```

Add a server with password authentication:

```sh
aaa add legacy --user admin --host example.com --password 'your-password'
```

When a saved password is present, `aaa` uses its built-in OpenSSH AskPass helper to provide it to `ssh` automatically; no `sshpass` installation is required. To remove a saved password, update the server with an empty value: `--password ''`.

List configured servers:

```sh
aaa list
```

Connect by name or index:

```sh
aaa connect production
aaa production
aaa connect 1
aaa 1
```

Remove a server by name or index:

```sh
aaa remove production
aaa remove 1
```

Show help:

```sh
aaa help
```

### Short command aliases

Every command can be shortened to its first letter:

```sh
# add
aaa a production --user admin --host example.com --port 22 --password 'your-password'

# list
aaa l

# connect
aaa c production

# remove
aaa r production
aaa r 1

# help
aaa h
```

Connecting is even shorter because the `connect` command can be omitted:

```sh
aaa production
aaa 1
```

## Configuration

Server data is stored locally in:

```text
~/.aah/servers
```

When a redirect URL is configured, `aaa` requests the URL before connecting and extracts the SSH host and port from its redirect target. If the request fails, the most recently resolved host and port are used as a fallback when available.

### Password security

Passwords are stored as plain text in `~/.aah/servers`. The configuration directory and file are restricted to the current user with permissions `0700` and `0600`, but SSH keys are still recommended whenever possible. Supplying a password on the command line may also save it in your shell history.

On Windows, the configuration is stored under `%USERPROFILE%/.aah/servers` when `HOME` is unavailable. The POSIX `0700` and `0600` permission modes apply only to macOS and Linux.

## Automated releases

GitHub Actions builds and tests the project on Windows, macOS, and Linux for every push to `main` and every pull request. Push a version tag to create a GitHub Release containing archives for all three platforms and a `SHA256SUMS.txt` file:

```sh
git tag v0.1.0
git push origin v0.1.0
```
