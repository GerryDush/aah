#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include <time.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#define MAX_NAME    64
#define MAX_USER    64
#define MAX_HOST    256
#define MAX_URL     512
#define MAX_PASSWORD 256
#define MAX_TIME    32
#define MAX_LINE    2048
#define MAX_EXEC_PATH 4096

typedef struct server {
    char name[MAX_NAME];
    char user[MAX_USER];
    char host[MAX_HOST];
    int  port;
    char url[MAX_URL];
    char password[MAX_PASSWORD];
    char created_at[MAX_TIME];
    char last_connected[MAX_TIME];
    struct server *next;
} server_t;

server_t *head = NULL;
static char executable_path[MAX_EXEC_PATH];

void init_executable_path(const char *argv0) {
    char raw_path[MAX_EXEC_PATH];
    raw_path[0] = '\0';

#ifdef __APPLE__
    uint32_t size = sizeof(raw_path);
    if (_NSGetExecutablePath(raw_path, &size) != 0)
        raw_path[0] = '\0';
#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", raw_path, sizeof(raw_path) - 1);
    if (len >= 0)
        raw_path[len] = '\0';
#endif

    if (raw_path[0] == '\0' && argv0)
        strncpy(raw_path, argv0, sizeof(raw_path) - 1);
    raw_path[sizeof(raw_path) - 1] = '\0';

    if (!realpath(raw_path, executable_path))
        executable_path[0] = '\0';
}

int is_password_prompt(const char *prompt) {
    const char keyword[] = "password";
    if (!prompt) return 0;

    for (; *prompt; prompt++) {
        size_t i = 0;
        while (keyword[i] && prompt[i] &&
               tolower((unsigned char)prompt[i]) == keyword[i]) {
            i++;
        }
        if (keyword[i] == '\0') return 1;
    }
    return 0;
}

int run_askpass(int argc, char **argv) {
    const char *password = getenv("AAA_PASSWORD");
    const char *prompt = NULL;
    if (argc >= 3 && strcmp(argv[1], "--askpass") == 0)
        prompt = argv[2];
    else if (argc >= 2)
        prompt = argv[1];
    if (!password || !is_password_prompt(prompt)) return 1;

    printf("%s\n", password);
    return 0;
}

/* ---------- 时间工具 ---------- */
void get_current_time(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
}

void read_config(void) {
    char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.aah/servers", home);
    FILE *fp = fopen(path, "r");
    if (!fp) return;

    char line[MAX_LINE];
    server_t *tail = NULL;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        server_t *s = calloc(1, sizeof(server_t));
        char *ptr = line;
        char *fields[8];
        int i = 0;
        // 用 strsep 拆分，保留空字段
        while (i < 8 && (fields[i] = strsep(&ptr, "|")) != NULL) {
            i++;
        }
        // 如果字段数不足，补默认值
        if (i >= 1) strncpy(s->name, fields[0], MAX_NAME - 1);
        if (i >= 2) strncpy(s->user, fields[1], MAX_USER - 1); else strcpy(s->user, "root");
        if (i >= 3) strncpy(s->host, fields[2], MAX_HOST - 1);
        if (i >= 4) s->port = atoi(fields[3]);
        if (i >= 5) strncpy(s->url, fields[4], MAX_URL - 1);
        if (i >= 6) strncpy(s->created_at, fields[5], MAX_TIME - 1); else strcpy(s->created_at, "Unknown");
        if (i >= 7) strncpy(s->last_connected, fields[6], MAX_TIME - 1); else strcpy(s->last_connected, "Never");
        if (i >= 8) strncpy(s->password, fields[7], MAX_PASSWORD - 1);

        // 保留配置文件中的顺序，避免每次写回时反转服务器列表。
        if (tail) {
            tail->next = s;
        } else {
            head = s;
        }
        tail = s;
    }
    fclose(fp);
}

void write_config(void) {
    char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.aah/servers", home);
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.aah", home);
    mkdir(dir, 0700);
    chmod(dir, 0700);

    mode_t old_umask = umask(0077);
    FILE *fp = fopen(path, "w");
    umask(old_umask);
    if (!fp) { perror("fopen"); return; }
    chmod(path, 0600);
    server_t *cur = head;
    while (cur) {
        fprintf(fp, "%s|%s|%s|%d|%s|%s|%s|%s\n",
                cur->name, cur->user, cur->host, cur->port, cur->url,
                cur->created_at, cur->last_connected, cur->password);
        cur = cur->next;
    }
    fclose(fp);
}

