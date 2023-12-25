/*
 * WPA Supplicant - command line interface for wpa_supplicant daemon
 * Copyright (c) 2004-2011, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#ifdef CONFIG_CTRL_IFACE

#ifdef CONFIG_CTRL_IFACE_UNIX
#include <dirent.h>
#endif /* CONFIG_CTRL_IFACE_UNIX */

#include "wpa_cli_wrapper.h"
#include "common/wpa_ctrl.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/edit.h"
#include "common/version.h"
#ifdef ANDROID
#include <cutils/properties.h>
#endif /* ANDROID */


static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *mon_conn;
static int wpa_cli_quit = 0;
static int wpa_cli_attached = 0;
static int wpa_cli_connected = 0;
static int wpa_cli_last_id = 0;
#ifndef CONFIG_CTRL_IFACE_DIR
#define CONFIG_CTRL_IFACE_DIR "/var/run/wpa_supplicant"
#endif /* CONFIG_CTRL_IFACE_DIR */
static const char *ctrl_iface_dir = CONFIG_CTRL_IFACE_DIR;
static char *ctrl_ifname = NULL;
static const char *pid_file = NULL;
static const char *action_file = NULL;
static int interactive = 0;

static void wpa_cli_mon_receive(int sock, void *eloop_ctx, void *sock_ctx);

static void wpa_cli_msg_cb(char *msg, size_t len)
{
    printf("%s\n", msg);
}

static int _wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd, int print, char *reply)
{
    //Modified by Kurt; enlarge command string for scan_result
    char buf[4096];
    size_t len;
    int ret;

    if (ctrl_conn == NULL) {
        printf("Not connected to wpa_supplicant - command dropped.\n");
        return -1;
    }
    len = sizeof(buf) - 1;
    ret = wpa_ctrl_request(ctrl, cmd, os_strlen(cmd), buf, &len,
                   wpa_cli_msg_cb);
    if (ret == -2) {
        printf("'%s' command timed out.\n", cmd);
        return -2;
    } else if (ret < 0) {
        printf("'%s' command failed.\n", cmd);
        return -1;
    }
    if (print) {
        buf[len] = '\0';
        printf("%s", buf);
        if (interactive && len > 0 && buf[len - 1] != '\n')
            printf("\n");
    }
    strncpy(reply, buf, len);
    reply[len] = 0;

    return 0;
}

static int wpa_ctrl_command(struct wpa_ctrl *ctrl, char *cmd, char *reply)
{
    return _wpa_ctrl_command(ctrl, cmd, 1, reply);
}

static int str_starts(const char *src, const char *match)
{
    return os_strncmp(src, match, os_strlen(match)) == 0;
}

static int str_match(const char *a, const char *b)
{
    return os_strncmp(a, b, os_strlen(b)) == 0;
}

static int wpa_cli_exec(const char *program, const char *arg1,
            const char *arg2)
{
    char *cmd;
    size_t len;
    int res;
    int ret = 0;

    len = os_strlen(program) + os_strlen(arg1) + os_strlen(arg2) + 3;
    cmd = os_malloc(len);
    if (cmd == NULL)
        return -1;
    res = os_snprintf(cmd, len, "%s %s %s", program, arg1, arg2);
    if (res < 0 || (size_t) res >= len) {
        os_free(cmd);
        return -1;
    }
    cmd[len - 1] = '\0';
#ifndef _WIN32_WCE
    if (system(cmd) < 0)
        ret = -1;
#endif /* _WIN32_WCE */
    os_free(cmd);

    return ret;
}

static int wpa_cli_show_event(const char *event)
{
    const char *start;

    start = os_strchr(event, '>');
    if (start == NULL)
        return 1;

    start++;
    /*
     * Skip BSS added/removed events since they can be relatively frequent
     * and are likely of not much use for an interactive user.
     */
    if (str_starts(start, WPA_EVENT_BSS_ADDED) ||
        str_starts(start, WPA_EVENT_BSS_REMOVED))
        return 0;

    return 1;
}

int wpa_cli_add_network(void)
{
    int res;
    char reply[20] = {0};

    res = wpa_ctrl_command(ctrl_conn, "ADD_NETWORK", reply);
    if (res) {
        printf("wpa_cli add network failed, res: %d\n", res);
        return res;
    }

    if (strcmp(reply, "0")) {
        printf("wpa_cli add network failed, reply: %s\n", reply);
        return -1;
    }

    return 0;
}


int wpa_cli_set_network_ssid(int network_id, char *ssid)
{
    char cmd[256] = {0};
    char reply[20] = {0};
    int res;

    res = os_snprintf(cmd, sizeof(cmd), "SET_NETWORK %d ssid \"%s\"",
          network_id, ssid);
    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long SET_NETWORK command.\n");
        return -1;
    }

    res = wpa_ctrl_command(ctrl_conn, cmd, reply);
    if (res) {
        printf("wpa_cli set network ssid failed, res: %d\n", res);
        return res;
    }

    if (strcmp(reply, "OK")) {
        printf("wpa_cli set network ssid failed, reply: %s\n", reply);
        return -1;
    }

    return 0;
}