server_t *find_server(const char *name) {
    server_t *cur = head;
    while (cur) {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

server_t *find_server_by_index(int index) {
    server_t *cur = head;
    int i = 1;
    while (cur) {
        if (i == index) return cur;
        i++;
        cur = cur->next;
    }
    return NULL;
}

void add_server(const char *name, const char *user, const char *host, int port,
                const char *url, const char *password) {
    server_t *exist = find_server(name);
    if (exist) {
        if (user) strncpy(exist->user, user, MAX_USER - 1);
        if (host) strncpy(exist->host, host, MAX_HOST - 1);
        if (port > 0) exist->port = port;
        if (url) strncpy(exist->url, url, MAX_URL - 1);
        if (password) {
            strncpy(exist->password, password, MAX_PASSWORD - 1);
            exist->password[MAX_PASSWORD - 1] = '\0';
        }
        return;
    }
    server_t *s = calloc(1, sizeof(server_t));
    strncpy(s->name, name, MAX_NAME - 1);
    strncpy(s->user, user ? user : "root", MAX_USER - 1);
    if (host) strncpy(s->host, host, MAX_HOST - 1);
    if (port > 0) s->port = port;
    if (url) strncpy(s->url, url, MAX_URL - 1);
    if (password) strncpy(s->password, password, MAX_PASSWORD - 1);
    get_current_time(s->created_at, MAX_TIME);
    strcpy(s->last_connected, "Never");
    if (!head) {
        head = s;
    } else {
        server_t *tail = head;
        while (tail->next)
            tail = tail->next;
        tail->next = s;
    }
}

void remove_server(const char *name) {
    server_t *cur = head, *prev = NULL;
    while (cur) {
        if (strcmp(cur->name, name) == 0) {
            if (prev) prev->next = cur->next;
            else head = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    fprintf(stderr, "Server '%s' not found\n", name);
}

void list_servers(void) {
    server_t *cur = head;
    if (!cur) { printf("No servers configured.\n"); return; }
    printf("%-4s %-20s %-8s %-20s %-8s %-20s %-20s %s\n",
           "No.", "Name", "User", "Host", "Port", "Created", "Last Connected", "URL");
    printf("------------------------------------------------------------------------------------------------------------------------\n");
    int idx = 1;
    while (cur) {
        printf("%-4d %-20s %-8s %-20s %-8d %-20s %-20s %s\n",
               idx++, cur->name, cur->user, cur->host, cur->port,
               cur->created_at, cur->last_connected, cur->url);
        cur = cur->next;
    }
}

/* ---------- 工具函数 ---------- */
char *trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

char *get_redirect_url(const char *url) {
    static char redir[MAX_URL];
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -s -o /dev/null -w \"%%{redirect_url}\" \"%s\"", url);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    if (fgets(redir, sizeof(redir), fp)) {
        size_t len = strlen(redir);
        while (len > 0 && (redir[len-1] == '\n' || redir[len-1] == '\r' || redir[len-1] == ' ')) {
            redir[--len] = '\0';
        }
        if (redir[0] == '\0') {
            pclose(fp);
            return NULL;
        }
    } else {
        redir[0] = '\0';
    }
    pclose(fp);
    return redir[0] ? redir : NULL;
}

int parse_url(const char *url, char *host, int *port) {
    while (*url == ' ' || *url == '\t' || *url == '\r' || *url == '\n') url++;
    if (*url == '\0') return -1;

    const char *p = strstr(url, "://");
    if (!p) return -1;
    p += 3;

    const char *end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;

    char buf[MAX_HOST + 16];
    int len = end - p;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
    strncpy(buf, p, len);
    buf[len] = '\0';

    if (buf[0] == '[') {
        char *closing = strchr(buf, ']');
        if (!closing) return -1;
        *closing = '\0';
        strncpy(host, buf + 1, MAX_HOST - 1);
        char *port_ptr = closing + 1;
        if (*port_ptr == ':') {
            *port = atoi(port_ptr + 1);
        } else {
            *port = 22;
        }
        return 0;
    }

    char *colon = strrchr(buf, ':');
    if (colon) {
        *colon = '\0';
        strncpy(host, buf, MAX_HOST - 1);
        *port = atoi(colon + 1);
    } else {
        strncpy(host, buf, MAX_HOST - 1);
        *port = 22;
    }
    return 0;
}

int is_ipv6(const char *host) {
    return (strchr(host, ':') != NULL && host[0] != '[');
}

void format_host(char *buf, const char *host) {
    if (is_ipv6(host)) {
        snprintf(buf, MAX_HOST + 2, "[%s]", host);
    } else {
        strncpy(buf, host, MAX_HOST);
    }
}

/* ---------- 连接服务器 ---------- */
void connect_server(const char *name) {
    server_t *s = find_server(name);
    if (!s) {
        fprintf(stderr, "Server '%s' not found\n", name);
        return;
    }

    char host[MAX_HOST];
    int port;

    if (s->url[0] != '\0') {
        char *redir = get_redirect_url(s->url);
        if (!redir) {
            fprintf(stderr, "Failed to get redirect from URL: %s\n", s->url);
            if (s->host[0] != '\0') {
                strncpy(host, s->host, MAX_HOST);
                port = s->port;
                fprintf(stderr, "Using stored host/port as fallback.\n");
            } else {
                fprintf(stderr, "No fallback host/port provided.\n");
                return;
            }
        } else {
            if (parse_url(redir, host, &port) != 0) {
                fprintf(stderr, "Failed to parse redirect URL: '%s'\n", redir);
                return;
            }

            // 保存本次解析结果用于列表展示和失败兜底；后续连接仍会优先请求 URL。
            strncpy(s->host, host, MAX_HOST - 1);
            s->host[MAX_HOST - 1] = '\0';
            s->port = port;
        }
    } else {
        if (s->host[0] == '\0') {
            fprintf(stderr, "Server '%s' has no host or URL configured.\n", name);
            return;
        }
        strncpy(host, s->host, MAX_HOST);
        port = s->port;
    }

    if (s->password[0] != '\0' && executable_path[0] == '\0') {
        fprintf(stderr, "Cannot determine the aaa executable path for password authentication.\n");
        return;
    }

    // 更新上次连接时间
    get_current_time(s->last_connected, MAX_TIME);
    write_config();

    char formatted_host[MAX_HOST + 2];
    format_host(formatted_host, host);

    char user[MAX_USER];
    strncpy(user, s->user[0] ? s->user : "root", MAX_USER - 1);

    char port_string[16];
    char target[MAX_USER + MAX_HOST + 3];
    snprintf(port_string, sizeof(port_string), "%d", port);
    snprintf(target, sizeof(target), "%s@%s", user, formatted_host);

    printf("Connecting to %s as %s (%s:%d)\n", name, user, host, port);
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        if (s->password[0] != '\0') {
            setenv("AAA_PASSWORD", s->password, 1);
            setenv("AAA_ASKPASS", "1", 1);
            setenv("SSH_ASKPASS", executable_path, 1);
            setenv("SSH_ASKPASS_REQUIRE", "force", 1);
            setenv("DISPLAY", "aaa-askpass", 0);
            execlp("ssh", "ssh",
                   "-o", "PreferredAuthentications=keyboard-interactive,password",
                   "-o", "NumberOfPasswordPrompts=1",
                   "-p", port_string, target, (char *)NULL);
            perror("ssh");
        } else {
            execlp("ssh", "ssh", "-p", port_string, target, (char *)NULL);
            perror("ssh");
        }
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
    } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        fprintf(stderr, "SSH connection failed (exit code %d)\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "SSH connection terminated by signal %d\n", WTERMSIG(status));
    }
}

void print_usage(const char *prog) {
    printf("Usage (full or single-letter):\n");
    printf("  %s add | a <name> [--user <user>] [--host <host>] [--port <port>] [--url <url>] [--password <password>]\n", prog);
    printf("  %s remove | r <name|index>\n", prog);
    printf("  %s list | l\n", prog);
    printf("  %s connect | c <name|index>   (or just %s <name|index>)\n", prog, prog);
    printf("  %s help | h\n", prog);
    printf("\nExamples:\n");
    printf("  %s add myserver --user admin --host 192.168.1.100 --port 2222 --password secret\n", prog);
    printf("  %s add myserver --url https://ssh.example.com\n", prog);
    printf("  %s list\n", prog);
    printf("  %s connect myserver (connects to server @myserver)\n", prog);
    printf("  %s myserver (connects to server @myserver)\n", prog);
    printf("  %s connect 1 (connects to server #1)\n", prog);
    printf("  %s 1   (connects to server #1)\n", prog);
}

int parse_number(const char *str) {
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0' || val <= 0) return -1;
    return (int)val;
}

/* ---------- 主程序 ---------- */
int main(int argc, char **argv) {
    init_executable_path(argv[0]);

    const char *askpass_mode = getenv("AAA_ASKPASS");
    if ((askpass_mode && strcmp(askpass_mode, "1") == 0) ||
        (argc >= 2 && strcmp(argv[1], "--askpass") == 0))
        return run_askpass(argc, argv);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    read_config();

    const char *cmd = argv[1];
    int cmd_type = 0; // 1=add, 2=remove, 3=list, 4=connect, 5=help

    // 匹配全名或首字母
    if (strcmp(cmd, "add") == 0 || strcmp(cmd, "a") == 0)
        cmd_type = 1;
    else if (strcmp(cmd, "remove") == 0 || strcmp(cmd, "r") == 0)
        cmd_type = 2;
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0)
        cmd_type = 3;
    else if (strcmp(cmd, "connect") == 0 || strcmp(cmd, "c") == 0)
        cmd_type = 4;
    else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "--help") == 0)
        cmd_type = 5;

    if (cmd_type == 1) { // add
        if (argc < 3) {
            fprintf(stderr, "Error: missing name for add command.\n");
            return 1;
        }
        const char *name = argv[2];
        const char *user = "root";
        const char *host = NULL;
        int port = 0;
        const char *url = NULL;
        const char *password = NULL;

        int opt;
        int opt_index = 0;
        static struct option long_opts[] = {
            {"user", required_argument, 0, 'u'},
            {"host", required_argument, 0, 'h'},
            {"port", required_argument, 0, 'p'},
            {"url",  required_argument, 0, 'r'},
            {"password", required_argument, 0, 'w'},
            {0, 0, 0, 0}
        };
        optind = 3;
        while ((opt = getopt_long(argc, argv, "u:h:p:r:w:", long_opts, &opt_index)) != -1) {
            switch (opt) {
                case 'u': user = optarg; break;
                case 'h': host = optarg; break;
                case 'p': port = atoi(optarg); break;
                case 'r': url = optarg; break;
                case 'w': password = optarg; break;
                default: print_usage(argv[0]); return 1;
            }
        }

        if (!host && !url) {
            fprintf(stderr, "Error: must provide either --host or --url.\n");
            return 1;
        }
        if (host && port == 0) port = 22;
        if (password && (strchr(password, '|') || strchr(password, '\n') || strchr(password, '\r'))) {
            fprintf(stderr, "Error: password cannot contain '|', newline, or carriage return.\n");
            return 1;
        }
        if (password && password[0] != '\0' && executable_path[0] == '\0') {
            fprintf(stderr, "Error: cannot determine the aaa executable path for password authentication.\n");
            return 1;
        }

        add_server(name, user, host, port, url, password);
        write_config();
        printf("Server '%s' added/updated.\n", name);

    } else if (cmd_type == 2) { // remove
        if (argc < 3) {
            fprintf(stderr, "Error: missing name or index for remove command.\n");
            return 1;
        }
        const char *arg = argv[2];
        int idx = parse_number(arg);
        if (idx > 0) {
            server_t *s = find_server_by_index(idx);
            if (s) remove_server(s->name);
            else fprintf(stderr, "No server at index %d\n", idx);
        } else {
            remove_server(arg);
        }
        write_config();

    } else if (cmd_type == 3) { // list
        list_servers();

    } else if (cmd_type == 4) { // connect
        if (argc < 3) {
            fprintf(stderr, "Error: missing name or index for connect.\n");
            return 1;
        }
        const char *arg = argv[2];
        int idx = parse_number(arg);
        if (idx > 0) {
            server_t *s = find_server_by_index(idx);
            if (s) connect_server(s->name);
            else fprintf(stderr, "No server at index %d\n", idx);
        } else {
            connect_server(arg);
        }

    } else if (cmd_type == 5) { // help
        print_usage(argv[0]);

    } else {
        // 隐式连接：可能是数字序号或服务器名称
        int idx = parse_number(cmd);
        if (idx > 0) {
            server_t *s = find_server_by_index(idx);
            if (s) connect_server(s->name);
            else fprintf(stderr, "No server at index %d\n", idx);
        } else {
            connect_server(cmd);
        }
    }

    return 0;
}