int wpa_cli_set_network_key_mgmt(int network_id, char *key_mgmt)
{
    char cmd[256] = {0};
    char reply[20] = {0};
    int res;

    res = os_snprintf(cmd, sizeof(cmd), "SET_NETWORK %d key_mgmt %s",
          network_id, key_mgmt);
    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long SET_NETWORK command.\n");
        return -1;
    }

    res = wpa_ctrl_command(ctrl_conn, cmd, reply);
    if (res) {
        printf("wpa_cli set network key_mgmt failed, res: %d\n", res);
        return res;
    }

    if (strcmp(reply, "OK")) {
        printf("wpa_cli set network key_mgmt failed, reply: %s\n", reply);
        return -1;
    }

    return 0;
}

int wpa_cli_set_network_psk(int network_id, char *psk)
{
    char cmd[256] = {0};
    char reply[20] = {0};
    int res;

    res = os_snprintf(cmd, sizeof(cmd), "SET_NETWORK %d psk \"%s\"",
          network_id, psk);
    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long SET_NETWORK command.\n");
        return -1;
    }

    res = wpa_ctrl_command(ctrl_conn, cmd, reply);
    if (res) {
        printf("wpa_cli set network psk failed, res: %d\n", res);
        return res;
    }

    if (strcmp(reply, "OK")) {
        printf("wpa_cli set network psk failed, reply: %s\n", reply);
        return -1;
    }

    return 0;
}

int wpa_cli_select_network(int network_id)
{
    char cmd[32] = {0};
    char reply[20] = {0};
    int res;

    res = os_snprintf(cmd, sizeof(cmd), "SELECT_NETWORK %d", network_id);
    if (res < 0 || (size_t) res >= sizeof(cmd) - 1) {
        printf("Too long SELECT_NETWORK command.\n");
        return -1;
    }

    res = wpa_ctrl_command(ctrl_conn, cmd, reply);
    if (res) {
        printf("wpa_cli select network failed, res: %d\n", res);
        return res;
    }

    if (strcmp(reply, "OK")) {
        printf("wpa_cli select network failed, reply: %s\n", reply);
        return -1;
    }

    return 0;
}

static void wpa_cli_action_process(const char *msg)
{
    const char *pos;
    char *copy = NULL, *id, *pos2;

    pos = msg;
    if (*pos == '<') {
        /* skip priority */
        pos = os_strchr(pos, '>');
        if (pos)
            pos++;
        else
            pos = msg;
    }

    if (str_match(pos, WPA_EVENT_CONNECTED)) {
        int new_id = -1;
        os_unsetenv("WPA_ID");
        os_unsetenv("WPA_ID_STR");
        os_unsetenv("WPA_CTRL_DIR");

        pos = os_strstr(pos, "[id=");
        if (pos)
            copy = os_strdup(pos + 4);

        if (copy) {
            pos2 = id = copy;
            while (*pos2 && *pos2 != ' ')
                pos2++;
            *pos2++ = '\0';
            new_id = atoi(id);
            os_setenv("WPA_ID", id, 1);
            while (*pos2 && *pos2 != '=')
                pos2++;
            if (*pos2 == '=')
                pos2++;
            id = pos2;
            while (*pos2 && *pos2 != ']')
                pos2++;
            *pos2 = '\0';
            os_setenv("WPA_ID_STR", id, 1);
            os_free(copy);
        }

        os_setenv("WPA_CTRL_DIR", ctrl_iface_dir, 1);

        if (!wpa_cli_connected || new_id != wpa_cli_last_id) {
            wpa_cli_connected = 1;
            wpa_cli_last_id = new_id;
            wpa_cli_exec(action_file, ctrl_ifname, "CONNECTED");
        }
    } else if (str_match(pos, WPA_EVENT_DISCONNECTED)) {
        if (wpa_cli_connected) {
            wpa_cli_connected = 0;
            wpa_cli_exec(action_file, ctrl_ifname, "DISCONNECTED");
        }
    } else if (str_match(pos, P2P_EVENT_GROUP_STARTED)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, P2P_EVENT_GROUP_REMOVED)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, P2P_EVENT_CROSS_CONNECT_ENABLE)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, P2P_EVENT_CROSS_CONNECT_DISABLE)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, WPS_EVENT_SUCCESS)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, WPS_EVENT_FAIL)) {
        wpa_cli_exec(action_file, ctrl_ifname, pos);
    } else if (str_match(pos, WPA_EVENT_TERMINATING)) {
        printf("wpa_supplicant is terminating - stop monitoring\n");
        wpa_cli_quit = 1;
    }
}

static void wpa_cli_close_connection(void)
{
    if (ctrl_conn == NULL)
        return;

    if (wpa_cli_attached) {
        wpa_ctrl_detach(interactive ? mon_conn : ctrl_conn);
        wpa_cli_attached = 0;
    }
    wpa_ctrl_close(ctrl_conn);
    ctrl_conn = NULL;
    if (mon_conn) {
        eloop_unregister_read_sock(wpa_ctrl_get_fd(mon_conn));
        wpa_ctrl_close(mon_conn);
        mon_conn = NULL;
    }
}

static int wpa_cli_open_connection(const char *ifname, int attach)
{
#if defined(CONFIG_CTRL_IFACE_UDP) || defined(CONFIG_CTRL_IFACE_NAMED_PIPE)
    ctrl_conn = wpa_ctrl_open(ifname);
    if (ctrl_conn == NULL)
        return -1;

    if (attach && interactive)
        mon_conn = wpa_ctrl_open(ifname);
    else
        mon_conn = NULL;
#else /* CONFIG_CTRL_IFACE_UDP || CONFIG_CTRL_IFACE_NAMED_PIPE */
    char *cfile = NULL;
    int flen, res;

    if (ifname == NULL)
        return -1;

#ifdef ANDROID
    if (access(ctrl_iface_dir, F_OK) < 0) {
        cfile = os_strdup(ifname);
        if (cfile == NULL)
            return -1;
    }
#endif /* ANDROID */

    if (cfile == NULL) {
        flen = os_strlen(ctrl_iface_dir) + os_strlen(ifname) + 2;
        cfile = os_malloc(flen);
        if (cfile == NULL)
            return -1;
        res = os_snprintf(cfile, flen, "%s/%s", ctrl_iface_dir,
                  ifname);
        if (res < 0 || res >= flen) {
            os_free(cfile);
            return -1;
        }
    }

    ctrl_conn = wpa_ctrl_open(cfile);
    if (ctrl_conn == NULL) {
        os_free(cfile);
        return -1;
    }

    if (attach && interactive)
        mon_conn = wpa_ctrl_open(cfile);
    else
        mon_conn = NULL;
    os_free(cfile);
#endif /* CONFIG_CTRL_IFACE_UDP || CONFIG_CTRL_IFACE_NAMED_PIPE */

    if (mon_conn) {
        if (wpa_ctrl_attach(mon_conn) == 0) {
            wpa_cli_attached = 1;
            if (interactive)
                eloop_register_read_sock(
                    wpa_ctrl_get_fd(mon_conn),
                    wpa_cli_mon_receive, NULL, NULL);
        } else {
            printf("Warning: Failed to attach to "
                   "wpa_supplicant.\n");
            return -1;
        }
    }

    return 0;
}

static void wpa_cli_reconnect(void)
{
    wpa_cli_close_connection();
    wpa_cli_open_connection(ctrl_ifname, 1);
}

static void wpa_cli_recv_pending(struct wpa_ctrl *ctrl, int action_monitor)
{
    if (ctrl_conn == NULL) {
        wpa_cli_reconnect();
        return;
    }
    while (wpa_ctrl_pending(ctrl) > 0) {
        char buf[256];
        size_t len = sizeof(buf) - 1;
        if (wpa_ctrl_recv(ctrl, buf, &len) == 0) {
            buf[len] = '\0';
            if (action_monitor)
                wpa_cli_action_process(buf);
            else {
                if (wpa_cli_show_event(buf)) {
                    edit_clear_line();
                    printf("\r%s\n", buf);
                    edit_redraw();
                }
            }
        } else {
            printf("Could not read pending message.\n");
            break;
        }
    }

    if (wpa_ctrl_pending(ctrl) < 0) {
        printf("Connection to wpa_supplicant lost - trying to "
               "reconnect\n");
        wpa_cli_reconnect();
    }
}

static void wpa_cli_mon_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
    wpa_cli_recv_pending(mon_conn, 0);
}

static void wpa_cli_cleanup(void)
{
    wpa_cli_close_connection();
    if (pid_file)
        os_daemonize_terminate(pid_file);

    os_program_deinit();
}

static int g_wpa_cli_initialized = 0;
int init_wpa_cli(const char *ctrl_if_name, const char *ctrl_if_dir)
{
    if (g_wpa_cli_initialized == 1) {
        printf("%s,%d wpa_cli already initialized!\n", __func__, __LINE__);
        return -1;
    }

    if (ctrl_if_name == NULL || ctrl_if_dir == NULL) {
        printf("%s,%d argument error\n", __func__, __LINE__);
        return -1;
    }

    ctrl_iface_dir = ctrl_if_dir;
    ctrl_ifname = (char *)ctrl_if_name;

    if (wpa_cli_open_connection(ctrl_ifname, 0) < 0) {
        printf("Failed to connect to wpa_supplicant, %s  error: %s\n",
            ctrl_ifname ? ctrl_ifname : "(nil)",
            strerror(errno));
        return -1;
    }

    g_wpa_cli_initialized = 1;

    return 0;
}

int deinit_wpa_cli(void)
{
    if (g_wpa_cli_initialized == 1) {
        wpa_cli_cleanup();
        g_wpa_cli_initialized = 0;
    }

    return 0;
}

#endif
