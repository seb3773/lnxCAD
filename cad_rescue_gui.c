#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <grp.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <termios.h>
#include <sys/klog.h>
#include <linux/reboot.h>
#include <dirent.h>
#include <sys/stat.h>

#include <sys/prctl.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <pty.h>


#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
#include "nuklear.h"

#define NK_RAWFB_IMPLEMENTATION
#include "nuklear_rawfb.h"

static void safe_copy(char *dst, const char *src, size_t max_len) {
    if (!dst || max_len == 0) return;
    size_t i = 0;
    while (src && src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void safe_concat(char *dst, const char *src, size_t max_len) {
    if (!dst || max_len == 0) return;
    size_t len = 0;
    while (dst[len] && len < max_len - 1) len++;
    size_t i = 0;
    while (src && src[i] && len + i < max_len - 1) {
        dst[len + i] = src[i];
        i++;
    }
    dst[len + i] = '\0';
}

static void secure_clear_buffer(void *v, size_t n) {
    if (!v || n == 0) return;
    volatile unsigned char *p = (volatile unsigned char *)v;
    while (n--) *p++ = 0;
}


#include "cad_theme.h"
#include "cursor_data.h"
#include "custom_font.h"
#include "shutdown_icon.h"
#include "terminal_icon.h"
#include "find_icon.h"
#include "sleep_icon.h"
#include "power_icon.h"
#include "restart_icon.h"
#include "check1_icon.h"
#include "check2_icon.h"
#include "close_icon.h"
#include "runcommand_icon.h"
#include "console_icon.h"
#include "taskmgr_icon.h"
#include "password_icon.h"
#include "branding_icon.h"

#include "busybox_bin.h"

typedef struct {
    const char *lang_code;
    const char *btn_lock;
    const char *btn_switch_user;
    const char *btn_sign_out;
    const char *btn_change_password;
    const char *btn_task_manager;
    const char *btn_run_command;
    const char *btn_cancel;
    const char *btn_back;
    const char *title_switch_user;
    const char *label_active_sessions;
    const char *btn_start_new_session;
    const char *btn_close;
    const char *btn_sleep;
    const char *btn_restart;
    const char *btn_shutdown;
    const char *label_enter_admin_pwd;
    const char *btn_authenticate;
    const char *label_auth_failed;
    const char *title_change_password;
    const char *label_current_pwd;
    const char *label_new_pwd;
    const char *label_confirm_pwd;
    const char *label_show_passwords;
    const char *btn_apply;
    const char *err_pwd_empty;
    const char *err_pwd_mismatch;
    const char *msg_changing_pwd;
    const char *title_run_command;
    const char *label_enter_command;
    const char *label_execute_as_root;
    const char *btn_run;
    const char *label_parent_vt;
    const char *label_login_screen;
    const char *label_graphical;
    const char *label_console;
    const char *title_task_manager;
    const char *btn_top_ram;
    const char *btn_top_cpu;
    const char *btn_kill;
    const char *btn_terminate;
    const char *btn_continue;
    const char *btn_pause;
    const char *unit_mb;
    const char *title_privilege_elevation;
    const char *label_process_name;
    const char *btn_root_mode;
    const char *btn_user_mode;
    const char *err_pwd_short_simple;
    const char *err_pwd_incorrect;
    const char *msg_pwd_changed;
    const char *err_pwd_change_failed;
} translation_t;

#include "translations/en.h"
#include "translations/fr.h"
#include "translations/de.h"
#include "translations/es.h"
#include "translations/it.h"
#include "translations/pt.h"
#include "translations/nl.h"
#include "translations/sv.h"
#include "translations/pl.h"
#include "translations/ro.h"
#include "translations/tr.h"

static const translation_t *available_translations[] = {
    &trans_en, /* Fallback index 0 */
    &trans_fr,
    &trans_de,
    &trans_es,
    &trans_it,
    &trans_pt,
    &trans_nl,
    &trans_sv,
    &trans_pl,
    &trans_ro,
    &trans_tr,
    NULL
};

static const translation_t *current_trans = &trans_en;

/* Dynamic sizes computed at startup based on current language translation and font */
static int pm_menu_w = 230;
static int root_mode_btn_w = 110;

#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#endif
#ifndef F_SEAL_SHRINK
#define F_SEAL_SHRINK 0x0002
#endif
#ifndef F_SEAL_GROW
#define F_SEAL_GROW 0x0004
#endif
#ifndef F_SEAL_WRITE
#define F_SEAL_WRITE 0x0008
#endif
#ifndef F_SEAL_SEAL
#define F_SEAL_SEAL 0x0001
#endif
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001
#endif
#ifndef SYS_memfd_create
#define SYS_memfd_create 319
#endif
#ifndef SYS_execveat
#define SYS_execveat 322
#endif
#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH 0x1000
#endif

// --- LIBTSM & PTY ---
#include "libtsm.h"
#include "shl-pty.h"
#include "xkbcommon/xkbcommon-keysyms.h"

struct tsm_screen *term_screen = NULL;
struct tsm_vte *term_vte = NULL;
struct shl_pty *term_pty = NULL;
int show_terminal = 0;

static unsigned int fixed_cols = 80;
static unsigned int fixed_rows = 24;
static int busybox_memfd = -1;
static int terminal_mode = 0; /* 0 = bash, 1 = busybox */

static void get_username_by_uid(int uid, char *username_out, size_t max_len);

struct recovery_journal_t {
    volatile int original_vt;
    volatile int rescue_vt;
    volatile int tty_fd;
    volatile int rescue_tty_fd;
    volatile int drm_fd;
    volatile int kbd_fd;
    volatile int mouse_fd;
    volatile int kbd_mode_saved;
    volatile int original_kbd_mode;
    volatile int kbd_grabbed;
    volatile int mouse_grabbed;
};
struct recovery_journal_t journal = { -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0 };

#define MAX_SESSIONS 32
struct session_info_t {
    int vt;
    int is_graphical;
    int is_dm;
    uid_t uid;
    char username[32];
    char proc_name[64];
    int priority; // 0 = default, 1 = shell, 2 = desktop, 3 = DM, 4 = display server (Xorg/Wayland)
};
static struct session_info_t sessions[MAX_SESSIONS];
static int num_sessions = 0;

static void terminal_vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data) {
    (void)vte;
    (void)data;
    if (term_pty) {
        shl_pty_write(term_pty, u8, len);
    }
}

static void terminal_pty_input_cb(struct shl_pty *pty, void *data, char *u8, size_t len) {
    (void)pty;
    (void)data;
    if (term_vte) {
        tsm_vte_input(term_vte, u8, len);
    }
}

#ifdef ENABLE_DEBUG
static void DEBUG_LOG(const char *msg);
static void DEBUG_LOG_INT(const char *msg, int val);
#else
#define DEBUG_LOG(msg) ((void)0)
#define DEBUG_LOG_INT(msg, val) ((void)0)
#endif
static void int_to_str(int val, char *buf);
void cleanup_drm(void);

static void init_busybox_memfd(void) {
    busybox_memfd = syscall(SYS_memfd_create, "busybox", MFD_CLOEXEC);
    if (busybox_memfd < 0) {
        DEBUG_LOG("[WARN] memfd_create échoué, busybox en RAM indisponible.");
        return;
    }
    
    const unsigned char *src = busybox_data;
    size_t remaining = busybox_data_len;
    while (remaining > 0) {
        ssize_t written = write(busybox_memfd, src, remaining);
        if (written <= 0) {
            if (errno == EINTR) continue;
            DEBUG_LOG("[ERROR] Écriture busybox dans memfd échouée.");
            close(busybox_memfd);
            busybox_memfd = -1;
            return;
        }
        src += (size_t)written;
        remaining -= (size_t)written;
    }
    
    if (fcntl(busybox_memfd, F_ADD_SEALS, 
              F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) < 0) {
        DEBUG_LOG("[WARN] Impossible de sceller le memfd (non critique).");
    }
    
#ifdef ENABLE_DEBUG
    char msg[128] = "[DEBUG] BusyBox chargé en RAM (memfd fd=";
    char fd_str[32];
    int_to_str(busybox_memfd, fd_str);
    safe_concat(msg, fd_str, sizeof(msg));
    safe_concat(msg, ", octets: ", sizeof(msg));
    char len_str[32];
    int_to_str((int)busybox_data_len, len_str);
    safe_concat(msg, len_str, sizeof(msg));
    safe_concat(msg, ").", sizeof(msg));
    DEBUG_LOG(msg);
#endif
}

static void close_current_pty(void) {
    if (term_pty) {
        pid_t child_pid = shl_pty_get_child(term_pty);
        shl_pty_unref(term_pty);
        term_pty = NULL;
        if (child_pid > 0) {
            kill(-child_pid, SIGKILL);
            waitpid(child_pid, NULL, WNOHANG);
        }
    }
}

static void cleanup_terminal_session(void) {
    close_current_pty();
    if (term_screen) {
        tsm_screen_clear_sb(term_screen);
        tsm_screen_erase_screen(term_screen, false);
        tsm_screen_move_to(term_screen, 0, 0);
        tsm_screen_reset(term_screen);
    }
    if (term_vte) {
        tsm_vte_reset(term_vte);
    }
}

static struct {
    int uid;
    int gid;
    char username[64];
    char home[256];
    char display[64];
    char xauthority[256];
    char xdg_runtime_dir[256];
    char wayland_display[64];
    char dbus_addr[512];
    char de_name[64];
    char session_id[32];
    char lock_cmd[256];
    char run_cmd[256];
    char raw_env[32768];
    int raw_env_len;
} user_session = {0};

static void drop_privs_to_user() {
    if (getuid() == 0) {
        if (setgroups(0, NULL) < 0) _exit(126);
    }
    if (setgid(user_session.gid) < 0) _exit(126);
    if (setuid(user_session.uid) < 0) _exit(126);
    if (getuid() != (uid_t)user_session.uid || geteuid() != (uid_t)user_session.uid) _exit(126);
    if (getgid() != (gid_t)user_session.gid || getegid() != (gid_t)user_session.gid) _exit(126);
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
}

static void drop_privs_to_user_passwd() {
    if (getuid() == 0) {
        if (setgroups(0, NULL) < 0) _exit(126);
    }
    if (setgid(user_session.gid) < 0) _exit(126);
    if (setuid(user_session.uid) < 0) _exit(126);
    if (getuid() != (uid_t)user_session.uid || geteuid() != (uid_t)user_session.uid) _exit(126);
    if (getgid() != (gid_t)user_session.gid || getegid() != (gid_t)user_session.gid) _exit(126);
}

static void drop_privs_to_user_with_groups() {
    if (getuid() == 0) {
        if (initgroups(user_session.username, user_session.gid) < 0) _exit(126);
    }
    if (setgid(user_session.gid) < 0) _exit(126);
    if (setuid(user_session.uid) < 0) _exit(126);
    if (getuid() != (uid_t)user_session.uid || geteuid() != (uid_t)user_session.uid) _exit(126);
    if (getgid() != (gid_t)user_session.gid || getegid() != (gid_t)user_session.gid) _exit(126);
}


/* ---- PRIVILEGE ESCALATION STATE & AUTHS ---- */
static int root_authorized = 0;        // Flag global d'authentification root validée
static int terminal_root_mode = 0;     // Mode du terminal (0 = user, 1 = root)
static int taskmgr_root_mode = 0;      // Mode du task manager (0 = user, 1 = root)

static int show_password_prompt = 0;   // Affichage de la boîte de dialogue de mot de passe
static int pwd_prompt_source = 0;      // 1 = Terminal, 2 = Task Manager, 3 = Run Command
static char root_pwd_buffer[256] = {0};
static int root_pwd_len = 0;
static char root_auth_msg[128] = {0};
static int pwd_focus_idx = 0;
static int pwd_enter_pressed = 0;




static int safe_fexecve(int fd, char *argv[], char *envp[]) {
    fexecve(fd, argv, envp);
    return syscall(SYS_execveat, fd, "", argv, envp, AT_EMPTY_PATH);
}

static pid_t launch_terminal_shell(int mode) {
    pid_t pid = shl_pty_open(&term_pty, terminal_pty_input_cb, NULL, fixed_cols, fixed_rows);
    if (pid < 0) {
        DEBUG_LOG("[ERROR] Impossible de forker le PTY.");
        return -1;
    }
    if (pid == 0) {
        /* PROCESSUS ENFANT */
        if (!terminal_root_mode) {
            drop_privs_to_user();
            
            char env_home[512] = "HOME="; safe_concat(env_home, user_session.home, sizeof(env_home));
            char env_user[128] = "USER="; safe_concat(env_user, user_session.username, sizeof(env_user));
            char hostname[128] = "unknown";
            gethostname(hostname, sizeof(hostname));
            char env_host[256] = "HOSTNAME="; safe_concat(env_host, hostname, sizeof(env_host));
            char *envp[512];
            int envc = 0;
            envp[envc++] = env_home;
            envp[envc++] = env_user;
            envp[envc++] = env_host;
            envp[envc++] = "PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin:/opt/trinity/bin:/opt/tde/bin";
            envp[envc++] = "TERM=xterm-256color";
            
            char *p = user_session.raw_env;
            while (p < user_session.raw_env + user_session.raw_env_len && envc < 510) {
                if (strncmp(p, "HOME=", 5) != 0 && strncmp(p, "PATH=", 5) != 0 && strncmp(p, "TERM=", 5) != 0 && strncmp(p, "USER=", 5) != 0 && strncmp(p, "HOSTNAME=", 9) != 0) {
                    envp[envc++] = p;
                }
                p += strlen(p) + 1;
            }
            envp[envc] = NULL;



            if (mode == 0) {
                if (busybox_memfd >= 0) close(busybox_memfd);
                execve("/bin/bash", (char *[]){"/bin/bash", NULL}, envp);
                execve("/bin/sh", (char *[]){"/bin/sh", NULL}, envp);
            } else {
                if (busybox_memfd >= 0) {
                    char *argv[] = {"sh", NULL};
                    safe_fexecve(busybox_memfd, argv, envp);
                }
            }
        } else {
            setenv("TERM", "xterm-256color", 1);
            if (mode == 0) {
                /* Mode bash : essayer bash puis sh */
                if (busybox_memfd >= 0) close(busybox_memfd);
                execl("/bin/bash", "/bin/bash", NULL);
                execl("/bin/sh", "/bin/sh", NULL);
            } else {
                /* Mode busybox : exécuter depuis le memfd */
                if (busybox_memfd >= 0) {
                    char hostname[128] = "unknown";
                    gethostname(hostname, sizeof(hostname));
                    char env_host[256] = "HOSTNAME="; safe_concat(env_host, hostname, sizeof(env_host));
                    char *argv[] = {"sh", NULL};
                    char *envp[] = {
                        "TERM=xterm-256color",
                        "HOME=/root",
                        "PATH=/sbin:/usr/sbin:/bin:/usr/bin",
                        "USER=root",
                        env_host,
                        NULL
                    };
                    safe_fexecve(busybox_memfd, argv, envp);
                }
            }
        }
        /* Si on arrive ici, tout a échoué */
        _exit(1);
    }
    return pid;
}

struct terminal_render_ctx {
    struct nk_context *ctx;
    struct nk_rect bounds;
    float cell_w;
    float cell_h;
};

static int terminal_draw_cb(struct tsm_screen *con, uint64_t id, const uint32_t *ch, size_t len,
                            unsigned int width, unsigned int posx, unsigned int posy,
                            const struct tsm_screen_attr *attr, tsm_age_t age, void *data) {
    (void)con; (void)id; (void)width; (void)age;
    struct terminal_render_ctx *rctx = (struct terminal_render_ctx *)data;
    
    // Pixel-perfect rounding to prevent subpixel rendering overlap and blur
    float px = (float)(int)(rctx->bounds.x + posx * rctx->cell_w);
    float py = (float)(int)(rctx->bounds.y + posy * rctx->cell_h);
    
    // Draw background if not default black
    struct nk_color bg_color = nk_rgb(attr->br, attr->bg, attr->bb);
    if (attr->inverse) {
        bg_color = nk_rgb(attr->fr, attr->fg, attr->fb);
    }
    
    // Draw background
    if (bg_color.r != 0 || bg_color.g != 0 || bg_color.b != 0 || attr->inverse) {
        nk_fill_rect(nk_window_get_canvas(rctx->ctx), nk_rect(px, py, rctx->cell_w * (width > 0 ? width : 1), rctx->cell_h), 0, bg_color);
    }
    
    if (!len || !ch) {
        // Draw cursor even on empty cells
        if (posx == tsm_screen_get_cursor_x(con) && posy == tsm_screen_get_cursor_y(con)) {
            struct nk_color fg_color = nk_rgb(attr->fr, attr->fg, attr->fb);
            nk_fill_rect(nk_window_get_canvas(rctx->ctx), nk_rect(px, py, rctx->cell_w, rctx->cell_h), 0, fg_color);
        }
        return 0;
    }
    
    // Convert UCS4 to UTF8
    char utf8[16];
    size_t utf8_len = 0;
    for (size_t i = 0; i < len; ++i) {
        utf8_len += tsm_ucs4_to_utf8(ch[i], utf8 + utf8_len);
        if (utf8_len >= sizeof(utf8) - 4) break;
    }
    utf8[utf8_len] = '\0';
    
    // Draw foreground text
    struct nk_color fg_color = nk_rgb(attr->fr, attr->fg, attr->fb);
    if (attr->inverse) {
        fg_color = nk_rgb(attr->br, attr->bg, attr->bb);
    }
    
    // Handle cursor logic
    if (posx == tsm_screen_get_cursor_x(con) && posy == tsm_screen_get_cursor_y(con)) {
        nk_fill_rect(nk_window_get_canvas(rctx->ctx), nk_rect(px, py, rctx->cell_w, rctx->cell_h), 0, fg_color);
        fg_color = bg_color; // Invert text color over cursor
    }
    
    nk_draw_text(nk_window_get_canvas(rctx->ctx), nk_rect(px, py, rctx->cell_w * width, rctx->cell_h),
                 utf8, utf8_len, rctx->ctx->style.font, nk_rgba(0,0,0,0), fg_color);
    
    return 0;
}


/* ---- UTILS SANS STDIO ---- */
static void int_to_str(int val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    int i = 0, j;
    char temp[32];
    long long v = val;
    if (v < 0) { buf[i++] = '-'; v = -v; }
    int start = i;
    while (v > 0) {
        temp[i++] = (v % 10) + '0';
        v /= 10;
    }
    for (j = 0; j < i - start; j++) {
        buf[start + j] = temp[i - 1 - j];
    }
    buf[i] = '\0';
}

static int str_to_int(const char *s) {
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') s++; // Skip leading whitespace
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    
    long long res = 0;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        if (res > 2147483647LL) {
            return sign == 1 ? 2147483647 : -2147483648;
        }
        s++;
    }
    return (int)(res * sign);
}

static unsigned long long str_to_ull(const char *s) {
    unsigned long long res = 0;
    while (*s == ' ') s++;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}

#ifdef ENABLE_DEBUG
static void DEBUG_LOG(const char *msg) {
    write(1, msg, strlen(msg));
    write(1, "\n", 1);
}

static void DEBUG_LOG_INT(const char *msg, int val) {
    write(1, msg, strlen(msg));
    char buf[32];
    int_to_str(val, buf);
    write(1, buf, strlen(buf));
    write(1, "\n", 1);
}
#endif

/* Dessine le curseur Xcursor avec alpha blending sur le framebuffer XRGB */
static void draw_cursor(uint8_t *fb, uint32_t fb_w, uint32_t fb_h, uint32_t pitch,
                        int mx, int my) {
    uint32_t *dst = (uint32_t *)fb;
    uint32_t stride = pitch / 4;
    
    for (int cy = 0; cy < CURSOR_H; cy++) {
        int sy = my - CURSOR_HOT_Y + cy;
        if (sy < 0 || sy >= (int)fb_h) continue;
        for (int cx = 0; cx < CURSOR_W; cx++) {
            int sx = mx - CURSOR_HOT_X + cx;
            if (sx < 0 || sx >= (int)fb_w) continue;
            
            uint32_t cpx = cursor_data[cy * CURSOR_W + cx];
            uint32_t alpha = (cpx >> 24) & 0xFF;
            if (alpha == 0) continue;
            
            /* Xcursor ARGB est pré-multiplié, on le dé-pré-multiplie pour blender */
            uint32_t cr = (cpx >> 16) & 0xFF;
            uint32_t cg = (cpx >> 8)  & 0xFF;
            uint32_t cb =  cpx        & 0xFF;
            
            if (alpha == 255) {
                dst[sy * stride + sx] = (cr << 16) | (cg << 8) | cb;
            } else {
                uint32_t bg = dst[sy * stride + sx];
                uint32_t br = (bg >> 16) & 0xFF;
                uint32_t bg_ = (bg >> 8)  & 0xFF;
                uint32_t bb =  bg        & 0xFF;
                uint32_t inv = 255 - alpha;
                uint32_t or_ = (cr * 255 + br * inv) / 255;
                uint32_t og  = (cg * 255 + bg_ * inv) / 255;
                uint32_t ob  = (cb * 255 + bb * inv) / 255;
                dst[sy * stride + sx] = (or_ << 16) | (og << 8) | ob;
            }
        }
    }
}

static void draw_rect(uint8_t *fb, uint32_t fb_w, uint32_t fb_h, uint32_t pitch,
                      int x, int y, int w, int h, uint32_t color) {
    uint32_t *dst = (uint32_t *)fb;
    uint32_t stride = pitch / 4;
    for (int cy = 0; cy < h; cy++) {
        int sy = y + cy;
        if (sy < 0 || sy >= (int)fb_h) continue;
        for (int cx = 0; cx < w; cx++) {
            int sx = x + cx;
            if (sx < 0 || sx >= (int)fb_w) continue;
            dst[sy * stride + sx] = color;
        }
    }
}

/* Dessine une image ARGB 32-bits sur le framebuffer XRGB */
static void draw_image(uint8_t *fb, uint32_t fb_w, uint32_t fb_h, uint32_t pitch,
                       int x, int y, int img_w, int img_h, const uint32_t *img_data) {
    uint32_t *dst = (uint32_t *)fb;
    uint32_t stride = pitch / 4;
    
    for (int cy = 0; cy < img_h; cy++) {
        int sy = y + cy;
        if (sy < 0 || sy >= (int)fb_h) continue;
        for (int cx = 0; cx < img_w; cx++) {
            int sx = x + cx;
            if (sx < 0 || sx >= (int)fb_w) continue;
            
            uint32_t cpx = img_data[cy * img_w + cx];
            uint32_t alpha = (cpx >> 24) & 0xFF;
            
            if (alpha == 255) {
                dst[sy * stride + sx] = cpx & 0xFFFFFF;
            } else if (alpha > 0) {
                uint32_t bg = dst[sy * stride + sx];
                uint32_t inv_alpha = 255 - alpha;
                
                uint32_t r = (((cpx >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * inv_alpha) / 255;
                uint32_t g = (((cpx >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * inv_alpha) / 255;
                uint32_t b = ((cpx & 0xFF) * alpha + (bg & 0xFF) * inv_alpha) / 255;
                
                dst[sy * stride + sx] = (r << 16) | (g << 8) | b;
            }
        }
    }
}



#ifdef ENABLE_DEBUG
static void write_debug_log(const char *msg) {
    int fd = open("/home/cdef/_PROJETS/CAD/auth_debug.log", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0666);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}
#else
#define write_debug_log(msg) ((void)0)
#endif


static int verify_root_password_pty_su(const char *password) {
    write_debug_log("--- Starting SU check ---\n");
    struct sigaction old_sa, new_sa;
    memset(&new_sa, 0, sizeof(new_sa));
    new_sa.sa_handler = SIG_DFL;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = 0;
    sigaction(SIGCHLD, &new_sa, &old_sa);

    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);
    if (pid < 0) {
        write_debug_log("SU forkpty failed\n");
        sigaction(SIGCHLD, &old_sa, NULL);
        return -1;
    }
    if (pid == 0) {
        if (getuid() == 0 && user_session.uid > 0) {
            drop_privs_to_user_with_groups();
        }
        setenv("LC_ALL", "C", 1);
        execlp("su", "su", "-c", "true", (char *)NULL);
        _exit(127);
    }
    
    char buf[1024];
    ssize_t n;
    int pass_sent = 0;
    fd_set fds;
    struct timeval tv;
    
    while (!pass_sent) {
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int sel = select(master + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) {
            write_debug_log("SU select timeout before password prompt\n");
            break;
        }
        n = read(master, buf, sizeof(buf) - 1);
        if (n <= 0) {
            write_debug_log("SU read EOF/error before password prompt\n");
            break;
        }
        buf[n] = '\0';
        write_debug_log("SU read prompt: "); write_debug_log(buf); write_debug_log("\n");
        if (strstr(buf, "assword") || strstr(buf, "passe") ||
            strstr(buf, "contrase") || strstr(buf, "Contrase") ||
            strstr(buf, "Kennwort") || strstr(buf, "kennwort") ||
            strstr(buf, "senha") || strstr(buf, "Senha") ||
            strstr(buf, "Parola") || strstr(buf, "parola")) {
            dprintf(master, "%s\n", password);
            pass_sent = 1;
            write_debug_log("SU password sent\n");
        }
    }
    
    while (1) {
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int sel = select(master + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) break;
        n = read(master, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        write_debug_log("SU after-read: "); write_debug_log(buf); write_debug_log("\n");
    }
    
    close(master);
    int status = -1;
    waitpid(pid, &status, 0);
    sigaction(SIGCHLD, &old_sa, NULL);

#ifdef ENABLE_DEBUG
    char status_msg[8];
    int val = WEXITSTATUS(status);
    int exited = WIFEXITED(status);
    write_debug_log("SU exit status: ");
    if (exited) {
        status_msg[0] = '0' + (val / 100) % 10;
        status_msg[1] = '0' + (val / 10) % 10;
        status_msg[2] = '0' + val % 10;
        status_msg[3] = '\n';
        status_msg[4] = '\0';
        write_debug_log(status_msg);
    } else {
        write_debug_log("terminated by signal\n");
    }
#endif

    secure_clear_buffer(buf, sizeof(buf));
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        write_debug_log("SU check SUCCESS\n");
        return 1;
    }
    write_debug_log("SU check FAILED\n");
    return (WIFEXITED(status) && WEXITSTATUS(status) == 127) ? -1 : 0;
}

static int verify_root_password_pty_sudo(const char *password) {
    write_debug_log("--- Starting SUDO check ---\n");
    struct sigaction old_sa, new_sa;
    memset(&new_sa, 0, sizeof(new_sa));
    new_sa.sa_handler = SIG_DFL;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = 0;
    sigaction(SIGCHLD, &new_sa, &old_sa);

    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);
    if (pid < 0) {
        write_debug_log("SUDO forkpty failed\n");
        sigaction(SIGCHLD, &old_sa, NULL);
        return -1;
    }
    if (pid == 0) {
        if (getuid() == 0 && user_session.uid > 0) {
            drop_privs_to_user_with_groups();
        }
        setenv("LC_ALL", "C", 1);
        execlp("sudo", "sudo", "-k", "-S", "true", (char *)NULL);
        _exit(127);
    }
    
    char buf[1024];
    ssize_t n;
    int pass_sent = 0;
    fd_set fds;
    struct timeval tv;
    
    while (!pass_sent) {
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int sel = select(master + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) {
            write_debug_log("SUDO select timeout before password prompt\n");
            break;
        }
        n = read(master, buf, sizeof(buf) - 1);
        if (n <= 0) {
            write_debug_log("SUDO read EOF/error before password prompt\n");
            break;
        }
        buf[n] = '\0';
        write_debug_log("SUDO read prompt: "); write_debug_log(buf); write_debug_log("\n");
        if (strstr(buf, "assword") || strstr(buf, "passe") ||
            strstr(buf, "contrase") || strstr(buf, "Contrase") ||
            strstr(buf, "Kennwort") || strstr(buf, "kennwort") ||
            strstr(buf, "senha") || strstr(buf, "Senha") ||
            strstr(buf, "Parola") || strstr(buf, "parola")) {
            dprintf(master, "%s\n", password);
            pass_sent = 1;
            write_debug_log("SUDO password sent\n");
        }
    }
    
    while (1) {
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int sel = select(master + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) break;
        n = read(master, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        write_debug_log("SUDO after-read: "); write_debug_log(buf); write_debug_log("\n");
    }
    
    close(master);
    int status = -1;
    waitpid(pid, &status, 0);
    sigaction(SIGCHLD, &old_sa, NULL);

#ifdef ENABLE_DEBUG
    char status_msg[8];
    int val = WEXITSTATUS(status);
    int exited = WIFEXITED(status);
    write_debug_log("SUDO exit status: ");
    if (exited) {
        status_msg[0] = '0' + (val / 100) % 10;
        status_msg[1] = '0' + (val / 10) % 10;
        status_msg[2] = '0' + val % 10;
        status_msg[3] = '\n';
        status_msg[4] = '\0';
        write_debug_log(status_msg);
    } else {
        write_debug_log("terminated by signal\n");
    }
#endif

    secure_clear_buffer(buf, sizeof(buf));
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        write_debug_log("SUDO check SUCCESS\n");
        return 1;
    }
    write_debug_log("SUDO check FAILED\n");
    return (WIFEXITED(status) && WEXITSTATUS(status) == 127) ? -1 : 0;
}

static int verify_root_password_shadow(const char *password) {
#ifdef __GLIBC__
    int fd = open("/etc/shadow", O_RDONLY);
    if (fd < 0) return -1;
    
    char buf[8192];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    int ret = -1;
    char *line = buf;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next++ = '\0';
        
        if (strncmp(line, "root:", 5) == 0) {
            char *p = line + 5;
            char *hash_end = strchr(p, ':');
            if (hash_end) {
                *hash_end = '\0';
                char *res = crypt(password, p);
                if (res && strcmp(res, p) == 0) {
                    ret = 1;
                } else {
                    ret = 0;
                }
            }
            break;
        }
        line = next;
    }
    secure_clear_buffer(buf, sizeof(buf));
    return ret;
#else
    (void)password;
    return -1;
#endif
}


static int verify_root_password(const char *password) {
    int res = verify_root_password_pty_su(password);
    if (res == 1) return 1;
    
    int res_sudo = verify_root_password_pty_sudo(password);
    if (res_sudo == 1) return 1;
    
    return verify_root_password_shadow(password);
}

static int change_own_password(const char *oldpass, const char *newpass, char *outmsg, size_t outlen) {
    struct sigaction old_sa, new_sa;
    memset(&new_sa, 0, sizeof(new_sa));
    new_sa.sa_handler = SIG_DFL;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = 0;
    sigaction(SIGCHLD, &new_sa, &old_sa);

    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);
    if (pid < 0) {
        sigaction(SIGCHLD, &old_sa, NULL);
        return -1;
    }
    if (pid == 0) {
        if (getuid() == 0 && user_session.uid > 0) {
            drop_privs_to_user_passwd();
        }
        setenv("LC_ALL", "C", 1);
        execlp("passwd", "passwd", (char *)NULL);
        _exit(127);
    }
    char buf[1024];
    ssize_t n;
    int has_msg_flag = 0;
    int oldpass_sent = 0;
    int newpass_sent = 0;
    int confirm_sent = 0;
    if (outmsg && outlen > 0) outmsg[0] = '\0';
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(master, &fds);
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
        int sel = select(master + 1, &fds, NULL, NULL, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            break;
        } else if (sel == 0) {
            break; // Timeout
        }
        n = read(master, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        
        int is_current = (strstr(buf, "current") != NULL || strstr(buf, "Current") != NULL ||
                          strstr(buf, "actuel") != NULL || strstr(buf, "Actuel") != NULL ||
                          strstr(buf, "courant") != NULL || strstr(buf, "Courant") != NULL ||
                          strstr(buf, "aktuelle") != NULL || strstr(buf, "Aktuelle") != NULL ||
                          strstr(buf, "actual") != NULL || strstr(buf, "Actual") != NULL ||
                          strstr(buf, "attuale") != NULL || strstr(buf, "Attuale") != NULL);

        int is_retype = (strstr(buf, "retype") != NULL || strstr(buf, "Retype") != NULL ||
                         strstr(buf, "confirm") != NULL || strstr(buf, "Confirm") != NULL ||
                         strstr(buf, "ressaisissez") != NULL || strstr(buf, "Ressaisissez") != NULL ||
                         strstr(buf, "confirmez") != NULL || strstr(buf, "Confirmez") != NULL ||
                         strstr(buf, "repetez") != NULL || strstr(buf, "Répétez") != NULL ||
                         strstr(buf, "wiederholen") != NULL || strstr(buf, "Wiederholen") != NULL ||
                         strstr(buf, "bestätigen") != NULL || strstr(buf, "Bestätigen") != NULL ||
                         strstr(buf, "bestaetigen") != NULL || strstr(buf, "Bestaetigen") != NULL ||
                         strstr(buf, "repita") != NULL || strstr(buf, "Repita") != NULL ||
                         strstr(buf, "confirme") != NULL || strstr(buf, "Confirme") != NULL ||
                         strstr(buf, "vuelva") != NULL || strstr(buf, "Vuelva") != NULL ||
                         strstr(buf, "reinserisci") != NULL || strstr(buf, "Reinserisci") != NULL ||
                         strstr(buf, "conferma") != NULL || strstr(buf, "Conferma") != NULL ||
                         strstr(buf, "ripeti") != NULL || strstr(buf, "Ripeti") != NULL);

        int is_new = ((strstr(buf, "new") != NULL || strstr(buf, "New") != NULL ||
                       strstr(buf, "nouveau") != NULL || strstr(buf, "Nouveau") != NULL ||
                       strstr(buf, "nouvelle") != NULL || strstr(buf, "Nouvelle") != NULL ||
                       strstr(buf, "neue") != NULL || strstr(buf, "Neue") != NULL ||
                       strstr(buf, "neues") != NULL || strstr(buf, "Neues") != NULL ||
                       strstr(buf, "nueva") != NULL || strstr(buf, "Nueva") != NULL ||
                       strstr(buf, "nuevo") != NULL || strstr(buf, "Nuevo") != NULL ||
                       strstr(buf, "nuova") != NULL || strstr(buf, "Nuova") != NULL) && !is_retype);

        if (is_current && !oldpass_sent) {
            if (oldpass && *oldpass) {
                dprintf(master, "%s\n", oldpass);
                oldpass_sent = 1;
            }
        } else if (is_new && !newpass_sent) {
            dprintf(master, "%s\n", newpass);
            newpass_sent = 1;
        } else if (is_retype && !confirm_sent) {
            dprintf(master, "%s\n", newpass);
            confirm_sent = 1;
        }

        if (strstr(buf, "You must choose a longer password") || strstr(buf, "too short") || strstr(buf, "too simple")) {
            safe_copy(outmsg, current_trans->err_pwd_short_simple, outlen);
            has_msg_flag = 1;
        }
        if (strstr(buf, "Authentication token manipulation error") || strstr(buf, "Incorrect password") || strstr(buf, "Wrong password") || strstr(buf, "Authentication failure")) {
            safe_copy(outmsg, current_trans->err_pwd_incorrect, outlen);
            has_msg_flag = 1;
        }
        if (strstr(buf, "password updated successfully") || strstr(buf, "successfully updated")) {
            safe_copy(outmsg, current_trans->msg_pwd_changed, outlen);
            has_msg_flag = 1;
        }
    }
    close(master);
    int status = -1;
    waitpid(pid, &status, 0);
    sigaction(SIGCHLD, &old_sa, NULL);

    secure_clear_buffer(buf, sizeof(buf));
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        safe_copy(outmsg, current_trans->msg_pwd_changed, outlen);
        return 0;
    } else {
        if (!has_msg_flag) {
            safe_copy(outmsg, current_trans->err_pwd_change_failed, outlen);
        }
        return -1;
    }
}

static void detect_user_session(const char *username) {
    /* 1. Résolution UID/GID via /etc/passwd */
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd >= 0) {
        char buf[8192];
        int bytes = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (bytes > 0) {
            buf[bytes] = '\0';
            char *line = buf;
            while (line && *line) {
                char *next = strchr(line, '\n');
                if (next) *next++ = '\0';
                
                char *p = line;
                char *user = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                char *uid_str = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                char *gid_str = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0'; // gecos
                char *home_str = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                
                int uid = str_to_int(uid_str);
                int is_match = 0;
                if (username && username[0] && strcmp(user, username) == 0) is_match = 1;
                else if ((!username || !username[0]) && uid >= 1000 && uid < 60000) is_match = 1;
                
                if (is_match) {
                    user_session.uid = uid;
                    user_session.gid = str_to_int(gid_str);
                    safe_copy(user_session.username, user, sizeof(user_session.username));
                    safe_copy(user_session.home, home_str, sizeof(user_session.home));
                    break;
                }
                line = next;
            }
        }
    }
    
    if (user_session.uid == 0) {
        user_session.uid = 1000;
        user_session.gid = 1000;
        safe_copy(user_session.username, "user", sizeof(user_session.username));
        safe_copy(user_session.home, "/home/user", sizeof(user_session.home));
    }
    
    /* 2. Recherche d'un processus graphique pour extraire l'environnement */
    DIR *dir = opendir("/proc");
    if (dir) {
        struct dirent *ent;
        int found = 0;
        while ((ent = readdir(dir)) != NULL && !found) {
            if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                char path[256] = "/proc/";
                safe_concat(path, ent->d_name, sizeof(path));
                
                struct stat st;
                if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                    safe_concat(path, "/environ", sizeof(path));
                    int fd = open(path, O_RDONLY);
                    if (fd >= 0) {
                        char env[32768];
                        ssize_t n = read(fd, env, sizeof(env) - 1);
                        close(fd);
                        if (n > 0) {
                            env[n] = '\0';
                            char *p = env;
                            int is_graphical = 0;
                            while (p < env + n) {
                                char *end = memchr(p, '\0', env + n - p);
                                if (!end) break;
                                if (strncmp(p, "DISPLAY=", 8) == 0 || strncmp(p, "WAYLAND_DISPLAY=", 16) == 0) {
                                    is_graphical = 1;
                                    break;
                                }
                                p = end + 1;
                            }
                            if (is_graphical) {
                                p = env;
                                while (p < env + n) {
                                    char *end = memchr(p, '\0', env + n - p);
                                    if (!end) break;
                                    if (strncmp(p, "DISPLAY=", 8) == 0) safe_copy(user_session.display, p + 8, sizeof(user_session.display));
                                    else if (strncmp(p, "XAUTHORITY=", 11) == 0) safe_copy(user_session.xauthority, p + 11, sizeof(user_session.xauthority));
                                    else if (strncmp(p, "WAYLAND_DISPLAY=", 16) == 0) safe_copy(user_session.wayland_display, p + 16, sizeof(user_session.wayland_display));
                                    else if (strncmp(p, "DBUS_SESSION_BUS_ADDRESS=", 25) == 0) safe_copy(user_session.dbus_addr, p + 25, sizeof(user_session.dbus_addr));
                                    else if (strncmp(p, "XDG_CURRENT_DESKTOP=", 20) == 0) safe_copy(user_session.de_name, p + 20, sizeof(user_session.de_name));
                                    else if (strncmp(p, "DESKTOP_SESSION=", 16) == 0 && !user_session.de_name[0]) safe_copy(user_session.de_name, p + 16, sizeof(user_session.de_name));
                                    else if (strncmp(p, "XDG_RUNTIME_DIR=", 16) == 0) safe_copy(user_session.xdg_runtime_dir, p + 16, sizeof(user_session.xdg_runtime_dir));
                                    else if (strncmp(p, "XDG_SESSION_ID=", 15) == 0) safe_copy(user_session.session_id, p + 15, sizeof(user_session.session_id));
                                    p = end + 1;
                                }
                                
                                // Copie de l'environnement complet pour execute_as_user
                                if (n <= (ssize_t)sizeof(user_session.raw_env)) {
                                    memcpy(user_session.raw_env, env, n);
                                    user_session.raw_env_len = n;
                                } else {
                                    memcpy(user_session.raw_env, env, sizeof(user_session.raw_env));
                                    user_session.raw_env_len = sizeof(user_session.raw_env);
                                }
                                
                                found = 1;
                            }
                        }
                    }
                }
            }
        }
        closedir(dir);
    }
    
    /* 3. Déduction de la commande de lock */
    char *de = user_session.de_name;
    if (strstr(de, "TDE") || strstr(de, "Trinity")) {
        safe_copy(user_session.lock_cmd, "dcop kdesktop KScreensaverIface lock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "KDE") || strstr(de, "PLASMA")) {
        safe_copy(user_session.lock_cmd, "qdbus org.freedesktop.ScreenSaver /ScreenSaver Lock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "GNOME") || strstr(de, "UNITY")) {
        safe_copy(user_session.lock_cmd, "gnome-screensaver-command -l", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "XFCE")) {
        safe_copy(user_session.lock_cmd, "xflock4", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "LXQT")) {
        safe_copy(user_session.lock_cmd, "lxqt-lockscreen", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "LXDE")) {
        safe_copy(user_session.lock_cmd, "lxlock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "MATE")) {
        safe_copy(user_session.lock_cmd, "mate-screensaver-command -l", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "CINNAMON")) {
        safe_copy(user_session.lock_cmd, "cinnamon-screensaver-command -l", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "PANTHEON")) {
        safe_copy(user_session.lock_cmd, "dm-tool lock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "ENLIGHTENMENT")) {
        safe_copy(user_session.lock_cmd, "enlightenment_remote -screen-lock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "sway")) {
        safe_copy(user_session.lock_cmd, "swaylock", sizeof(user_session.lock_cmd));
    } else if (strstr(de, "Hyprland")) {
        safe_copy(user_session.lock_cmd, "hyprlock", sizeof(user_session.lock_cmd));
    } else {
        safe_copy(user_session.lock_cmd, "xdg-screensaver lock", sizeof(user_session.lock_cmd));
    }
}

static void execute_command_safe(const char *cmd, int as_root) {
    if (!cmd || !cmd[0]) return;
    
    pid_t pid = fork();
    if (pid == 0) {
        if (fork() != 0) {
            _exit(0); // Intermediate child exits, leaving grandchild adopted by init
        }
        if (journal.kbd_fd >= 0) close(journal.kbd_fd);
        if (journal.mouse_fd >= 0) close(journal.mouse_fd);
        if (journal.tty_fd >= 0) close(journal.tty_fd);
        if (journal.rescue_tty_fd >= 0) close(journal.rescue_tty_fd);
        if (journal.drm_fd >= 0) close(journal.drm_fd);
        
        if (!as_root) {
            drop_privs_to_user();
        }
        
        char env_home[512] = "HOME=";
        if (!as_root) {
            safe_concat(env_home, user_session.home, sizeof(env_home));
        } else {
            safe_concat(env_home, "/root", sizeof(env_home));
        }
        
        char *envp[512];
        int envc = 0;
        envp[envc++] = env_home;
        envp[envc++] = "PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin:/opt/trinity/bin:/opt/tde/bin";
        
        if (!as_root) {
            char *p = user_session.raw_env;
            while (p < user_session.raw_env + user_session.raw_env_len && envc < 510) {
                if (strncmp(p, "HOME=", 5) != 0 && strncmp(p, "PATH=", 5) != 0) {
                    envp[envc++] = p;
                }
                p += strlen(p) + 1;
            }
        }
        envp[envc] = NULL;
        
        if (busybox_memfd >= 0) {
            char *argv[] = {"sh", "-c", (char*)cmd, NULL};
            safe_fexecve(busybox_memfd, argv, envp);
            close(busybox_memfd);
        }
        write_debug_log("execute_command_safe fallback to /bin/sh\n");
        char *argv[] = {"/bin/sh", "-c", (char*)cmd, NULL};
        execve("/bin/sh", argv, envp);
        write_debug_log("execute_command_safe execution failed completely\n");
        _exit(127);
    } else if (pid > 0) {
        while (waitpid(pid, NULL, 0) < 0) {
            if (errno != EINTR) break;
        }
    }
}

static void execute_as_user(const char *cmd) {
    execute_command_safe(cmd, 0);
}

static char sys_info_str[256] = {0};

struct cad_iwreq {
    union {
        char ifrn_name[16];
    } ifr_ifrn;
    union {
        struct {
            void *pointer;
            unsigned short length;
            unsigned short flags;
        } essid;
    } u;
};

#define CAD_SIOCGIWESSID 0x8B1B

static void get_wifi_ssid(char *ssid_out, size_t max_len) {
    ssid_out[0] = '\0';
    
    int fd = open("/proc/net/wireless", O_RDONLY);
    if (fd < 0) return;
    
    char buf[1024];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';
    
    char *line = buf;
    while (line && *line) {
        char *next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }
        
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *iface = line;
            while (*iface == ' ' || *iface == '\t') iface++;
            
            if (iface[0] != '\0') {
                struct cad_iwreq wrq;
                char essid[33];
                memset(essid, 0, sizeof(essid));
                memset(&wrq, 0, sizeof(wrq));
                
                strncpy(wrq.ifr_ifrn.ifrn_name, iface, 15);
                wrq.u.essid.pointer = essid;
                wrq.u.essid.length = sizeof(essid) - 1;
                
                int sock = socket(AF_INET, SOCK_DGRAM, 0);
                if (sock >= 0) {
                    if (ioctl(sock, CAD_SIOCGIWESSID, &wrq) >= 0) {
                        if (essid[0] != '\0') {
                            safe_copy(ssid_out, essid, max_len);
                            close(sock);
                            break;
                        }
                    }
                    close(sock);
                }
            }
        }
        line = next_line;
    }
}

static void init_static_info() {
    char hostname[256] = "unknown";
    struct utsname uts;
    if (uname(&uts) == 0) {
        safe_copy(hostname, uts.nodename, sizeof(hostname));
    }
    
    const char *user = "root";
    extern char **environ;
    if (environ) {
        for (char **ep = environ; *ep != NULL; ep++) {
            if (strncmp(*ep, "SUDO_USER=", 10) == 0) {
                user = *ep + 10;
                break;
            } else if (strncmp(*ep, "USER=", 5) == 0 && strcmp(user, "root") == 0) {
                user = *ep + 5;
            }
        }
    }
    
    detect_user_session(user);
    
    char cached_user_host[256] = {0};
    safe_copy(cached_user_host, user, sizeof(cached_user_host));
    safe_concat(cached_user_host, " @ ", sizeof(cached_user_host));
    safe_concat(cached_user_host, hostname, sizeof(cached_user_host));

    /* Récupération de l'IP (une seule fois au démarrage) */
    char ip[32] = "";
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        struct ifconf ifc;
        char buf[1024];
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = buf;
        if (ioctl(fd, SIOCGIFCONF, &ifc) >= 0) {
            struct ifreq *ifr = ifc.ifc_req;
            int n = ifc.ifc_len / sizeof(struct ifreq);
            for (int i = 0; i < n; i++) {
                if (strcmp(ifr[i].ifr_name, "lo") != 0) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)&ifr[i].ifr_addr;
                    uint32_t addr = ntohl(sa->sin_addr.s_addr);
                    char p1[4], p2[4], p3[4], p4[4];
                    int_to_str((addr >> 24) & 0xFF, p1);
                    int_to_str((addr >> 16) & 0xFF, p2);
                    int_to_str((addr >> 8) & 0xFF, p3);
                    int_to_str(addr & 0xFF, p4);
                    safe_copy(ip, p1, sizeof(ip)); safe_concat(ip, ".", sizeof(ip)); safe_concat(ip, p2, sizeof(ip)); safe_concat(ip, ".", sizeof(ip)); safe_concat(ip, p3, sizeof(ip)); safe_concat(ip, ".", sizeof(ip)); safe_concat(ip, p4, sizeof(ip));
                    break;
                }
            }
        }
        close(fd);
    }
    
    char ssid[33] = "";
    get_wifi_ssid(ssid, sizeof(ssid));
    
    if (ip[0] != '\0') {
        safe_copy(sys_info_str, cached_user_host, sizeof(sys_info_str));
        safe_concat(sys_info_str, " : ", sizeof(sys_info_str));
        safe_concat(sys_info_str, ip, sizeof(sys_info_str));
        if (ssid[0] != '\0') {
            safe_concat(sys_info_str, " (", sizeof(sys_info_str));
            safe_concat(sys_info_str, ssid, sizeof(sys_info_str));
            safe_concat(sys_info_str, ")", sizeof(sys_info_str));
        }
    } else {
        safe_copy(sys_info_str, cached_user_host, sizeof(sys_info_str));
    }
}

struct btn_style_snapshot {
    struct nk_style_item normal;
    struct nk_style_item hover;
    struct nk_style_item active;
    struct nk_color text_normal;
    struct nk_color text_hover;
    struct nk_color text_active;
    nk_flags text_alignment;
};

static void btn_style_save(struct nk_context *ctx, struct btn_style_snapshot *s) {
    s->normal = ctx->style.button.normal;
    s->hover = ctx->style.button.hover;
    s->active = ctx->style.button.active;
    s->text_normal = ctx->style.button.text_normal;
    s->text_hover = ctx->style.button.text_hover;
    s->text_active = ctx->style.button.text_active;
    s->text_alignment = ctx->style.button.text_alignment;
}

static void btn_style_restore(struct nk_context *ctx, const struct btn_style_snapshot *s) {
    ctx->style.button.normal = s->normal;
    ctx->style.button.hover = s->hover;
    ctx->style.button.active = s->active;
    ctx->style.button.text_normal = s->text_normal;
    ctx->style.button.text_hover = s->text_hover;
    ctx->style.button.text_active = s->text_active;
    ctx->style.button.text_alignment = s->text_alignment;
}

void set_windows10_style(struct nk_context *ctx) {
    /* rawfb ne fait pas de blending alpha sur les rectangles pleins.
       On pré-calcule les couleurs opaques de survol par-dessus le fond (32, 80, 129). */
    struct nk_color hover_color = THEME_COLOR_HOVER; // Équivalent à 255,255,255 à 40%
    struct nk_color active_color = THEME_COLOR_ACTIVE; // Équivalent à 255,255,255 à 60%
    struct nk_color text_color = nk_rgba(255, 255, 255, 255);

    /* Fenêtre transparente sans bordure */
    ctx->style.window.background = nk_rgba(0, 0, 0, 0);
    ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.window.border = 0.0f;
    ctx->style.window.padding = nk_vec2(0, 0);

    /* Boutons transparents par défaut (Flat design) */
    ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.button.hover = nk_style_item_color(hover_color);
    ctx->style.button.active = nk_style_item_color(active_color);
    ctx->style.button.border = 0.0f;
    ctx->style.button.rounding = 0.0f;
    ctx->style.button.padding = nk_vec2(20, 15); // Confortable à cliquer
    ctx->style.button.text_normal = text_color;
    ctx->style.button.text_hover = text_color;
    ctx->style.button.text_active = text_color;
    ctx->style.button.text_alignment = NK_TEXT_CENTERED; // Centrer le texte dans le bouton

    /* Champ texte (Edit) */
    ctx->style.edit.normal = nk_style_item_color(nk_rgba(30, 30, 30, 255));
    ctx->style.edit.hover = nk_style_item_color(nk_rgba(40, 40, 40, 255));
    ctx->style.edit.active = nk_style_item_color(nk_rgba(0, 0, 0, 255));
    ctx->style.edit.border_color = nk_rgba(255, 255, 255, 255); // Bordure blanche au focus
    ctx->style.edit.border = 1.0f;
    ctx->style.edit.rounding = 0.0f;
    ctx->style.edit.text_normal = text_color;
    ctx->style.edit.text_hover = text_color;
    ctx->style.edit.text_active = text_color;
}

/* Wrapper autour de ioctl pour gérer les interruptions (EINTR/EAGAIN) */
int drm_ioctl(int fd, unsigned long request, void *arg) {
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    return ret;
}

typedef enum {
    STATE_INIT,
    STATE_TAKEOVER_VT,
    STATE_TAKEOVER_DRM,
    STATE_INIT_INPUT,
    STATE_RUNNING,
    STATE_RESTORE,
    STATE_FAILED,
    STATE_PANIC
} cad_state_t;

static volatile cad_state_t current_state = STATE_INIT;

/* État global du moteur de rendu */
struct {
    uint32_t conn_id;
    uint32_t crtc_id;
    struct drm_mode_modeinfo mode;
    
    uint32_t width, height;
    uint32_t pitch, size;
    uint32_t handle[2];
    uint8_t *buffer[2];
    uint32_t fb_id[2];
    
    struct drm_mode_crtc saved_crtc;
    int double_buffered;
} drm;

static void trigger_sysrq(char key) {
    int fd = open("/proc/sysrq-trigger", O_WRONLY);
    if (fd >= 0) {
        if (write(fd, &key, 1) < 0) {
            /* ignore write error */
        }
        close(fd);
    }
}

static void execute_sleep(int active_fb_id) {
    DEBUG_LOG("[POWER] Entering Sleep mode (suspend to RAM)...");
    
    /* Verrouiller la session utilisateur avant la mise en veille */
    if (user_session.lock_cmd[0] != '\0') {
        DEBUG_LOG("[POWER] Locking user session...");
        execute_as_user(user_session.lock_cmd);
        usleep(200000); /* Laisser 200ms pour laisser le verrouilleur s'initialiser */
    }

    /* Forker le sync pour éviter de bloquer indéfiniment si un disque est en panne (D-state) */
    pid_t sync_pid = fork();
    if (sync_pid == 0) {
        sync();
        _exit(0);
    }
    
    int status;
    for (int i = 0; i < 20; i++) {
        int ret = waitpid(sync_pid, &status, WNOHANG);
        if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
        usleep(100000); /* 100ms */
    }
    
    /* Tuer le sync s'il a dépassé 2s pour ne pas bloquer la mise en veille */
    if (sync_pid > 0) {
        kill(sync_pid, 9);
        for (int retry = 0; retry < 50; retry++) {
            int ret = waitpid(sync_pid, NULL, WNOHANG);
            if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
            usleep(1000);
        }
    }

    int fd = open("/sys/power/state", O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "mem", 3) < 0) {
            DEBUG_LOG("[ERROR] Failed to write to /sys/power/state");
        }
        close(fd);
    } else {
        DEBUG_LOG("[ERROR] Failed to open /sys/power/state");
    }
    
    /* --- CODE DE REPRISE DE VEILLE --- */
    DEBUG_LOG("[POWER] Resumed from Sleep mode. Restoring graphics mode...");
    if (journal.drm_fd >= 0) {
        struct drm_mode_crtc crtc = {0};
        crtc.crtc_id = drm.crtc_id;
        crtc.fb_id = active_fb_id;
        crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&drm.conn_id;
        crtc.count_connectors = 1;
        crtc.mode = drm.mode;
        crtc.mode_valid = 1;
        
        usleep(100000); // 100ms pour laisser le GPU se réinitialiser
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
            DEBUG_LOG("[ERROR] Failed to restore CRTC after sleep, retrying...");
            usleep(500000); // 500ms
            if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
                DEBUG_LOG("[ERROR] Failed to restore CRTC after sleep on retry");
            }
        }
    }
}

static void execute_restart(void) {
    /* Ignorer SIGTERM pour que systemd ne tue pas notre processus de secours
     * au début de sa procédure d'extinction (évite de bloquer si systemd se fige ensuite). */
    signal(SIGTERM, SIG_IGN);

    /* Libérer les descripteurs de fichiers hérités (en particulier le DRM Master)
     * pour éviter de bloquer le serveur X pendant la procédure d'extinction. */
    if (journal.drm_fd >= 0) {
        ioctl(journal.drm_fd, DRM_IOCTL_DROP_MASTER, 0);
        close(journal.drm_fd);
    }
    if (journal.kbd_fd >= 0) close(journal.kbd_fd);
    if (journal.mouse_fd >= 0) close(journal.mouse_fd);
    if (journal.rescue_tty_fd >= 0) close(journal.rescue_tty_fd);
    if (journal.tty_fd >= 0) close(journal.tty_fd);

    DEBUG_LOG("[POWER] Restart requested. Synchronizing disks (forked).");
    pid_t sync_pid = fork();
    if (sync_pid == 0) {
        sync();
        _exit(0);
    }
    int status;
    for (int i = 0; i < 20; i++) {
        int ret = waitpid(sync_pid, &status, WNOHANG);
        if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
        usleep(100000);
    }
    if (sync_pid > 0) {
        kill(sync_pid, 9);
        for (int retry = 0; retry < 50; retry++) {
            int ret = waitpid(sync_pid, NULL, WNOHANG);
            if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
            usleep(1000);
        }
    }
    
    /* Tentative 1 : Commande gracieuse systemd/sysvinit */
    pid_t pid = fork();
    if (pid == 0) {
        char *cmd = "systemctl reboot || reboot";
        char *argv[] = {"sh", "-c", cmd, NULL};
        char *envp[] = {"PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL};
        if (busybox_memfd >= 0) {
            safe_fexecve(busybox_memfd, argv, envp);
        }
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    
    /* Délai de grâce de 6 secondes (adapté aux systèmes lents ou saturés) */
    sleep(6);
    
    /* Tentative 2 : Fallback d'urgence matériel ultra-sécurisé via SysRq 'u' + reboot syscall. */
    DEBUG_LOG("[POWER] Graceful restart timed out. Forcing clean SysRq emergency reboot.");
    sync_pid = fork();
    if (sync_pid == 0) {
        sync();
        _exit(0);
    }
    for (int i = 0; i < 20; i++) {
        int ret = waitpid(sync_pid, &status, WNOHANG);
        if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
        usleep(100000);
    }
    if (sync_pid > 0) {
        kill(sync_pid, 9);
        for (int retry = 0; retry < 50; retry++) {
            int ret = waitpid(sync_pid, NULL, WNOHANG);
            if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
            usleep(1000);
        }
    }
    sleep(1);
    trigger_sysrq('u'); /* Remount all filesystems read-only */
    sleep(1);
    
    /* Appel système brutal de redémarrage */
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL);
    _exit(0);
}

static void execute_shutdown(void) {
    /* Ignorer SIGTERM pour que systemd ne tue pas notre processus de secours. */
    signal(SIGTERM, SIG_IGN);

    /* Libérer les descripteurs de fichiers hérités (en particulier le DRM Master)
     * pour éviter de bloquer le serveur X pendant la procédure d'extinction. */
    if (journal.drm_fd >= 0) {
        ioctl(journal.drm_fd, DRM_IOCTL_DROP_MASTER, 0);
        close(journal.drm_fd);
    }
    if (journal.kbd_fd >= 0) close(journal.kbd_fd);
    if (journal.mouse_fd >= 0) close(journal.mouse_fd);
    if (journal.rescue_tty_fd >= 0) close(journal.rescue_tty_fd);
    if (journal.tty_fd >= 0) close(journal.tty_fd);

    DEBUG_LOG("[POWER] Shutdown requested. Synchronizing disks (forked).");
    pid_t sync_pid = fork();
    if (sync_pid == 0) {
        sync();
        _exit(0);
    }
    int status;
    for (int i = 0; i < 20; i++) {
        int ret = waitpid(sync_pid, &status, WNOHANG);
        if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
        usleep(100000);
    }
    if (sync_pid > 0) {
        kill(sync_pid, 9);
        for (int retry = 0; retry < 50; retry++) {
            int ret = waitpid(sync_pid, NULL, WNOHANG);
            if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
            usleep(1000);
        }
    }
    
    /* Tentative 1 : Commande gracieuse systemd/sysvinit */
    pid_t pid = fork();
    if (pid == 0) {
        char *cmd = "systemctl poweroff || poweroff";
        char *argv[] = {"sh", "-c", cmd, NULL};
        char *envp[] = {"PATH=/sbin:/usr/sbin:/bin:/usr/bin", NULL};
        if (busybox_memfd >= 0) {
            safe_fexecve(busybox_memfd, argv, envp);
        }
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }
    
    /* Délai de grâce de 6 secondes (adapté aux systèmes lents ou saturés) */
    sleep(6);
    
    /* Tentative 2 : Fallback d'urgence matériel ultra-sécurisé via SysRq 'u' + poweroff syscall. */
    DEBUG_LOG("[POWER] Graceful shutdown timed out. Forcing clean SysRq emergency poweroff.");
    sync_pid = fork();
    if (sync_pid == 0) {
        sync();
        _exit(0);
    }
    for (int i = 0; i < 20; i++) {
        int ret = waitpid(sync_pid, &status, WNOHANG);
        if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
        usleep(100000);
    }
    if (sync_pid > 0) {
        kill(sync_pid, 9);
        for (int retry = 0; retry < 50; retry++) {
            int ret = waitpid(sync_pid, NULL, WNOHANG);
            if (ret == sync_pid || (ret < 0 && errno == ECHILD)) break;
            usleep(1000);
        }
    }
    sleep(1);
    trigger_sysrq('u'); /* Remount all filesystems read-only */
    sleep(1);
    
    /* Appel système brutal d'extinction */
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
    _exit(0);
}

static void execute_sign_out(void) {
    /* CRITIQUE : Fermer immédiatement tous les fd hérités du parent (fork).
     * En particulier le fd DRM avec statut Master, car tant qu'il est ouvert,
     * le kernel refuse de donner le DRM Master au nouveau serveur X lancé par
     * le display manager après le logout. Résultat sans ce cleanup : écran noir
     * avec juste le pointeur de la souris visible. */
    if (journal.drm_fd >= 0) {
        ioctl(journal.drm_fd, DRM_IOCTL_DROP_MASTER, 0);
        close(journal.drm_fd);
    }
    if (journal.kbd_fd >= 0) close(journal.kbd_fd);
    if (journal.mouse_fd >= 0) close(journal.mouse_fd);
    if (journal.rescue_tty_fd >= 0) close(journal.rescue_tty_fd);
    /* Note : on ne ferme pas journal.tty_fd car on en a besoin pour surveiller le VT */
    
    /* 1. Attente déterministe du retour sur le VT de la session graphique */
    for (int i = 0; i < 50; i++) {
        struct vt_stat vts;
        int tty_fd = open("/dev/tty0", O_RDONLY);
        if (tty_fd >= 0) {
            if (ioctl(tty_fd, VT_GETSTATE, &vts) == 0) {
                if (vts.v_active == journal.original_vt) {
                    close(tty_fd);
                    break;
                }
            }
            close(tty_fd);
        }
        usleep(100000); // 100ms
    }
    
    /* Fermer le tty_fd hérité maintenant qu'on n'en a plus besoin */
    if (journal.tty_fd >= 0) close(journal.tty_fd);
    
    /* Laisser le serveur X/Wayland reprendre pleinement le contrôle du VT */
    usleep(500000); // 500ms
    
    /* Tentative 1 (PRIORITAIRE) : Commande de logout native du Desktop Environment.
     * C'est la méthode la plus propre : elle passe par le gestionnaire de session
     * du bureau qui ferme les applications proprement, sauvegarde l'état de session,
     * et termine le serveur X/Wayland de manière ordonnée. */
    const char *de_cmd = NULL;
    char *de = user_session.de_name;
    
    if (strstr(de, "TDE") || strstr(de, "Trinity")) {
        de_cmd = "dcop ksmserver ksmserver logout 0 0 0";
    } else if (strstr(de, "KDE") || strstr(de, "PLASMA")) {
        de_cmd = "qdbus org.kde.ksmserver /KSMServer logout 0 0 0";
    } else if (strstr(de, "GNOME") || strstr(de, "UNITY") || strstr(de, "PANTHEON") || strstr(de, "Budgie") || strstr(de, "BUDGIE")) {
        de_cmd = "gnome-session-quit --logout --no-prompt --force";
    } else if (strstr(de, "XFCE")) {
        de_cmd = "xfce4-session-logout --logout --fast";
    } else if (strstr(de, "CINNAMON")) {
        de_cmd = "cinnamon-session-quit --logout --no-prompt";
    } else if (strstr(de, "MATE")) {
        de_cmd = "mate-session-save --logout --force-logout";
    } else if (strstr(de, "LXQT")) {
        de_cmd = "lxqt-leave --logout";
    } else if (strstr(de, "LXDE")) {
        de_cmd = "pkill lxsession";
    } else if (strstr(de, "sway")) {
        de_cmd = "swaymsg exit";
    } else if (strstr(de, "Hyprland")) {
        de_cmd = "hyprctl dispatch exit";
    } else if (strstr(de, "i3")) {
        de_cmd = "i3-msg exit";
    } else if (strstr(de, "openbox")) {
        de_cmd = "openbox --exit";
    } else if (strstr(de, "ENLIGHTENMENT")) {
        de_cmd = "enlightenment_remote -logout";
    }
    
    if (de_cmd) {
        /* Lancer la commande native du bureau en tant qu'utilisateur.
         * execute_as_user() fork et retourne immédiatement dans le parent.
         * On laisse ensuite le bureau gérer sa fermeture proprement.
         * Timeout généreux de 15 secondes avant de considérer un échec. */
        execute_as_user(de_cmd);
        
        /* Attendre que la session se ferme naturellement (max 15 secondes) */
        for (int sec = 0; sec < 150; sec++) {
            usleep(100000); // 100ms
            int user_procs = 0;
            DIR *dir = opendir("/proc");
            if (dir) {
                struct dirent *ent;
                while ((ent = readdir(dir)) != NULL) {
                    if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                        char path[256] = "/proc/";
                        safe_concat(path, ent->d_name, sizeof(path));
                        struct stat st;
                        if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                            pid_t pid = str_to_int(ent->d_name);
                            if (pid != getpid() && pid != getppid()) {
                                user_procs++;
                                if (user_procs > 2) break; /* Plus de 2 processus = session encore active */
                            }
                        }
                    }
                }
                closedir(dir);
            }
            /* Si plus aucun (ou quasi aucun) processus utilisateur, le logout a réussi */
            if (user_procs <= 2) {
                _exit(0);
            }
        }
        /* Si on arrive ici, le logout gracieux n'a pas abouti en 15 secondes.
         * On passe au loginctl en fallback. */
    }
    
    /* Tentative 2 (FALLBACK) : loginctl terminate-session / terminate-user.
     * Utilisé uniquement si la commande native du bureau n'existe pas
     * ou si elle n'a pas réussi à fermer la session en 15 secondes. */
    if (user_session.session_id[0] || user_session.username[0]) {
        pid_t pid = fork();
        if (pid == 0) {
            char *argv[5];
            int argc = 0;
            argv[argc++] = "/usr/bin/loginctl";
            if (user_session.session_id[0]) {
                argv[argc++] = "terminate-session";
                argv[argc++] = user_session.session_id;
            } else {
                argv[argc++] = "terminate-user";
                argv[argc++] = user_session.username;
            }
            argv[argc] = NULL;
            execv("/usr/bin/loginctl", argv);
            _exit(127);
        }
        int status;
        if (waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            /* loginctl a réussi, attendre 10s que la session se ferme */
            for (int sec = 0; sec < 100; sec++) {
                usleep(100000);
                int user_procs = 0;
                DIR *dir = opendir("/proc");
                if (dir) {
                    struct dirent *ent;
                    while ((ent = readdir(dir)) != NULL) {
                        if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                            char path[256] = "/proc/";
                            safe_concat(path, ent->d_name, sizeof(path));
                            struct stat st;
                            if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                                pid_t pid2 = str_to_int(ent->d_name);
                                if (pid2 != getpid() && pid2 != getppid()) {
                                    user_procs++;
                                    if (user_procs > 2) break;
                                }
                            }
                        }
                    }
                    closedir(dir);
                }
                if (user_procs <= 2) {
                    _exit(0);
                }
            }
        }
    }
    
    /* Tentative 3 (FAILSAFE ULTIME) : Tuer les processus de l'utilisateur manuellement.
     * Utilisé uniquement si RIEN d'autre n'a fonctionné. */
    pid_t self_pid = getpid();
    pid_t parent_pid = getppid();
    
    /* Phase 1 : SIGTERM à tous les processus de l'UID */
    DIR *dir = opendir("/proc");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                char path[256] = "/proc/";
                safe_concat(path, ent->d_name, sizeof(path));
                struct stat st;
                if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                    pid_t pid = str_to_int(ent->d_name);
                    if (pid != self_pid && pid != parent_pid) {
                        kill(pid, 15); // SIGTERM
                    }
                }
            }
        }
        closedir(dir);
    }
    
    /* Attendre 3 secondes que les processus se terminent */
    for (int sec = 0; sec < 30; sec++) {
        usleep(100000);
        int still_running = 0;
        dir = opendir("/proc");
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                    char path[256] = "/proc/";
                    safe_concat(path, ent->d_name, sizeof(path));
                    struct stat st;
                    if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                        pid_t pid = str_to_int(ent->d_name);
                        if (pid != self_pid && pid != parent_pid) {
                            still_running = 1;
                            break;
                        }
                    }
                }
            }
            closedir(dir);
        }
        if (!still_running) break;
    }
    
    /* Phase 2 : SIGKILL aux récalcitrants */
    dir = opendir("/proc");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                char path[256] = "/proc/";
                safe_concat(path, ent->d_name, sizeof(path));
                struct stat st;
                if (stat(path, &st) == 0 && (int)st.st_uid == user_session.uid) {
                    pid_t pid = str_to_int(ent->d_name);
                    if (pid != self_pid && pid != parent_pid) {
                        kill(pid, 9); // SIGKILL
                    }
                }
            }
        }
        closedir(dir);
    }
    
    _exit(0);
}

static int read_int_from_file(const char *path, int *val) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[32];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = '\0';
        *val = str_to_int(buf);
        return 1;
    }
    return 0;
}

static void write_int_to_file(const char *path, int val) {
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        char buf[32];
        int_to_str(val, buf);
        safe_concat(buf, "\n", sizeof(buf));
        
        int len = strlen(buf);
        int written = 0;
        /* Boucle de retry pour sysfs (au cas improbable où le write serait partiel) */
        while (written < len) {
            int ret = write(fd, buf + written, len - written);
            if (ret <= 0) break;
            written += ret;
        }
        close(fd);
    }
}

/* ---- GESTION DU BACKLIGHT D'URGENCE ---- */
struct backlight_saved_state {
    char path[256];
    int saved_brightness;
    int enforced_brightness;
    int has_enforced;
};

#define MAX_BACKLIGHTS 4
struct backlight_saved_state backlights[MAX_BACKLIGHTS];
int num_backlights = 0;

struct linux_dirent64 {
    uint64_t         d_ino;
    int64_t          d_off;
    unsigned short   d_reclen;
    unsigned char    d_type;
    char             d_name[];
};

void force_backlight() {
    int dir_fd = open("/sys/class/backlight", O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return;
    
    char buf[1024];
    int nread;
    while ((nread = syscall(SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            if (d->d_reclen == 0) break;
            if (d->d_name[0] != '.') {
                if (num_backlights >= MAX_BACKLIGHTS) break;
                
                char path_max[256], path_cur[256];
                safe_copy(path_max, "/sys/class/backlight/", sizeof(path_max)); safe_concat(path_max, d->d_name, sizeof(path_max)); safe_concat(path_max, "/max_brightness", sizeof(path_max));
                safe_copy(path_cur, "/sys/class/backlight/", sizeof(path_cur)); safe_concat(path_cur, d->d_name, sizeof(path_cur)); safe_concat(path_cur, "/brightness", sizeof(path_cur));
                
                int max_b = 0, cur_b = 0;
                if (read_int_from_file(path_max, &max_b) && read_int_from_file(path_cur, &cur_b)) {
                    safe_copy(backlights[num_backlights].path, path_cur, sizeof(backlights[num_backlights].path));
                    backlights[num_backlights].saved_brightness = cur_b;
                    
                    if (cur_b <= (max_b / 20)) {
                        int safe_b = max_b / 3;
                        if (safe_b == 0) safe_b = 1;
                        backlights[num_backlights].enforced_brightness = safe_b;
                        backlights[num_backlights].has_enforced = 1;
                        write_int_to_file(path_cur, safe_b);
                        DEBUG_LOG("[DEBUG] Rallumage d'urgence du backlight.");
                    } else {
                        backlights[num_backlights].enforced_brightness = cur_b;
                        backlights[num_backlights].has_enforced = 1;
                    }
                    num_backlights++;
                }
            }
            bpos += d->d_reclen;
        }
    }
    close(dir_fd);
}

void maintain_backlight() {
    for (int i = 0; i < num_backlights; i++) {
        if (backlights[i].has_enforced) {
            int cur_b = 0;
            if (read_int_from_file(backlights[i].path, &cur_b)) {
                if (cur_b != backlights[i].enforced_brightness) {
                    write_int_to_file(backlights[i].path, backlights[i].enforced_brightness);
                }
            }
        }
    }
}

void restore_backlight() {
    for (int i = 0; i < num_backlights; i++) {
        write_int_to_file(backlights[i].path, backlights[i].saved_brightness);
        DEBUG_LOG("[DEBUG] Restauration du backlight.");
    }
}

/* ---- GESTION DU SYSRQ ---- */
static int saved_sysrq = -1;

static void disable_sysrq() {
    if (read_int_from_file("/proc/sys/kernel/sysrq", &saved_sysrq)) {
        write_int_to_file("/proc/sys/kernel/sysrq", 0);
        DEBUG_LOG_INT("[DEBUG] SysRq désactivé (sauvegardé: ", saved_sysrq);
    }
}

static void restore_sysrq() {
    if (saved_sysrq >= 0) {
        
        write_int_to_file("/proc/sys/kernel/sysrq", saved_sysrq);

        saved_sysrq = -1;
    }
}

/* Routine pour voler l'écran à X11 via le sous-système VT */
int steal_vt() {
    current_state = STATE_TAKEOVER_VT;
    journal.tty_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (journal.tty_fd < 0) {
        DEBUG_LOG("[ERROR] [DEBUG] Impossible d'ouvrir /dev/tty0 (Besoin d'être root)");
        return -1;
    }

    struct vt_stat vts;
    if (ioctl(journal.tty_fd, VT_GETSTATE, &vts) < 0) {
        DEBUG_LOG("[ERROR] [DEBUG] Échec de VT_GETSTATE");
        close(journal.tty_fd);
        return -1;
    }
    journal.original_vt = vts.v_active;
    DEBUG_LOG_INT("[DEBUG] VT Actif de X11/Wayland : ", journal.original_vt);

    /* Demander au noyau le premier VT libre */
    int free_vt = 12; // Valeur de repli par défaut
    if (ioctl(journal.tty_fd, VT_OPENQRY, &free_vt) < 0 || free_vt <= 0) {
        DEBUG_LOG_INT("[DEBUG] Échec de VT_OPENQRY, utilisation du VT par défaut : ", free_vt);
    }
    journal.rescue_vt = free_vt;

    /* Ouverture isolée du TTY de secours pour appliquer les modes sans toucher à X11 */
    char rescue_tty_path[32];
    safe_copy(rescue_tty_path, "/dev/tty", sizeof(rescue_tty_path)); char num[16]; int_to_str(journal.rescue_vt, num); safe_concat(rescue_tty_path, num, sizeof(rescue_tty_path));
    journal.rescue_tty_fd = open(rescue_tty_path, O_RDWR | O_CLOEXEC);

    /* Bascule de force sur le VT vierge (pour expulser X11) */
    DEBUG_LOG_INT("[DEBUG] Kidnapping de l'écran... Bascule de force sur le VT : ", journal.rescue_vt);
    if (ioctl(journal.tty_fd, VT_ACTIVATE, journal.rescue_vt) < 0) {
        DEBUG_LOG("[ERROR] [DEBUG] Échec de VT_ACTIVATE");
        if (journal.rescue_tty_fd >= 0) close(journal.rescue_tty_fd);
        if (journal.tty_fd >= 0) close(journal.tty_fd);
        return -1;
    }
    
    /* On attend que le noyau confirme la bascule (X11 perd son Master) */
    ioctl(journal.tty_fd, VT_WAITACTIVE, journal.rescue_vt);
    
    if (journal.rescue_tty_fd >= 0) {
        /* On passe NOTRE TTY en mode graphique pour cacher le curseur texte clignotant */
        ioctl(journal.rescue_tty_fd, KDSETMODE, KD_GRAPHICS);
        /* Forcer le rallumage physique de l'écran (Unblank) en cas de mise en veille (DPMS) */
        char unblank = 4;
        ioctl(journal.rescue_tty_fd, TIOCLINUX, &unblank);
    }
    
    DEBUG_LOG("[DEBUG] Le noyau a révoqué les droits de X11.");
    return 0;
}

static volatile int vt_restored = 0;

void restore_vt() {
    if (__atomic_test_and_set(&vt_restored, __ATOMIC_SEQ_CST)) return;
    current_state = STATE_RESTORE;

    /* 1. Restaurer l'état clavier/texte UNIQUEMENT sur notre TTY de secours */
    if (journal.rescue_tty_fd >= 0) {
        syscall(SYS_ioctl, journal.rescue_tty_fd, KDSETMODE, KD_TEXT);
        ioctl(journal.rescue_tty_fd, KDSKBMODE, K_XLATE); // Sane translation mode
        
        struct termios term;
        if (tcgetattr(journal.rescue_tty_fd, &term) == 0) {
            term.c_lflag |= (ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
            term.c_iflag |= (BRKINT | ICRNL | IXON | IMAXBEL);
            term.c_oflag |= (OPOST | ONLCR);
            term.c_cflag |= (CS8 | CREAD);
            tcsetattr(journal.rescue_tty_fd, TCSANOW, &term);
        }
        
        /* Purge matérielle ANSI (RIS) */
        if (write(journal.rescue_tty_fd, "\033c", 2) < 0) { /* ignore error */ }
        close(journal.rescue_tty_fd);
        journal.rescue_tty_fd = -1;
    }

    if (journal.tty_fd >= 0) {
        /* Réactivation des logs console du noyau */
        klogctl(7, NULL, 0);
        
        /* On rebascule sur le VT d'origine de X11 */
        DEBUG_LOG_INT("[DEBUG] Restitution de l'écran à X11 (Retour au VT) : ", journal.original_vt);
        ioctl(journal.tty_fd, VT_ACTIVATE, journal.original_vt);
        ioctl(journal.tty_fd, VT_WAITACTIVE, journal.original_vt);
        
        /* Restauration de la luminosité et du SysRq */
        restore_backlight();
        restore_sysrq();
        
        close(journal.tty_fd);
        journal.tty_fd = -1;
    }
}

static volatile sig_atomic_t got_fatal_signal = 0;

void emergency_restore(int sig) {
    (void)sig;
    got_fatal_signal = 1;
    current_state = STATE_PANIC;
    if (journal.rescue_tty_fd >= 0) {
        /* Async-signal-safe recovery minimal */
        syscall(SYS_ioctl, journal.rescue_tty_fd, KDSETMODE, KD_TEXT);
    }
    if (journal.tty_fd >= 0) {
        /* Fallback si le crash a eu lieu pendant steal_vt avant l'ouverture de rescue_tty_fd */
        syscall(SYS_ioctl, journal.tty_fd, KDSETMODE, KD_TEXT);
        // Tenter de rebasculer sur le VT d'origine
        if (journal.original_vt > 0) {
            syscall(SYS_ioctl, journal.tty_fd, VT_ACTIVATE, journal.original_vt);
        }
    } else if (journal.rescue_tty_fd >= 0) {
        if (journal.original_vt > 0) {
            syscall(SYS_ioctl, journal.rescue_tty_fd, VT_ACTIVATE, journal.original_vt);
        }
    }
    
    // Restaurer SysRq (écriture brute via syscall)
    if (saved_sysrq >= 0) {
        int sfd = syscall(SYS_open, "/proc/sys/kernel/sysrq", O_WRONLY, 0);
        if (sfd >= 0) {
            char c = '0' + (saved_sysrq % 10);
            syscall(SYS_write, sfd, &c, 1);
            syscall(SYS_close, sfd);
        }
    }
    _exit(1);
}

int init_drm() {
    int found_card = 0;
    
    current_state = STATE_TAKEOVER_DRM;
    /* Avant de toucher à DRM, on vole le VT pour éjecter X11 */
    if (steal_vt() < 0) {
        current_state = STATE_FAILED;
        return -1;
    }

    DEBUG_LOG("[DEBUG] Début de la recherche DRM...");
    for (int card_idx = 0; card_idx < 16 && !found_card; card_idx++) {
        char card_path[32];
        safe_copy(card_path, "/dev/dri/card", sizeof(card_path)); char num[16]; int_to_str(card_idx, num); safe_concat(card_path, num, sizeof(card_path));
        
        journal.drm_fd = open(card_path, O_RDWR | O_CLOEXEC);
        if (journal.drm_fd < 0) {
            if (errno != ENOENT) DEBUG_LOG("[DEBUG] Échec ouverture carte DRM.");
            continue;
        }

        /* Le SET_MASTER se fera plus tard, uniquement si la carte possède un connecteur valide */

        DEBUG_LOG("[DEBUG] Carte DRM ouverte avec succès.");

        /* Vérifier la capacité Dumb Buffer */
        struct drm_get_cap cap = { .capability = DRM_CAP_DUMB_BUFFER };
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_GET_CAP, &cap) < 0 || cap.value == 0) {
            DEBUG_LOG("[DEBUG] Pas de Dumb Buffer.");
            close(journal.drm_fd);
            journal.drm_fd = -1;
            continue;
        }

        /* 1. Récupérer les ressources (Connecteurs, CRTCs) */
        struct drm_mode_card_res res = {0};
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
            DEBUG_LOG("[DEBUG] Échec GETRESOURCES.");
            close(journal.drm_fd);
            journal.drm_fd = -1;
            continue;
        }
        
        if (res.count_connectors == 0) {
            DEBUG_LOG("[DEBUG] 0 connecteurs trouvés.");
            close(journal.drm_fd);
            journal.drm_fd = -1;
            continue;
        }
        
        DEBUG_LOG("[DEBUG] Plusieurs connecteurs trouvés.");
        
        uint32_t *conn_ids = malloc(res.count_connectors * sizeof(uint32_t));
        uint32_t *crtc_ids = malloc(res.count_crtcs * sizeof(uint32_t));
        uint32_t *fb_ids = malloc(res.count_fbs * sizeof(uint32_t));
        uint32_t *enc_ids = malloc(res.count_encoders * sizeof(uint32_t));
        
        if (!conn_ids || !crtc_ids || !fb_ids || !enc_ids) {
            DEBUG_LOG("[ERROR] OOM : impossible d'allouer les tableaux DRM.");
            if (conn_ids) free(conn_ids);
            if (crtc_ids) free(crtc_ids);
            if (fb_ids) free(fb_ids);
            if (enc_ids) free(enc_ids);
            close(journal.drm_fd);
            journal.drm_fd = -1;
            continue;
        }
        
        res.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;
        res.crtc_id_ptr = (uint64_t)(uintptr_t)crtc_ids;
        res.fb_id_ptr = (uint64_t)(uintptr_t)fb_ids;
        res.encoder_id_ptr = (uint64_t)(uintptr_t)enc_ids;
        
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
            DEBUG_LOG("[DEBUG] Échec GETRESOURCES passe 2.");
            free(conn_ids); free(crtc_ids); free(fb_ids); free(enc_ids);
            close(journal.drm_fd);
            journal.drm_fd = -1;
            continue;
        }
        
        /* 2. Trouver un connecteur actif (Écran allumé) */
        int found_connector = 0;
        for (uint32_t i = 0; i < res.count_connectors && !found_connector; i++) {
            struct drm_mode_get_connector conn = {0};
            conn.connector_id = conn_ids[i];
            if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
                DEBUG_LOG("[DEBUG] Échec GETCONNECTOR.");
                continue;
            }
            
            DEBUG_LOG("[DEBUG] Connecteur état récupéré.");
            
            // 1 == connecté
            if (conn.connection == 1 && conn.count_modes > 0) {
                struct drm_mode_modeinfo *modes = malloc(conn.count_modes * sizeof(*modes));
                if (!modes) continue;
                
                // On prépare un DEUXIÈME appel propre (en ignorant props/encoders)
                struct drm_mode_get_connector conn2 = {0};
                conn2.connector_id = conn.connector_id;
                conn2.count_modes = conn.count_modes;
                conn2.modes_ptr = (uint64_t)(uintptr_t)modes;
                
                if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn2) == 0) {
                    drm.conn_id = conn2.connector_id;
                    drm.mode = modes[0]; // La première résolution
                    DEBUG_LOG("[DEBUG] Connecteur sélectionné.");
                    
                    // Trouver l'encodeur et le CRTC associés
                    if (conn2.encoder_id) {
                        struct drm_mode_get_encoder enc = {0};
                        enc.encoder_id = conn2.encoder_id;
                        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETENCODER, &enc) == 0 && enc.crtc_id) {
                            drm.crtc_id = enc.crtc_id;
                            DEBUG_LOG("[DEBUG] CRTC trouvé via encodeur.");
                            found_connector = 1;
                        }
                    }
                    
                    // Fallback
                    if (!found_connector && res.count_crtcs > 0) {
                         drm.crtc_id = crtc_ids[0];
                         DEBUG_LOG("[DEBUG] CRTC trouvé en fallback.");
                         found_connector = 1;
                    }
                } else {
                    DEBUG_LOG("[DEBUG] Échec GETCONNECTOR passe 2.");
                }
                free(modes);
            }
        }
        
        free(conn_ids);
        free(crtc_ids);
        free(fb_ids);
        free(enc_ids);

        if (found_connector) {
            found_card = 1;
            /* Tenter de "voler" le Master à X11/Wayland */
            if (ioctl(journal.drm_fd, DRM_IOCTL_SET_MASTER, 0) < 0) {
                DEBUG_LOG("[DEBUG] Avertissement : Impossible d'obtenir le DRM_MASTER.");
            } else {
                DEBUG_LOG("[DEBUG] DRM_MASTER obtenu avec succès.");
            }
        } else {
            close(journal.drm_fd);
            journal.drm_fd = -1;
        }
    }

    if (!found_card) {
#ifdef ENABLE_DEBUG
        write(2, "Erreur: Aucun écran actif trouvé sur les cartes /dev/dri/cardX.\n", 65);
#endif
        restore_vt();
        return -1;
    }
    
    drm.width = drm.mode.hdisplay;
    drm.height = drm.mode.vdisplay;
    
    /* 3. Allouer les "Dumb Buffers" pour le Double Buffering (avec fallback en Single Buffer si echec) */
    drm.double_buffered = 1;
    for (int i = 0; i < 2; i++) {
        struct drm_mode_create_dumb create_dumb = {0};
        create_dumb.width = drm.width;
        create_dumb.height = drm.height;
        create_dumb.bpp = 32; // 32 bits par pixel (XRGB)
        
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0) {
            DEBUG_LOG("[ERROR] DRM_IOCTL_MODE_CREATE_DUMB");
            if (i == 1) {
                DEBUG_LOG("[WARNING] Fallback to single buffering (creation of second buffer failed)");
                drm.double_buffered = 0;
                break;
            }
            cleanup_drm();
            return -1;
        }
        
        drm.pitch = create_dumb.pitch;
        drm.size = create_dumb.size;
        drm.handle[i] = create_dumb.handle;
        
        /* 4. Créer un Framebuffer à partir du Dumb Buffer */
        struct drm_mode_fb_cmd cmd = {0};
        cmd.width = drm.width;
        cmd.height = drm.height;
        cmd.pitch = drm.pitch;
        cmd.bpp = 32;
        cmd.depth = 24;
        cmd.handle = drm.handle[i];
        
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_ADDFB, &cmd) < 0) {
            DEBUG_LOG("[ERROR] DRM_IOCTL_MODE_ADDFB");
            if (i == 1) {
                DEBUG_LOG("[WARNING] Fallback to single buffering (ADDFB of second buffer failed)");
                struct drm_mode_destroy_dumb dd = { .handle = drm.handle[1] };
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
                drm.handle[1] = 0;
                drm.double_buffered = 0;
                break;
            }
            cleanup_drm();
            return -1;
        }
        drm.fb_id[i] = cmd.fb_id;
        
        /* 5. Mapper le buffer en espace utilisateur via mmap() */
        struct drm_mode_map_dumb map_dumb = {0};
        map_dumb.handle = drm.handle[i];
        
        if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) < 0) {
            DEBUG_LOG("[ERROR] DRM_IOCTL_MODE_MAP_DUMB");
            if (i == 1) {
                DEBUG_LOG("[WARNING] Fallback to single buffering (MAP_DUMB of second buffer failed)");
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_RMFB, &drm.fb_id[1]);
                drm.fb_id[1] = 0;
                struct drm_mode_destroy_dumb dd = { .handle = drm.handle[1] };
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
                drm.handle[1] = 0;
                drm.double_buffered = 0;
                break;
            }
            cleanup_drm();
            return -1;
        }
        
        drm.buffer[i] = mmap(0, drm.size, PROT_READ | PROT_WRITE, MAP_SHARED, journal.drm_fd, map_dumb.offset);
        if (drm.buffer[i] == MAP_FAILED) {
            DEBUG_LOG("[ERROR] mmap");
            if (i == 1) {
                DEBUG_LOG("[WARNING] Fallback to single buffering (mmap of second buffer failed)");
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_RMFB, &drm.fb_id[1]);
                drm.fb_id[1] = 0;
                struct drm_mode_destroy_dumb dd = { .handle = drm.handle[1] };
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
                drm.handle[1] = 0;
                drm.double_buffered = 0;
                break;
            }
            cleanup_drm();
            return -1;
        }
    }
    
    /* Sauvegarder l'état actuel du CRTC pour le restaurer à la sortie */
    drm.saved_crtc.crtc_id = drm.crtc_id;
    drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_GETCRTC, &drm.saved_crtc);
    
    /* Configurer le CRTC avec notre framebuffer (une seule fois à l'initialisation) */
    struct drm_mode_crtc crtc = {0};
    crtc.crtc_id = drm.crtc_id;
    crtc.fb_id = drm.fb_id[0];
    crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&drm.conn_id;
    crtc.count_connectors = 1;
    crtc.mode = drm.mode;
    crtc.mode_valid = 1;
    if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
        DEBUG_LOG("[ERROR] DRM_IOCTL_MODE_SETCRTC");
        cleanup_drm();
        return -1;
    }
    
    return 0;
}

void cleanup_drm() {
    if (journal.drm_fd < 0) return;
    
    /* Restaurer l'affichage d'origine si valide */
    if (drm.saved_crtc.crtc_id) {
        drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &drm.saved_crtc);
    }
    
    /* Relâcher le statut de maître */
    ioctl(journal.drm_fd, DRM_IOCTL_DROP_MASTER, 0);
    
    /* Nettoyage mémoire et ressources GPU */
    for (int i = 0; i < 2; i++) {
        if (drm.buffer[i] && drm.buffer[i] != MAP_FAILED) {
            munmap(drm.buffer[i], drm.size);
        }
        drm.buffer[i] = NULL;
        
        if (drm.fb_id[i]) {
            drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_RMFB, &drm.fb_id[i]);
            drm.fb_id[i] = 0;
        }
        if (drm.handle[i]) {
            struct drm_mode_destroy_dumb dd = { .handle = drm.handle[i] };
            drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
            drm.handle[i] = 0;
        }
    }
    close(journal.drm_fd);
    journal.drm_fd = -1;
    
    /* Rendre le VT à X11 */
    restore_vt();
}

int wait_vblank() {
    union drm_wait_vblank vbl;
    memset(&vbl, 0, sizeof(vbl));
    vbl.request.type = 1; // DRM_VBLANK_RELATIVE
    vbl.request.sequence = 1;
    return drm_ioctl(journal.drm_fd, DRM_IOCTL_WAIT_VBLANK, &vbl);
}

#include <linux/input.h>

int find_keyboard() {
    int dir_fd = open("/dev/input", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd < 0) return -1;
    
    char buf[4096];
    int nread;
    int found_fd = -1;
    
    while ((nread = syscall(SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            if (d->d_reclen == 0) break;
            
            /* On cherche les fichiers qui commencent par "event" */
            if (d->d_name[0] == 'e' && d->d_name[1] == 'v' && d->d_name[2] == 'e' && d->d_name[3] == 'n' && d->d_name[4] == 't') {
                char path[256];
                safe_copy(path, "/dev/input/", sizeof(path));
                safe_concat(path, d->d_name, sizeof(path));
                
                int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd >= 0) {
                    unsigned long evbit[4] = {0};
                    unsigned long keybit[16] = {0};
                    
                    if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) >= 0) {
                        if (evbit[0] & (1 << EV_KEY)) {
                            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) {
                                if ((keybit[KEY_ENTER/64] & (1ULL << (KEY_ENTER%64))) &&
                                    (keybit[KEY_ESC/64] & (1ULL << (KEY_ESC%64)))) {
                                    found_fd = fd;
                                    break;
                                }
                            }
                        }
                    }
                    close(fd);
                }
            }
            bpos += d->d_reclen;
        }
        if (found_fd >= 0) break;
    }
    close(dir_fd);
    return found_fd;
}


/* Évasion des cgroups (Edge Case 8)
 * Si le système gèle à cause des limites cgroup de X11/Wayland ou du navigateur,
 * on se déplace dans le cgroup racine pour éviter d'être étranglé ou tué par OOM/Freezer.
 */
static void escape_cgroups() {
    pid_t pid = getpid();
    char pid_str[32];
    int_to_str((int)pid, pid_str);
    safe_concat(pid_str, "\n", sizeof(pid_str));
    int len = strlen(pid_str);

    const char *cgroup_paths[] = {
        "/sys/fs/cgroup/cgroup.procs",                 // cgroup v2 (root)
        "/sys/fs/cgroup/system.slice/cgroup.procs",    // cgroup v2 system slice
        "/sys/fs/cgroup/memory/cgroup.procs",          // cgroup v1 memory root
        "/sys/fs/cgroup/cpu/cgroup.procs",             // cgroup v1 cpu root
        NULL
    };

    for (int i = 0; cgroup_paths[i] != NULL; i++) {
        int fd = open(cgroup_paths[i], O_WRONLY);
        if (fd >= 0) {
            if (write(fd, pid_str, len) > 0) {
                DEBUG_LOG("[DEBUG] Évasion réussie vers le cgroup racine.");
            }
            close(fd);
        }
    }
}

/* ---- TASK MANAGER (Zero Allocation) ---- */
#define MAX_TOP_PROCS 15

struct process_info {
    int pid;
    char name[32];
    int rss_pages;
    int cpu_percent;
    char state;
    uid_t uid;
    char username[32];
};

#define HIST_SIZE 16384
struct pid_hist {
    int pid;
    unsigned long long cpu_time;
};
static struct pid_hist cpu_hist_A[HIST_SIZE];
static struct pid_hist cpu_hist_B[HIST_SIZE];
static struct pid_hist *current_hist = cpu_hist_A;
static struct pid_hist *prev_hist = cpu_hist_B;
static unsigned long long prev_scan_ms = 0;

static struct process_info top_procs_mem[MAX_TOP_PROCS];
static struct process_info top_procs_cpu[MAX_TOP_PROCS];
static struct process_info top_procs_search[MAX_TOP_PROCS];
static int num_top_procs_mem = 0;
static int num_top_procs_cpu = 0;
static int num_top_procs_search = 0;
static int sort_mode_cpu = 0; // 0 = MEMORY, 1 = CPU, 2 = SEARCH
static char search_buffer[64] = {0};
static int search_len = 0;

static int tm_focus_group = 2; // 0 = Tabs, 1 = Search, 2 = List, 3 = Close

static int tm_selected_tab = 0; // 0 = RAM, 1 = CPU, 2 = Search
static int tm_selected_row = 0; // Index dans la liste
static int tm_selected_col = 0; // 0 = Nom, 1 = Pause, 2 = Terminate, 3 = Kill

static int is_process_blacklisted(const char *name) {
    if (strstr(name, "systemd") != NULL) return 1;
    if (strstr(name, "kthread") != NULL) return 1;
    if (strstr(name, "sd-pam") != NULL) return 1;
    if (strstr(name, "dbus") != NULL) return 1;
    if (strstr(name, "Xorg") != NULL) return 1;
    if (strstr(name, "Xwayland") != NULL) return 1;
    if (strstr(name, "kworker") != NULL) return 1;
    if (strstr(name, "pulseaudio") != NULL) return 1;
    if (strstr(name, "pipewire") != NULL) return 1;
    return 0;
}

static unsigned long long total_ram_mb = 0;
static unsigned long long used_ram_mb = 0;
static unsigned long long total_swap_mb = 0;
static unsigned long long used_swap_mb = 0;
static int global_cpu_percent = 0;
static unsigned long long prev_cpu_total = 0;
static unsigned long long prev_cpu_idle = 0;
static int logical_cores = 0;

static void scan_sysinfo() {
    if (logical_cores == 0) {
        logical_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (logical_cores <= 0) logical_cores = 1;
    }

    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd >= 0) {
        char buf[2048];
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            unsigned long long m_total = 0, m_free = 0, m_buf = 0, m_cache = 0, s_total = 0, s_free = 0;
            char *p = buf;
            while (*p) {
                if (strncmp(p, "MemTotal:", 9) == 0) m_total = str_to_ull(p + 9);
                else if (strncmp(p, "MemFree:", 8) == 0) m_free = str_to_ull(p + 8);
                else if (strncmp(p, "Buffers:", 8) == 0) m_buf = str_to_ull(p + 8);
                else if (strncmp(p, "Cached:", 7) == 0) m_cache = str_to_ull(p + 7);
                else if (strncmp(p, "SwapTotal:", 10) == 0) s_total = str_to_ull(p + 10);
                else if (strncmp(p, "SwapFree:", 9) == 0) s_free = str_to_ull(p + 9);
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
            }
            total_ram_mb = m_total / 1024;
            unsigned long long mem_avail = m_free + m_buf + m_cache;
            used_ram_mb = (m_total > mem_avail) ? (m_total - mem_avail) / 1024 : 0;
            total_swap_mb = s_total / 1024;
            used_swap_mb = (s_total > s_free) ? (s_total - s_free) / 1024 : 0;
        }
    }

    fd = open("/proc/stat", O_RDONLY);
    if (fd >= 0) {
        char buf[512];
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            if (strncmp(buf, "cpu ", 4) == 0) {
                char *p = buf + 4;
                unsigned long long user = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long nice = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long system = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long idle = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long iowait = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long irq = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long softirq = str_to_ull(p); while (*p == ' ') p++; while (*p >= '0' && *p <= '9') p++;
                unsigned long long steal = str_to_ull(p);
                
                unsigned long long current_idle = idle + iowait;
                unsigned long long current_non_idle = user + nice + system + irq + softirq + steal;
                unsigned long long current_total = current_idle + current_non_idle;
                
                if (current_total > prev_cpu_total) {
                    unsigned long long totald = current_total - prev_cpu_total;
                    unsigned long long idled = current_idle - prev_cpu_idle;
                    global_cpu_percent = ((totald - idled) * 100) / totald;
                }
                prev_cpu_total = current_total;
                prev_cpu_idle = current_idle;
            }
        }
    }
}

static void scan_processes() {
    scan_sysinfo();
    int dir_fd = open("/proc", O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return;

    struct process_info current_scan_mem[MAX_TOP_PROCS];
    struct process_info current_scan_cpu[MAX_TOP_PROCS];
    struct process_info current_scan_search[MAX_TOP_PROCS];
    int current_num_mem = 0;
    int current_num_cpu = 0;
    int current_num_search = 0;
    
    for (int i = 0; i < MAX_TOP_PROCS; i++) {
        current_scan_mem[i].rss_pages = -1;
        current_scan_cpu[i].cpu_percent = -1;
        current_scan_search[i].rss_pages = -1;
    }

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    unsigned long long current_ms = ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
    unsigned long long delta_ms = current_ms - prev_scan_ms;
    if (delta_ms == 0) delta_ms = 1;

    // Swap buffers
    struct pid_hist *temp = current_hist;
    current_hist = prev_hist;
    prev_hist = temp;

    // Clear current_hist
    for (int i = 0; i < HIST_SIZE; i++) current_hist[i].pid = 0;

    int my_pid = getpid();
    char path[256];
    char buf[1024];
    char dir_buf[16384]; // Augmenté pour éviter de faire trop d'appels getdents64 sur des systèmes lourdement chargés
    int nread;

    while ((nread = syscall(SYS_getdents64, dir_fd, dir_buf, sizeof(dir_buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (dir_buf + bpos);
            if (d->d_reclen == 0) break;
            
            if (d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                int pid = str_to_int(d->d_name);
                if (pid != my_pid && pid != 0) {
                    safe_copy(path, "/proc/", sizeof(path)); safe_concat(path, d->d_name, sizeof(path)); safe_concat(path, "/stat", sizeof(path));
                    int fd = open(path, O_RDONLY);
                    if (fd >= 0) {
                        int n = read(fd, buf, sizeof(buf) - 1);
                        close(fd);
                        if (n > 0) {
                            buf[n] = '\0';
                            struct process_info p = { .pid = pid, .name = {0}, .rss_pages = 0, .cpu_percent = 0, .state = '?', .uid = 0, .username = {0} };
                            
                            char proc_pid_path[256];
                            safe_copy(proc_pid_path, "/proc/", sizeof(proc_pid_path));
                            safe_concat(proc_pid_path, d->d_name, sizeof(proc_pid_path));
                            struct stat st_proc;
                            if (stat(proc_pid_path, &st_proc) == 0) {
                                p.uid = st_proc.st_uid;
                                get_username_by_uid(st_proc.st_uid, p.username, sizeof(p.username));
                            } else {
                                p.uid = 0;
                                safe_copy(p.username, "root", sizeof(p.username));
                            }
                            
                            char *ptr = buf;
                            while (*ptr && *ptr != '(') ptr++;
                            if (*ptr == '(') {
                                ptr++;
                                char *last_paren = NULL;
                                for (char *s = ptr; *s; s++) {
                                    if (*s == ')') last_paren = s;
                                }
                                
                                if (last_paren) {
                                    int i = 0;
                                    while (ptr < last_paren && i < 31) {
                                        p.name[i++] = *ptr++;
                                    }
                                    p.name[i] = '\0';
                                    ptr = last_paren + 1;
                                }
                            }
                            while (*ptr == ' ') ptr++;
                            
                            if (*ptr) p.state = *ptr;
                            
                            // Parse CPU time (utime = field 14, stime = field 15)
                            unsigned long long current_cpu_time = 0;
                            int spaces_cpu = 0;
                            char *ptr_cpu = ptr;
                            while (*ptr_cpu && spaces_cpu < 11) {
                                if (*ptr_cpu == ' ') spaces_cpu++;
                                ptr_cpu++;
                            }
                            if (*ptr_cpu) {
                                current_cpu_time = str_to_ull(ptr_cpu);
                                while (*ptr_cpu && *ptr_cpu != ' ') ptr_cpu++; // skip utime
                                if (*ptr_cpu == ' ') {
                                    ptr_cpu++;
                                    current_cpu_time += str_to_ull(ptr_cpu); // add stime
                                }
                            }
                            
                            // Find in prev_hist
                            unsigned long long prev_cpu_time = 0;
                            int slot = pid % HIST_SIZE;
                            int start_slot = slot;
                            while (prev_hist[slot].pid != 0) {
                                if (prev_hist[slot].pid == pid) {
                                    prev_cpu_time = prev_hist[slot].cpu_time;
                                    break;
                                }
                                slot = (slot + 1) % HIST_SIZE;
                                if (slot == start_slot) break;
                            }
                            
                            unsigned long long delta_ticks = 0;
                            if (prev_scan_ms != 0 && current_cpu_time >= prev_cpu_time) {
                                delta_ticks = current_cpu_time - prev_cpu_time;
                            }
                            
                            p.cpu_percent = (int)((delta_ticks * 1000ULL) / delta_ms);
                            if (logical_cores > 0) {
                                p.cpu_percent /= logical_cores;
                            }

                            // Insert into current_hist
                            slot = pid % HIST_SIZE;
                            start_slot = slot;
                            while (current_hist[slot].pid != 0) {
                                slot = (slot + 1) % HIST_SIZE;
                                if (slot == start_slot) break; // Table pleine
                            }
                            if (current_hist[slot].pid == 0) {
                                current_hist[slot].pid = pid;
                                current_hist[slot].cpu_time = current_cpu_time;
                            }
                            // Note: Si la table est pleine (> 16384 processus), on abandonne silencieusement.
                            // Le CPU affiché sera de 0% pour les processus supplémentaires.
                            
                            // Parse RSS (field 24)
                            int spaces_mem = 0;
                            while (*ptr && spaces_mem < 21) {
                                if (*ptr == ' ') spaces_mem++;
                                ptr++;
                            }
                            if (*ptr) {
                                p.rss_pages = str_to_int(ptr);
                            }

                            if (!is_process_blacklisted(p.name)) {
                                // Insertion tri par Mémoire
                                if (p.rss_pages > current_scan_mem[MAX_TOP_PROCS - 1].rss_pages) {
                                    int insert_idx = MAX_TOP_PROCS - 1;
                                    while (insert_idx > 0 && p.rss_pages > current_scan_mem[insert_idx - 1].rss_pages) {
                                        insert_idx--;
                                    }
                                    for (int i = MAX_TOP_PROCS - 1; i > insert_idx; i--) {
                                        current_scan_mem[i] = current_scan_mem[i - 1];
                                    }
                                    current_scan_mem[insert_idx] = p;
                                    if (current_num_mem < MAX_TOP_PROCS) current_num_mem++;
                                }
                                
                                // Insertion tri par CPU
                                if (p.cpu_percent > current_scan_cpu[MAX_TOP_PROCS - 1].cpu_percent) {
                                    int insert_idx = MAX_TOP_PROCS - 1;
                                    while (insert_idx > 0 && p.cpu_percent > current_scan_cpu[insert_idx - 1].cpu_percent) {
                                        insert_idx--;
                                    }
                                    for (int i = MAX_TOP_PROCS - 1; i > insert_idx; i--) {
                                        current_scan_cpu[i] = current_scan_cpu[i - 1];
                                    }
                                    current_scan_cpu[insert_idx] = p;
                                    if (current_num_cpu < MAX_TOP_PROCS) current_num_cpu++;
                                }
                                
                                // Insertion recherche (Tri par TOP RAM)
                                if (sort_mode_cpu == 2 && search_len >= 3) {
                                    if (strcasestr(p.name, search_buffer) != NULL) {
                                        if (p.rss_pages > current_scan_search[MAX_TOP_PROCS - 1].rss_pages) {
                                            int insert_idx = MAX_TOP_PROCS - 1;
                                            while (insert_idx > 0 && p.rss_pages > current_scan_search[insert_idx - 1].rss_pages) {
                                                insert_idx--;
                                            }
                                            for (int i = MAX_TOP_PROCS - 1; i > insert_idx; i--) {
                                                current_scan_search[i] = current_scan_search[i - 1];
                                            }
                                            current_scan_search[insert_idx] = p;
                                            if (current_num_search < MAX_TOP_PROCS) current_num_search++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            bpos += d->d_reclen;
        }
    }
    close(dir_fd);

    for (int i = 0; i < MAX_TOP_PROCS; i++) {
        top_procs_mem[i] = current_scan_mem[i];
        top_procs_cpu[i] = current_scan_cpu[i];
        top_procs_search[i] = current_scan_search[i];
    }
    num_top_procs_mem = current_num_mem;
    num_top_procs_cpu = current_num_cpu;
    num_top_procs_search = current_num_search;
    prev_scan_ms = current_ms;
}

#define UID_CACHE_MAX 128
struct uid_cache_entry {
    int uid;
    char username[32];
};
static struct uid_cache_entry uid_cache[UID_CACHE_MAX];
static int uid_cache_count = 0;

static void get_username_by_uid(int uid, char *username_out, size_t max_len) {
    username_out[0] = '\0';
    
    for (int i = 0; i < uid_cache_count; i++) {
        if (uid_cache[i].uid == uid) {
            safe_copy(username_out, uid_cache[i].username, max_len);
            return;
        }
    }
    
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd >= 0) {
        char buf[8192];
        int bytes = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (bytes > 0) {
            buf[bytes] = '\0';
            char *line = buf;
            while (line && *line) {
                char *next = strchr(line, '\n');
                if (next) *next++ = '\0';
                
                char *p = line;
                char *user = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                char *uid_str = p; p = strchr(p, ':'); if (!p) { line = next; continue; } *p++ = '\0';
                
                int line_uid = str_to_int(uid_str);
                
                if (uid_cache_count < UID_CACHE_MAX) {
                    int found = 0;
                    for (int j = 0; j < uid_cache_count; j++) {
                        if (uid_cache[j].uid == line_uid) { found = 1; break; }
                    }
                    if (!found) {
                        uid_cache[uid_cache_count].uid = line_uid;
                        safe_copy(uid_cache[uid_cache_count].username, user, sizeof(uid_cache[uid_cache_count].username));
                        uid_cache_count++;
                    }
                }
                
                if (line_uid == uid) {
                    safe_copy(username_out, user, max_len);
                }
                line = next;
            }
        }
    }
    
    if (username_out[0] == '\0') {
        int_to_str(uid, username_out);
        if (uid_cache_count < UID_CACHE_MAX) {
            uid_cache[uid_cache_count].uid = uid;
            safe_copy(uid_cache[uid_cache_count].username, username_out, sizeof(uid_cache[uid_cache_count].username));
            uid_cache_count++;
        }
    }
}

static int get_vt_from_fds(int pid) {
    char fd_dir_path[256];
    safe_copy(fd_dir_path, "/proc/", sizeof(fd_dir_path));
    char pid_str[32];
    int_to_str(pid, pid_str);
    safe_concat(fd_dir_path, pid_str, sizeof(fd_dir_path));
    safe_concat(fd_dir_path, "/fd", sizeof(fd_dir_path));
    
    int dir_fd = open(fd_dir_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return -1;
    
    char buf[2048];
    int nread;
    int found_vt = -1;
    while ((nread = syscall(SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            if (d->d_reclen == 0) break;
            if (d->d_name[0] != '.') {
                char link_path[512];
                safe_copy(link_path, fd_dir_path, sizeof(link_path));
                safe_concat(link_path, "/", sizeof(link_path));
                safe_concat(link_path, d->d_name, sizeof(link_path));
                
                char target[512];
                ssize_t len = readlink(link_path, target, sizeof(target) - 1);
                if (len > 0) {
                    target[len] = '\0';
                    if (strncmp(target, "/dev/tty", 8) == 0 && target[8] >= '0' && target[8] <= '9') {
                        found_vt = str_to_int(target + 8);
                        break;
                    }
                }
            }
            bpos += d->d_reclen;
        }
        if (found_vt >= 0) break;
    }
    close(dir_fd);
    return found_vt;
}

static int get_vt_from_environ(int pid) {
    char env_path[256];
    safe_copy(env_path, "/proc/", sizeof(env_path));
    char pid_str[32];
    int_to_str(pid, pid_str);
    safe_concat(env_path, pid_str, sizeof(env_path));
    safe_concat(env_path, "/environ", sizeof(env_path));
    
    int fd = open(env_path, O_RDONLY);
    if (fd < 0) return -1;
    
    char buf[4096];
    int bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (bytes <= 0) return -1;
    buf[bytes] = '\0';
    
    char *ptr = buf;
    char *end = buf + bytes;
    while (ptr < end && *ptr) {
        if (strncmp(ptr, "XDG_VTNR=", 9) == 0) {
            int vt = str_to_int(ptr + 9);
            if (vt >= 1 && vt <= 63) {
                return vt;
            }
        }
        while (ptr < end && *ptr) ptr++;
        ptr++;
    }
    return -1;
}

static void scan_sessions() {
    num_sessions = 0;
    int dir_fd = open("/proc", O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0) return;
    
    char dir_buf[16384];
    int nread;
    
    while ((nread = syscall(SYS_getdents64, dir_fd, dir_buf, sizeof(dir_buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (dir_buf + bpos);
            if (d->d_reclen == 0) break;
            
            if (d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                int pid = str_to_int(d->d_name);
                if (pid > 0 && pid != getpid()) {
                    char proc_path[256];
                    safe_copy(proc_path, "/proc/", sizeof(proc_path));
                    safe_concat(proc_path, d->d_name, sizeof(proc_path));
                    
                    struct stat st;
                    if (stat(proc_path, &st) == 0) {
                        char stat_path[256];
                        safe_copy(stat_path, proc_path, sizeof(stat_path));
                        safe_concat(stat_path, "/stat", sizeof(stat_path));
                        
                        int fd = open(stat_path, O_RDONLY);
                        if (fd >= 0) {
                            char buf[1024];
                            int n = read(fd, buf, sizeof(buf) - 1);
                            close(fd);
                            if (n > 0) {
                                buf[n] = '\0';
                                char name[64] = {0};
                                char *ptr = buf;
                                while (*ptr && *ptr != '(') ptr++;
                                if (*ptr == '(') {
                                    ptr++;
                                    char *last_paren = NULL;
                                    for (char *s = ptr; *s; s++) {
                                        if (*s == ')') last_paren = s;
                                    }
                                    if (last_paren) {
                                        int i = 0;
                                        while (ptr < last_paren && i < 63) {
                                            name[i++] = *ptr++;
                                        }
                                        name[i] = '\0';
                                        ptr = last_paren + 1;
                                    }
                                }
                                while (*ptr == ' ') ptr++;
                                
                                if (*ptr) {
                                    while (*ptr && *ptr != ' ') ptr++; // skip state
                                    while (*ptr == ' ') ptr++;
                                    while (*ptr && *ptr != ' ') ptr++; // skip ppid
                                    while (*ptr == ' ') ptr++;
                                    while (*ptr && *ptr != ' ') ptr++; // skip pgrp
                                    while (*ptr == ' ') ptr++;
                                    while (*ptr && *ptr != ' ') ptr++; // skip session
                                    while (*ptr == ' ') ptr++;
                                    
                                    int tty_nr = str_to_int(ptr);
                                    int vt = -1;
                                    if (tty_nr >= 1025 && tty_nr <= 1087) {
                                        vt = tty_nr - 1024;
                                    }
                                    
                                    int priority = 0;
                                    int is_graphical = 0;
                                    int is_dm = 0;
                                    
                                    if (strcmp(name, "Xorg") == 0 || strcmp(name, "X") == 0 ||
                                        strcmp(name, "gnome-shell") == 0 || strcmp(name, "kwin_wayland") == 0 ||
                                        strcmp(name, "sway") == 0 || strcmp(name, "hyprland") == 0 ||
                                        strcmp(name, "weston") == 0 || strcmp(name, "wayfire") == 0 ||
                                        strcmp(name, "labwc") == 0 || strcmp(name, "kwin") == 0) {
                                        priority = 4;
                                        is_graphical = 1;
                                    } else if (strcmp(name, "lightdm") == 0 || strcmp(name, "gdm") == 0 ||
                                               strcmp(name, "gdm-simple-gree") == 0 || strcmp(name, "sddm") == 0 ||
                                               strcmp(name, "sddm-greeter") == 0 || strcmp(name, "lightdm-gtk-gre") == 0 ||
                                               strcmp(name, "tdm") == 0) {
                                        priority = 3;
                                        is_dm = 1;
                                        is_graphical = 1;
                                    } else if (strcmp(name, "kicker") == 0 || strcmp(name, "plasma-shell") == 0 ||
                                               strcmp(name, "gnome-panel") == 0 || strcmp(name, "xfce4-panel") == 0 ||
                                               strcmp(name, "mate-panel") == 0 || strcmp(name, "tdeinit") == 0 ||
                                               strcmp(name, "kdesktop") == 0) {
                                        priority = 2;
                                        is_graphical = 1;
                                    } else if (strcmp(name, "bash") == 0 || strcmp(name, "zsh") == 0 ||
                                               strcmp(name, "sh") == 0 || strcmp(name, "dash") == 0 ||
                                               strcmp(name, "fish") == 0 || strcmp(name, "ksh") == 0 ||
                                               strcmp(name, "tcsh") == 0 || strcmp(name, "csh") == 0 ||
                                               strcmp(name, "login") == 0) {
                                        priority = 1;
                                    }
                                    
                                    if (vt == -1 && priority >= 1) {
                                        vt = get_vt_from_environ(pid);
                                        if (vt == -1 && priority >= 2) {
                                            vt = get_vt_from_fds(pid);
                                        }
                                    }
                                    
                                    if (priority >= 1 && vt >= 1 && vt <= 63 && vt != journal.rescue_vt) {
                                        char username[32];
                                        get_username_by_uid(st.st_uid, username, sizeof(username));
                                        
                                        int idx = -1;
                                        for (int j = 0; j < num_sessions; j++) {
                                            if (sessions[j].vt == vt) {
                                                idx = j;
                                                break;
                                            }
                                        }
                                        
                                        if (idx < 0) {
                                            if (num_sessions < MAX_SESSIONS) {
                                                sessions[num_sessions].vt = vt;
                                                sessions[num_sessions].uid = st.st_uid;
                                                safe_copy(sessions[num_sessions].username, username, sizeof(sessions[num_sessions].username));
                                                safe_copy(sessions[num_sessions].proc_name, name, sizeof(sessions[num_sessions].proc_name));
                                                sessions[num_sessions].priority = priority;
                                                sessions[num_sessions].is_graphical = is_graphical;
                                                sessions[num_sessions].is_dm = is_dm;
                                                num_sessions++;
                                            }
                                        } else {
                                            // Si on trouve un UID non-root, on met à jour.
                                            // On privilégie un UID utilisateur (>= 1000) face à un UID système (< 1000),
                                            // et un UID système face à root (UID == 0).
                                            if ((st.st_uid >= 1000 && sessions[idx].uid < 1000) || (st.st_uid != 0 && sessions[idx].uid == 0)) {
                                                sessions[idx].uid = st.st_uid;
                                                safe_copy(sessions[idx].username, username, sizeof(sessions[idx].username));
                                            }
                                            if (is_graphical) sessions[idx].is_graphical = 1;
                                            if (is_dm) sessions[idx].is_dm = 1;
                                            
                                            if (priority > sessions[idx].priority) {
                                                safe_copy(sessions[idx].proc_name, name, sizeof(sessions[idx].proc_name));
                                                sessions[idx].priority = priority;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            bpos += d->d_reclen;
        }
    }
    close(dir_fd);
    
    for (int i = 0; i < num_sessions - 1; i++) {
        for (int j = i + 1; j < num_sessions; j++) {
            if (sessions[i].vt > sessions[j].vt) {
                struct session_info_t tmp = sessions[i];
                sessions[i] = sessions[j];
                sessions[j] = tmp;
            }
        }
    }
}

int main() {
    /* 1. Isolation complète : Devenir process leader */
    escape_cgroups();

    /* Détermination de la langue */
    const char *lang = getenv("CAD_LANG");
    if (!lang) lang = getenv("LANG");
    if (lang && strlen(lang) >= 2) {
        char lang_short[3];
        lang_short[0] = lang[0];
        lang_short[1] = lang[1];
        lang_short[2] = '\0';
        if (lang_short[0] >= 'A' && lang_short[0] <= 'Z') lang_short[0] += 32;
        if (lang_short[1] >= 'A' && lang_short[1] <= 'Z') lang_short[1] += 32;

        int found = 0;
        for (int i = 0; available_translations[i] != NULL; i++) {
            if (strcmp(available_translations[i]->lang_code, lang_short) == 0) {
                current_trans = available_translations[i];
                found = 1;
                break;
            }
        }
        if (!found) {
            current_trans = &trans_en;
        }
    } else {
        current_trans = &trans_en;
    }



    /* Immunisation absolue contre l'OOM Killer (Score -1000 = intouchable) */
    int oom_fd = open("/proc/self/oom_score_adj", O_WRONLY);
    if (oom_fd >= 0) {
        write(oom_fd, "-1000", 5);
        close(oom_fd);
        DEBUG_LOG("[DEBUG] Immunité OOM Killer acquise (-1000).");
    } else {
        DEBUG_LOG("[DEBUG] Attention: Impossible de s'immuniser contre l'OOM Killer.");
    }

    /* Priorité I/O Best-Effort Maximale (IOPRIO_CLASS_BE, prio 0) pour bypasser la congestion disque sans affamer le noyau */
    if (syscall(SYS_ioprio_set, 1, 0, (2 << 13) | 0) == 0) {
        DEBUG_LOG("[DEBUG] Priorité I/O Best-Effort Maximale acquise.");
    } else {
        DEBUG_LOG("[DEBUG] Attention: Impossible d'acquérir la priorité I/O BE.");
    }


    /* Priorité CPU Temps Réel (SCHED_FIFO) */
    struct sched_param sp = { .sched_priority = 20 };
    if (sched_setscheduler(0, SCHED_RR, &sp) == 0) {
        DEBUG_LOG("[DEBUG] Priorité Temps Réel (SCHED_RR 20) acquise.");
    } else {
        DEBUG_LOG("[DEBUG] Échec SCHED_RR (pas de CAP_SYS_NICE ?), fallback setpriority(-20).");
        syscall(SYS_setpriority, 0, 0, -20); /* PRIO_PROCESS == 0 */
    }

    /* Désactivation des messages d'erreur du noyau (printk) sur la console pour éviter la corruption visuelle */
    klogctl(6, NULL, 0);

    /* Sécurité absolue : restauration du TTY (KD_TEXT) en cas de crash/kill */
    atexit(restore_vt);
    
    /* Allocation d'une pile dédiée pour le gestionnaire de signaux (cas de Stack Overflow) */
    static char emergency_stack[65536];
    stack_t ss;
    ss.ss_sp = emergency_stack;
    ss.ss_size = sizeof(emergency_stack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa = {0};
    sa.sa_handler = emergency_restore;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL); // Ignorer les sockets/pipes brisés silencieusement
    
    // Récolter automatiquement les processus enfants (éviter les zombies PTY)
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT;  // Le kernel récolte automatiquement les enfants
    sigaction(SIGCHLD, &sa, NULL);

    DEBUG_LOG("CAD Rescue GUI - Initialisation DRM/KMS pure ioctl...");
    
    /* Désactiver SysRq pour protéger la session d'urgence */
    disable_sysrq();
    
    /* Sauvetage de l'écran noir physique (Backlight < 5%) avant de prendre la main */
    force_backlight();
    
    if (init_drm() < 0) {
        return 1;
    }
    
    DEBUG_LOG("[DEBUG] Succès ! Écran pris en charge.");
    
    /* Configuration de Nuklear pour un Framebuffer 32-bits XRGB */
    struct rawfb_pl pl = {
        .bytesPerPixel = 4,
        .rshift = 16, .gshift = 8, .bshift = 0, .ashift = 24,
        .rloss = 0, .gloss = 0, .bloss = 0, .aloss = 0
    };
    
    /* Préparation des images pour NK_COMMAND_IMAGE */
    struct rawfb_image raw_img_find = {
        .pixels = (void*)find_data, .w = FIND_W, .h = FIND_H,
        .pitch = FIND_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_sleep = {
        .pixels = (void*)sleep_data, .w = SLEEP_W, .h = SLEEP_H,
        .pitch = SLEEP_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_power = {
        .pixels = (void*)power_data, .w = POWER_W, .h = POWER_H,
        .pitch = POWER_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_restart = {
        .pixels = (void*)restart_data, .w = RESTART_W, .h = RESTART_H,
        .pitch = RESTART_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_check1 = {
        .pixels = (void*)check1_data, .w = CHECK1_W, .h = CHECK1_H,
        .pitch = CHECK1_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_check2 = {
        .pixels = (void*)check2_data, .w = CHECK2_W, .h = CHECK2_H,
        .pitch = CHECK2_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_close = {
        .pixels = (void*)close_data, .w = CLOSE_W, .h = CLOSE_H,
        .pitch = CLOSE_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_runcommand = {
        .pixels = (void*)runcommand_data, .w = RUNCOMMAND_W, .h = RUNCOMMAND_H,
        .pitch = RUNCOMMAND_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_console = {
        .pixels = (void*)console_data, .w = CONSOLE_W, .h = CONSOLE_H,
        .pitch = CONSOLE_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_taskmgr = {
        .pixels = (void*)taskmgr_data, .w = TASKMGR_W, .h = TASKMGR_H,
        .pitch = TASKMGR_W * 4, .pl = pl
    };
    struct rawfb_image raw_img_password = {
        .pixels = (void*)password_data, .w = PASSWORD_W, .h = PASSWORD_H,
        .pitch = PASSWORD_W * 4, .pl = pl
    };

    struct nk_image nk_img_find = nk_image_ptr(&raw_img_find);
    nk_img_find.region[0] = 0; nk_img_find.region[1] = 0;
    nk_img_find.region[2] = FIND_W; nk_img_find.region[3] = FIND_H;

    struct nk_image nk_img_sleep = nk_image_ptr(&raw_img_sleep);
    nk_img_sleep.region[0] = 0; nk_img_sleep.region[1] = 0;
    nk_img_sleep.region[2] = SLEEP_W; nk_img_sleep.region[3] = SLEEP_H;

    struct nk_image nk_img_power = nk_image_ptr(&raw_img_power);
    nk_img_power.region[0] = 0; nk_img_power.region[1] = 0;
    nk_img_power.region[2] = POWER_W; nk_img_power.region[3] = POWER_H;

    struct nk_image nk_img_restart = nk_image_ptr(&raw_img_restart);
    nk_img_restart.region[0] = 0; nk_img_restart.region[1] = 0;
    nk_img_restart.region[2] = RESTART_W; nk_img_restart.region[3] = RESTART_H;

    struct nk_image nk_img_check1 = nk_image_ptr(&raw_img_check1);
    nk_img_check1.region[0] = 0; nk_img_check1.region[1] = 0;
    nk_img_check1.region[2] = CHECK1_W; nk_img_check1.region[3] = CHECK1_H;

    struct nk_image nk_img_check2 = nk_image_ptr(&raw_img_check2);
    nk_img_check2.region[0] = 0; nk_img_check2.region[1] = 0;
    nk_img_check2.region[2] = CHECK2_W; nk_img_check2.region[3] = CHECK2_H;

    struct nk_image nk_img_close = nk_image_ptr(&raw_img_close);
    nk_img_close.region[0] = 0; nk_img_close.region[1] = 0;
    nk_img_close.region[2] = CLOSE_W; nk_img_close.region[3] = CLOSE_H;

    struct nk_image nk_img_runcommand = nk_image_ptr(&raw_img_runcommand);
    nk_img_runcommand.region[0] = 0; nk_img_runcommand.region[1] = 0;
    nk_img_runcommand.region[2] = RUNCOMMAND_W; nk_img_runcommand.region[3] = RUNCOMMAND_H;

    struct nk_image nk_img_console = nk_image_ptr(&raw_img_console);
    nk_img_console.region[0] = 0; nk_img_console.region[1] = 0;
    nk_img_console.region[2] = CONSOLE_W; nk_img_console.region[3] = CONSOLE_H;

    struct nk_image nk_img_taskmgr = nk_image_ptr(&raw_img_taskmgr);
    nk_img_taskmgr.region[0] = 0; nk_img_taskmgr.region[1] = 0;
    nk_img_taskmgr.region[2] = TASKMGR_W; nk_img_taskmgr.region[3] = TASKMGR_H;

    struct nk_image nk_img_password = nk_image_ptr(&raw_img_password);
    nk_img_password.region[0] = 0; nk_img_password.region[1] = 0;
    nk_img_password.region[2] = PASSWORD_W; nk_img_password.region[3] = PASSWORD_H;

#if HAS_BRANDING
    struct rawfb_image raw_img_branding = {
        .pixels = (void*)branding_data, .w = BRANDING_W, .h = BRANDING_H,
        .pitch = BRANDING_W * 4, .pl = pl
    };
    struct nk_image nk_img_branding = nk_image_ptr(&raw_img_branding);
    nk_img_branding.region[0] = 0; nk_img_branding.region[1] = 0;
    nk_img_branding.region[2] = BRANDING_W; nk_img_branding.region[3] = BRANDING_H;
#endif

    
    /* Allocation statique (BSS) pour contourner les verrous (locks) de la glibc (malloc)
       et s'affranchir totalement de la corruption de Heap (Edge case 14/15) */
    static uint8_t nk_tex_mem[4 * 1024 * 1024];
    
    struct rawfb_context *rawfb = nk_rawfb_init(drm.buffer[0], nk_tex_mem, drm.width, drm.height, drm.pitch, pl);
    if (!rawfb) {
        DEBUG_LOG("[ERROR] Erreur d'initialisation Nuklear.");
        return 1;
    }
    struct nk_context *ctx = &rawfb->ctx;

    /* Calcul dynamique de la largeur des menus et boutons selon la langue et la police */
    {
        const struct nk_user_font *font = ctx->style.font;
        float w0 = font->width(font->userdata, font->height, current_trans->btn_sleep, strlen(current_trans->btn_sleep));
        float w1 = font->width(font->userdata, font->height, current_trans->btn_restart, strlen(current_trans->btn_restart));
        float w2 = font->width(font->userdata, font->height, current_trans->btn_shutdown, strlen(current_trans->btn_shutdown));
        float max_pm_w = w0;
        if (w1 > max_pm_w) max_pm_w = w1;
        if (w2 > max_pm_w) max_pm_w = w2;
        pm_menu_w = (int)max_pm_w + 100; // 52px offset icône + marge de sécurité
        if (pm_menu_w < 230) pm_menu_w = 230;

        const struct nk_user_font *mono_font = &rawfb->font_mono->handle;
        float w_root = mono_font->width(mono_font->userdata, mono_font->height, current_trans->btn_root_mode, strlen(current_trans->btn_root_mode));
        float w_user = mono_font->width(mono_font->userdata, mono_font->height, current_trans->btn_user_mode, strlen(current_trans->btn_user_mode));
        float max_btn_w = (w_root > w_user) ? w_root : w_user;
        root_mode_btn_w = (int)max_btn_w + 16; // 12px de padding (6px gauche + 6px droite) + 4px de marge de sécurité
        if (root_mode_btn_w < 110) root_mode_btn_w = 110;
    }

    journal.mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (journal.mouse_fd < 0) {
        DEBUG_LOG("[DEBUG] Attention: Impossible d'ouvrir /dev/input/mice.");
    } else {
        /* /dev/input/mice n'est pas un evdev pur, le grab peut échouer, on ignore l'erreur */
        ioctl(journal.mouse_fd, EVIOCGRAB, 1);
        
        /* Flush initial du buffer de la souris pour éviter les clics/mouvements fantômes résiduels */
        unsigned char discard[1024];
        while (read(journal.mouse_fd, discard, sizeof(discard)) > 0) {}
    }
    
    journal.kbd_fd = find_keyboard();
    if (journal.kbd_fd < 0) {
        DEBUG_LOG("[DEBUG] Attention: Aucun clavier trouvé.");
    } else {
        if (ioctl(journal.kbd_fd, EVIOCGRAB, 1) == 0) {
            DEBUG_LOG("[DEBUG] Clavier verrouillé en usage exclusif (EVIOCGRAB).");
            /* Flush complet des événements pour éviter les touches fantômes (ex: Alt/Ctrl coincé) */
            struct input_event ev;
            int flush_limit = 0;
            while (read(journal.kbd_fd, &ev, sizeof(ev)) > 0 && flush_limit++ < 128) {}
        } else {
            DEBUG_LOG("[DEBUG] Attention: Impossible de verrouiller le clavier en usage exclusif.");
        }
    }
    int mouse_x = drm.width / 2;
    int mouse_y = drm.height / 2;
    int prev_mouse_x = drm.width / 2;
    int prev_mouse_y = drm.height / 2;
    int mouse_btn_left = 0;
    int prev_mouse_btn_left = 0;
    static unsigned char mouse_acc[3];
    static int mouse_acc_len = 0;
    int keyboard_active = 1;
    int shift_pressed = 0;
    int ctrl_pressed = 0;

    init_static_info();

    /* Initialiser BusyBox en RAM d'office au démarrage pour servir de shell de secours
       ultra-résilient (utilisé par execute_as_user, reboot, shutdown, etc.) */
    init_busybox_memfd();

    /* Empêcher le swapout de notre mémoire (mlockall) 
       Effectué à la fin pour éviter que la limite RLIMIT_MEMLOCK ne bloque
       les mmaps de buffers DRM ou d'éventuels mallocs internes avant la boucle. */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {
        DEBUG_LOG("[DEBUG] Mémoire verrouillée en RAM (mlockall).");
    }

    // --- INIT TERMINAL (libtsm & PTY) ---
    /* Calcul déterministe de la taille du terminal pour éviter TOUT redimensionnement (et donc malloc) pendant la boucle de rendu */
    int term_w = (drm.width * 5) / 9;
    int term_h = (drm.height * 5) / 9;
    int main_h = term_h - 85;
    struct nk_user_font *font = &rawfb->font_mono->handle;
    float cell_w = font->width(font->userdata, font->height, "W", 1);
    float cell_h = font->height;
    
    /* Calcul pur sur les dimensions brutes. Nuklear clippera automatiquement s'il y a un demi-pixel de débordement,
       ce qui garantit que l'on remplit 100% de l'espace visuel disponible. */
    fixed_cols = (unsigned int)(term_w / cell_w);
    fixed_rows = (unsigned int)(main_h / cell_h);
    
    if (tsm_screen_new(&term_screen, NULL, NULL) == 0) {
        tsm_screen_resize(term_screen, fixed_cols, fixed_rows);
        if (tsm_vte_new(&term_vte, term_screen, terminal_vte_write_cb, NULL, NULL, NULL) == 0) {
            tsm_vte_set_palette(term_vte, "solarized-black"); // Init colors
            pid_t pid = launch_terminal_shell(terminal_mode);
            if (pid > 0) {
                DEBUG_LOG("[DEBUG] PTY ouvert avec succès.");
            }
        } else {
            DEBUG_LOG("[ERROR] Impossible d'initialiser tsm_vte.");
        }
    } else {
        DEBUG_LOG("[ERROR] Impossible d'initialiser tsm_screen.");
    }

    DEBUG_LOG("Démarrage de la boucle UI.");
    current_state = STATE_RUNNING;
    int running = 1;
    unsigned int frame_count = 0;
    
    /* Variables pour le champ "Run command" */
    char command_buffer[256] = {0};
    int command_len = 0;
    int show_run = 0;
    int show_task_manager = 0;
    int show_power_menu = 0;
    int pm_selected_idx = 0;
    int run_as_root = 0;
    int run_command_ready = 0;

    /* Variables pour le champ "Change password" */
    char old_pwd_buffer[256] = {0};
    int old_pwd_len = 0;
    char new_pwd_buffer[256] = {0};
    int new_pwd_len = 0;
    char confirm_pwd_buffer[256] = {0};
    int confirm_pwd_len = 0;
    char pwd_msg_buffer[256] = {0};
    int show_change_password = 0;
    int show_passwords = 0;
    int cp_focus_idx = 0;

    /* Variables pour le sous-menu "Switch user" */
    int show_switch_user = 0;
    int su_focus_idx = 0;
    int su_selected_idx = 0;

    /* Index de sélection clavier (6 = Cancel par défaut) */
    int selected_idx = 6;
    int trigger_action = -1;
    struct nk_color hover_color = THEME_COLOR_HOVER;

    /* Application du style Windows 10 */
    set_windows10_style(ctx);
    
    /* Désactiver le curseur interne de Nuklear (le petit carré parasite) */
    ctx->style.cursor_visible = 0;

    int front_buf = 0;
    /* Anti-clic-fantôme : cooldown de frames après ouverture d'un dialogue */
    int click_cooldown = 0;

    while (running) {
        int back_buf = drm.double_buffered ? (front_buf ^ 1) : 0;
        rawfb->fb.pixels = drm.buffer[back_buf];
        int prev_show_terminal = show_terminal;
        int cp_enter_pressed = 0;
        int is_modal = show_password_prompt;

        /* Snapshot de l'état des dialogues pour détecter les transitions */
        int snap_show_switch_user = show_switch_user;
        int snap_show_change_password = show_change_password;
        int snap_show_run = show_run;
        int snap_show_task_manager = show_task_manager;
        int snap_show_terminal = show_terminal;
        int snap_show_power_menu = show_power_menu;
        int snap_show_password_prompt = show_password_prompt;

        /* La mise à jour asynchrone de l'IP a été supprimée (trop risqué dans la boucle UI) */
        /* Rafraîchissement du Task Manager (toutes les 60 frames = ~1 sec) et nettoyage des zombies */
        if (frame_count % 60 == 0) {
            if (show_task_manager) {
                scan_processes();
            }
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
        if (frame_count % 500 == 0) {
            maintain_backlight();
        }
        frame_count++;

        /* 1. Lecture de la souris (avec synchronisation de paquets PS/2 pour éviter les désynchronisations et clics fantômes) */
        prev_mouse_btn_left = mouse_btn_left;
        int bytes_read_this_frame = 0;
        if (journal.mouse_fd >= 0) {
            unsigned char read_buf[128];
            ssize_t n;
            int mse_limit = 0;
            // On lit tout ce qui est dispo (non-bloquant) pour éviter de saturer
            while ((n = read(journal.mouse_fd, read_buf, sizeof(read_buf))) > 0 && mse_limit++ < 16) {
                bytes_read_this_frame += n;
                for (ssize_t i = 0; i < n; i++) {
                    mouse_acc[mouse_acc_len++] = read_buf[i];
                    if (mouse_acc_len == 3) {
                        // Un paquet complet de 3 octets doit avoir le bit 3 de l'octet 0 à 1.
                        if ((mouse_acc[0] & 0x08) == 0x08) {
                            mouse_btn_left = mouse_acc[0] & 0x1;
                            signed char dx = (signed char)mouse_acc[1];
                            signed char dy = (signed char)mouse_acc[2];
                            mouse_x += dx;
                            mouse_y -= dy; // Axe Y inversé
                            
                            // Clamp aux limites de l'écran
                            if (mouse_x < 0) mouse_x = 0;
                            if (mouse_y < 0) mouse_y = 0;
                            if (mouse_x >= (int)drm.width) mouse_x = drm.width - 1;
                            if (mouse_y >= (int)drm.height) mouse_y = drm.height - 1;
                            
                            mouse_acc_len = 0;
                        } else {
                            // Désynchronisation détectée ! Décalage à gauche pour retrouver le bit de synchro.
                            mouse_acc[0] = mouse_acc[1];
                            mouse_acc[1] = mouse_acc[2];
                            mouse_acc_len = 2;
                        }
                    }
                }
            }
        }
        
        static int mouse_idle_frames = 0;
        if (bytes_read_this_frame > 0) {
            mouse_idle_frames = 0;
        } else {
            mouse_idle_frames++;
            if (mouse_idle_frames > 2) {
                mouse_acc_len = 0;
            }
            if (mouse_idle_frames > 5) {
                mouse_btn_left = 0;
                prev_mouse_btn_left = 0;
            }
        }
        
        static int ignore_clicks = 0;
        if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y || mouse_btn_left != prev_mouse_btn_left) {
            if (keyboard_active) {
                keyboard_active = 0;
                if (mouse_btn_left) {
                    ignore_clicks = 1;
                }
            }
            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;
        }

        if (ignore_clicks) {
            if (mouse_btn_left == 0) {
                ignore_clicks = 0;
            } else {
                mouse_btn_left = 0;
            }
        }

        /* Anti-clic-fantôme : suppression du clic pendant le cooldown */
        if (click_cooldown > 0) {
            click_cooldown--;
            mouse_btn_left = 0;
            prev_mouse_btn_left = 0;
        }

        trigger_action = -1;

        /* Détection de clic sur les boutons manuels (en bas à droite) */
        int btn_shutdown_x = drm.width - SHUTDOWN_W - 20;
        int btn_shutdown_y = drm.height - SHUTDOWN_H - 20;
        int btn_terminal_x = btn_shutdown_x - TERMINAL_W - 4;
        int btn_terminal_y = drm.height - TERMINAL_H - 20;
        
        if (!is_modal && mouse_btn_left && !prev_mouse_btn_left) {
            if (mouse_x >= btn_terminal_x && mouse_x <= btn_terminal_x + TERMINAL_W &&
                mouse_y >= btn_terminal_y && mouse_y <= btn_terminal_y + TERMINAL_H) {
                trigger_action = 7; // Open terminal
            } else if (mouse_x >= btn_shutdown_x && mouse_x <= btn_shutdown_x + SHUTDOWN_W &&
                mouse_y >= btn_shutdown_y && mouse_y <= btn_shutdown_y + SHUTDOWN_H) {
                trigger_action = 8;
            } else if (show_power_menu) {
                int pm_w = pm_menu_w;
                int pm_h = 180;
                int pm_x = drm.width - pm_w - 20;
                int pm_y = drm.height - SHUTDOWN_H - 20 - pm_h;
                if (!(mouse_x >= pm_x && mouse_x <= pm_x + pm_w &&
                      mouse_y >= pm_y && mouse_y <= pm_y + pm_h)) {
                    show_power_menu = 0;
                }
            }
        }

        if (!show_change_password) {
            if (old_pwd_len > 0 || new_pwd_len > 0 || confirm_pwd_len > 0) {
                secure_clear_buffer(old_pwd_buffer, sizeof(old_pwd_buffer));
                old_pwd_len = 0;
                secure_clear_buffer(new_pwd_buffer, sizeof(new_pwd_buffer));
                new_pwd_len = 0;
                secure_clear_buffer(confirm_pwd_buffer, sizeof(confirm_pwd_buffer));
                confirm_pwd_len = 0;
            }
        }
        if (!show_password_prompt) {
            if (root_pwd_len > 0) {
                secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                root_pwd_len = 0;
            }
        }

        nk_input_begin(ctx);
        
        /* 2. Lecture du clavier (Limité à 64 events/frame pour éviter l'Input Flood) */
        if (journal.kbd_fd >= 0) {
            // --- PTY DISPATCH ---
            if (term_pty) {
                int pty_status = shl_pty_dispatch(term_pty);
                if (pty_status < 0 && pty_status != -EAGAIN) {
                    // Shell a quitté (exit ou erreur) — nettoyage complet
                    pid_t child_pid = shl_pty_get_child(term_pty);
                    shl_pty_unref(term_pty);  // close fd + free (shl_pty_close est appelé en interne)
                    term_pty = NULL;
                    show_terminal = 0;
                    // Récolter le zombie (belt-and-suspenders, SA_NOCLDWAIT devrait suffire)
                    if (child_pid > 0) waitpid(child_pid, NULL, WNOHANG);
                }
            }

            struct input_event ev;
            int kbd_limit = 0;
            while (read(journal.kbd_fd, &ev, sizeof(ev)) > 0 && kbd_limit++ < 64) {
                if (ev.type == EV_KEY) {
                    int down = (ev.value == 1 || ev.value == 2);
                    
                    if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                        shift_pressed = down;
                        continue;
                    }
                    if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                        ctrl_pressed = down;
                        continue;
                    }

                    uint32_t sym = XKB_KEY_NoSymbol;
                    uint32_t unicode = 0;

                    if (down) {
                        if (!shift_pressed) {
                            if (ev.code >= KEY_1 && ev.code <= KEY_9) unicode = '1' + (ev.code - KEY_1);
                            else if (ev.code == KEY_0) unicode = '0';
                            else if (ev.code == KEY_MINUS) unicode = '-';
                            else if (ev.code == KEY_EQUAL) unicode = '=';
                            else if (ev.code == KEY_LEFTBRACE) unicode = '[';
                            else if (ev.code == KEY_RIGHTBRACE) unicode = ']';
                            else if (ev.code == KEY_SEMICOLON) unicode = ';';
                            else if (ev.code == KEY_APOSTROPHE) unicode = '\'';
                            else if (ev.code == KEY_GRAVE) unicode = '`';
                            else if (ev.code == KEY_BACKSLASH) unicode = '\\';
                            else if (ev.code == KEY_COMMA) unicode = ',';
                            else if (ev.code == KEY_DOT) unicode = '.';
                            else if (ev.code == KEY_SLASH) unicode = '/';
                        } else {
                            if (ev.code == KEY_1) unicode = '!';
                            else if (ev.code == KEY_2) unicode = '@';
                            else if (ev.code == KEY_3) unicode = '#';
                            else if (ev.code == KEY_4) unicode = '$';
                            else if (ev.code == KEY_5) unicode = '%';
                            else if (ev.code == KEY_6) unicode = '^';
                            else if (ev.code == KEY_7) unicode = '&';
                            else if (ev.code == KEY_8) unicode = '*';
                            else if (ev.code == KEY_9) unicode = '(';
                            else if (ev.code == KEY_0) unicode = ')';
                            else if (ev.code == KEY_MINUS) unicode = '_';
                            else if (ev.code == KEY_EQUAL) unicode = '+';
                            else if (ev.code == KEY_LEFTBRACE) unicode = '{';
                            else if (ev.code == KEY_RIGHTBRACE) unicode = '}';
                            else if (ev.code == KEY_SEMICOLON) unicode = ':';
                            else if (ev.code == KEY_APOSTROPHE) unicode = '"';
                            else if (ev.code == KEY_GRAVE) unicode = '~';
                            else if (ev.code == KEY_BACKSLASH) unicode = '|';
                            else if (ev.code == KEY_COMMA) unicode = '<';
                            else if (ev.code == KEY_DOT) unicode = '>';
                            else if (ev.code == KEY_SLASH) unicode = '?';
                        }
                        
                        if (ev.code == KEY_Q) unicode = shift_pressed ? 'Q' : 'q';
                        else if (ev.code == KEY_W) unicode = shift_pressed ? 'W' : 'w';
                        else if (ev.code == KEY_E) unicode = shift_pressed ? 'E' : 'e';
                        else if (ev.code == KEY_R) unicode = shift_pressed ? 'R' : 'r';
                        else if (ev.code == KEY_T) unicode = shift_pressed ? 'T' : 't';
                        else if (ev.code == KEY_Y) unicode = shift_pressed ? 'Y' : 'y';
                        else if (ev.code == KEY_U) unicode = shift_pressed ? 'U' : 'u';
                        else if (ev.code == KEY_I) unicode = shift_pressed ? 'I' : 'i';
                        else if (ev.code == KEY_O) unicode = shift_pressed ? 'O' : 'o';
                        else if (ev.code == KEY_P) unicode = shift_pressed ? 'P' : 'p';
                        else if (ev.code == KEY_A) unicode = shift_pressed ? 'A' : 'a';
                        else if (ev.code == KEY_S) unicode = shift_pressed ? 'S' : 's';
                        else if (ev.code == KEY_D) unicode = shift_pressed ? 'D' : 'd';
                        else if (ev.code == KEY_F) unicode = shift_pressed ? 'F' : 'f';
                        else if (ev.code == KEY_G) unicode = shift_pressed ? 'G' : 'g';
                        else if (ev.code == KEY_H) unicode = shift_pressed ? 'H' : 'h';
                        else if (ev.code == KEY_J) unicode = shift_pressed ? 'J' : 'j';
                        else if (ev.code == KEY_K) unicode = shift_pressed ? 'K' : 'k';
                        else if (ev.code == KEY_L) unicode = shift_pressed ? 'L' : 'l';
                        else if (ev.code == KEY_Z) unicode = shift_pressed ? 'Z' : 'z';
                        else if (ev.code == KEY_X) unicode = shift_pressed ? 'X' : 'x';
                        else if (ev.code == KEY_C) unicode = shift_pressed ? 'C' : 'c';
                        else if (ev.code == KEY_V) unicode = shift_pressed ? 'V' : 'v';
                        else if (ev.code == KEY_B) unicode = shift_pressed ? 'B' : 'b';
                        else if (ev.code == KEY_N) unicode = shift_pressed ? 'N' : 'n';
                        else if (ev.code == KEY_M) unicode = shift_pressed ? 'M' : 'm';
                        else if (ev.code == KEY_SPACE) unicode = ' ';
                        
                        else if (ev.code == KEY_KP0) unicode = '0';
                        else if (ev.code == KEY_KP1) unicode = '1';
                        else if (ev.code == KEY_KP2) unicode = '2';
                        else if (ev.code == KEY_KP3) unicode = '3';
                        else if (ev.code == KEY_KP4) unicode = '4';
                        else if (ev.code == KEY_KP5) unicode = '5';
                        else if (ev.code == KEY_KP6) unicode = '6';
                        else if (ev.code == KEY_KP7) unicode = '7';
                        else if (ev.code == KEY_KP8) unicode = '8';
                        else if (ev.code == KEY_KP9) unicode = '9';
                        else if (ev.code == KEY_KPMINUS) unicode = '-';
                        else if (ev.code == KEY_KPPLUS) unicode = '+';
                        else if (ev.code == KEY_KPASTERISK) unicode = '*';
                        else if (ev.code == KEY_KPSLASH) unicode = '/';
                        else if (ev.code == KEY_KPDOT) unicode = '.';
                        
                        if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) sym = XKB_KEY_Return;
                        else if (ev.code == KEY_BACKSPACE) sym = XKB_KEY_BackSpace;
                        else if (ev.code == KEY_LEFT) sym = XKB_KEY_Left;
                        else if (ev.code == KEY_RIGHT) sym = XKB_KEY_Right;
                        else if (ev.code == KEY_UP) sym = XKB_KEY_Up;
                        else if (ev.code == KEY_DOWN) sym = XKB_KEY_Down;
                        else if (ev.code == KEY_ESC) sym = XKB_KEY_Escape;
                        else if (ev.code == KEY_TAB) sym = XKB_KEY_Tab;
                        else if (ev.code == KEY_DELETE) sym = XKB_KEY_Delete;
                        else if (ev.code == KEY_F1) sym = XKB_KEY_F1;
                        else if (ev.code == KEY_F2) sym = XKB_KEY_F2;
                        else if (ev.code == KEY_F3) sym = XKB_KEY_F3;
                        else if (ev.code == KEY_F4) sym = XKB_KEY_F4;
                        else if (ev.code == KEY_F5) sym = XKB_KEY_F5;
                        else if (ev.code == KEY_F6) sym = XKB_KEY_F6;
                        else if (ev.code == KEY_F7) sym = XKB_KEY_F7;
                        else if (ev.code == KEY_F8) sym = XKB_KEY_F8;
                        else if (ev.code == KEY_F9) sym = XKB_KEY_F9;
                        else if (ev.code == KEY_F10) sym = XKB_KEY_F10;

                        if (show_terminal && term_vte && term_screen && !is_modal) {
                            if (sym == XKB_KEY_Escape) {
                                show_terminal = 0;
                            } else if (sym != XKB_KEY_NoSymbol || unicode != 0) {
                                unsigned int mods = 0;
                                if (shift_pressed) mods |= TSM_SHIFT_MASK;
                                if (ctrl_pressed) mods |= TSM_CONTROL_MASK;
                                tsm_vte_handle_keyboard(term_vte, sym, unicode, mods, unicode);
                            }
                            continue;
                        }
                    }
                    
                    if (ev.code == KEY_BACKSPACE) {
                        nk_input_key(ctx, NK_KEY_BACKSPACE, down);
                    } else if (ev.code == KEY_ENTER || ev.code == KEY_KPENTER) {
                        if (show_password_prompt || show_task_manager || show_run || show_change_password || show_power_menu || show_terminal || show_switch_user) {
                            nk_input_key(ctx, NK_KEY_ENTER, down);
                        }
                    } else if (show_password_prompt || (show_task_manager && tm_focus_group == 1) || show_run || show_change_password) {
                        // Injection native des flèches gauche/droite pour nk_edit_string
                        if (ev.code == KEY_LEFT) nk_input_key(ctx, NK_KEY_LEFT, down);
                        else if (ev.code == KEY_RIGHT) nk_input_key(ctx, NK_KEY_RIGHT, down);
                    }
                    
                    if (down) {
                        if (ev.code == KEY_ESC) {
                            if (show_password_prompt) {
                                show_password_prompt = 0;
                                if (pwd_prompt_source == 3) run_as_root = 0;
                            } else if (show_task_manager) {
                                show_task_manager = 0;
                            } else if (show_run) {
                                show_run = 0;
                            } else if (show_change_password) {
                                show_change_password = 0;
                            } else if (show_switch_user) {
                                show_switch_user = 0;
                            } else if (show_power_menu) {
                                show_power_menu = 0;
                            } else {
                                running = 0;
                            }
                        } else if (show_password_prompt && ev.code == KEY_TAB) {
                            if (shift_pressed) {
                                pwd_focus_idx = (pwd_focus_idx - 1 + 3) % 3;
                            } else {
                                pwd_focus_idx = (pwd_focus_idx + 1) % 3;
                            }
                            keyboard_active = 1;
                        } else if (show_password_prompt && (ev.code == KEY_ENTER || ev.code == KEY_KPENTER || ev.code == KEY_SPACE)) {
                            keyboard_active = 1;
                            if (ev.code == KEY_SPACE && pwd_focus_idx == 2) {
                                show_password_prompt = 0;
                                if (pwd_prompt_source == 3) run_as_root = 0;
                            } else if (ev.code == KEY_SPACE && pwd_focus_idx == 1) {
                                pwd_enter_pressed = 1;
                            } else {
                                if (pwd_focus_idx == 2 && ev.code == KEY_ENTER) {
                                    show_password_prompt = 0;
                                    if (pwd_prompt_source == 3) run_as_root = 0;
                                } else {
                                    pwd_enter_pressed = 1;
                                }
                            }
                        } else if (show_change_password && ev.code == KEY_TAB) {
                            if (shift_pressed) {
                                cp_focus_idx = (cp_focus_idx - 1 + 6) % 6;
                            } else {
                                cp_focus_idx = (cp_focus_idx + 1) % 6;
                            }
                            keyboard_active = 1;
                        } else if (show_change_password && (ev.code == KEY_ENTER || ev.code == KEY_KPENTER || ev.code == KEY_SPACE)) {
                            keyboard_active = 1;
                            if (ev.code == KEY_SPACE) {
                                if (cp_focus_idx == 3) {
                                    show_passwords = !show_passwords;
                                } else if (cp_focus_idx == 4) {
                                    cp_enter_pressed = 1;
                                } else if (cp_focus_idx == 5) {
                                    show_change_password = 0;
                                }
                            } else { // Enter / KPEnter
                                if (cp_focus_idx == 3) {
                                    show_passwords = !show_passwords;
                                } else if (cp_focus_idx == 5) {
                                    show_change_password = 0;
                                } else { // 0, 1, 2, 4
                                    cp_enter_pressed = 1;
                                }
                            }
                        } else if (show_switch_user && ev.code == KEY_TAB) {
                            if (shift_pressed) {
                                su_focus_idx = (su_focus_idx - 1 + 3) % 3;
                            } else {
                                su_focus_idx = (su_focus_idx + 1) % 3;
                            }
                            keyboard_active = 1;
                        } else if (show_switch_user && su_focus_idx == 0 && ev.code == KEY_UP) {
                            if (su_selected_idx > 0) su_selected_idx--;
                            keyboard_active = 1;
                        } else if (show_switch_user && su_focus_idx == 0 && ev.code == KEY_DOWN) {
                            if (su_selected_idx < num_sessions - 1) su_selected_idx++;
                            keyboard_active = 1;
                        } else if (show_switch_user && (ev.code == KEY_ENTER || ev.code == KEY_KPENTER || ev.code == KEY_SPACE)) {
                            keyboard_active = 1;
                            cp_enter_pressed = 1; // Réutiliser pour la validation du bouton
                        } else if (!is_modal && show_task_manager && ev.code == KEY_TAB) {
                            int current_procs = (sort_mode_cpu == 0) ? num_top_procs_mem : ((sort_mode_cpu == 1) ? num_top_procs_cpu : num_top_procs_search);
                            int guard = 0;
                            do {
                                tm_focus_group = (tm_focus_group + 1) % 4;
                                if (++guard >= 4) break; // Sécurité anti-boucle infinie
                            } while (
                                (tm_focus_group == 1 && sort_mode_cpu != 2) || // Saute le champ Search si pas en vue Search
                                (tm_focus_group == 2 && current_procs == 0)    // Saute la liste si elle est vide
                            );
                            keyboard_active = 1;
                        } else if (!is_modal && show_task_manager && ev.code == KEY_LEFT) {
                            if (tm_focus_group == 0 && tm_selected_tab > 0) tm_selected_tab--;
                            else if (tm_focus_group == 2 && tm_selected_col > 0) tm_selected_col--;
                            keyboard_active = 1;
                        } else if (!is_modal && show_task_manager && ev.code == KEY_RIGHT) {
                            if (tm_focus_group == 0 && tm_selected_tab < 2) tm_selected_tab++;
                            else if (tm_focus_group == 2 && tm_selected_col < 3) tm_selected_col++;
                            keyboard_active = 1;
                        } else if (!is_modal && show_task_manager && ev.code == KEY_UP) {
                            if (tm_focus_group == 2 && tm_selected_row > 0) tm_selected_row--;
                            keyboard_active = 1;
                        } else if (!is_modal && show_task_manager && ev.code == KEY_DOWN) {
                            if (tm_focus_group == 2) {
                                int current_procs = (sort_mode_cpu == 0) ? num_top_procs_mem : ((sort_mode_cpu == 1) ? num_top_procs_cpu : num_top_procs_search);
                                if (tm_selected_row < current_procs - 1) tm_selected_row++;
                            }
                            keyboard_active = 1;
                        } else if (!is_modal && !show_task_manager && !show_run && !show_change_password && !show_power_menu && !show_terminal && !show_switch_user && ev.code == KEY_UP) {
                            selected_idx = (selected_idx - 1 + 9) % 9;
                            keyboard_active = 1;
                        } else if (!is_modal && !show_task_manager && !show_run && !show_change_password && !show_power_menu && !show_terminal && !show_switch_user && ev.code == KEY_DOWN) {
                            selected_idx = (selected_idx + 1) % 9;
                            keyboard_active = 1;
                        } else if (!is_modal && show_power_menu && ev.code == KEY_UP) {
                            pm_selected_idx = (pm_selected_idx - 1 + 3) % 3;
                            keyboard_active = 1;
                        } else if (!is_modal && show_power_menu && ev.code == KEY_DOWN) {
                            pm_selected_idx = (pm_selected_idx + 1) % 3;
                            keyboard_active = 1;
                        } else if (!is_modal && (ev.code == KEY_ENTER || ev.code == KEY_KPENTER || (show_task_manager && ev.code == KEY_SPACE && tm_focus_group != 0 && tm_focus_group != 1))) {
                            if (show_task_manager) {
                                trigger_action = 100;
                            } else if (show_power_menu) {
                                if (pm_selected_idx == 0) {
                                    execute_sleep(drm.fb_id[front_buf]);
                                    show_power_menu = 0;
                                } else if (pm_selected_idx == 1) {
                                    pid_t pid = fork();
                                    if (pid == 0) {
                                        setsid();
                                        if (fork() == 0) {
                                            execute_restart();
                                            _exit(0);
                                        }
                                        _exit(0);
                                    }
                                    running = 0;
                                } else if (pm_selected_idx == 2) {
                                    pid_t pid = fork();
                                    if (pid == 0) {
                                        setsid();
                                        if (fork() == 0) {
                                            execute_shutdown();
                                            _exit(0);
                                        }
                                        _exit(0);
                                    }
                                    running = 0;
                                }
                            } else if (!show_run && !show_change_password && !show_power_menu && !show_terminal && !show_switch_user && ev.code == KEY_ENTER) {
                                trigger_action = selected_idx;
                            }
                            keyboard_active = 1;
                        } else if (down && unicode != 0) {
                            nk_input_unicode(ctx, unicode);
                        }
                    }
                }
            }
        }

        /* 3. Transmission de la souris à Nuklear */
        if (keyboard_active) {
            /* Eloigner virtuellement la souris pour que Nuklear "oublie" le survol */
            nk_input_motion(ctx, -1000, -1000);
            nk_input_button(ctx, NK_BUTTON_LEFT, -1000, -1000, 0);
        } else {
            nk_input_motion(ctx, mouse_x, mouse_y);
            /* click_cooldown déjà appliqué : mouse_btn_left est à 0 si cooldown actif */
            nk_input_button(ctx, NK_BUTTON_LEFT, mouse_x, mouse_y, mouse_btn_left);
        }
        nk_input_end(ctx);

        /* --- OVERLAY INFORMATIONS SYSTEME (Haut Gauche) --- */
        if (!show_password_prompt) {
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(0, 0);
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            nk_flags sys_info_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND;
            if (nk_begin(ctx, "SysInfo", nk_rect(20, 0, 800, 60), sys_info_flags)) {
                nk_layout_row_dynamic(ctx, 40, 1);
                nk_label(ctx, sys_info_str, NK_TEXT_LEFT);
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);
            ctx->style.window.padding = old_pad;
        }

#if HAS_BRANDING
        /* --- BRANDING LOGO (Haut Droite) --- */
        if (!show_password_prompt) {
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(0, 0);
            nk_flags branding_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND;
            if (nk_begin(ctx, "Branding", nk_rect(drm.width - BRANDING_W - 20, 20, BRANDING_W, BRANDING_H), branding_flags)) {
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_layout_row_static(ctx, BRANDING_H, BRANDING_W, 1);
                struct nk_rect bounds = nk_widget_bounds(ctx);
                nk_draw_image(canvas, bounds, &nk_img_branding, nk_white);
            }
            nk_end(ctx);
            ctx->style.window.padding = old_pad;
        }
#endif

        /* 3. Définition de l'interface (Immediate Mode) */
        // La fenêtre elle-même est invisible grâce au style
        nk_flags cad_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND;
        int menu_w = 300;
        float max_w = 200.0f;
        if (!show_password_prompt) {
            const struct nk_user_font *font = ctx->style.font;
            float w0 = font->width(font->userdata, font->height, current_trans->btn_lock, strlen(current_trans->btn_lock));
            float w1 = font->width(font->userdata, font->height, current_trans->btn_switch_user, strlen(current_trans->btn_switch_user));
            float w2 = font->width(font->userdata, font->height, current_trans->btn_sign_out, strlen(current_trans->btn_sign_out));
            float w3 = font->width(font->userdata, font->height, current_trans->btn_change_password, strlen(current_trans->btn_change_password));
            float w4 = font->width(font->userdata, font->height, current_trans->btn_task_manager, strlen(current_trans->btn_task_manager));
            float w5 = font->width(font->userdata, font->height, current_trans->btn_run_command, strlen(current_trans->btn_run_command));
            
            max_w = w0;
            if (w1 > max_w) max_w = w1;
            if (w2 > max_w) max_w = w2;
            if (w3 > max_w) max_w = w3;
            if (w4 > max_w) max_w = w4;
            if (w5 > max_w) max_w = w5;
            
            menu_w = (int)max_w + 60; // 60px padding interne et marge de sécurité
            if (menu_w < 300) menu_w = 300;
        }
        if (!show_password_prompt) {
            if (nk_begin(ctx, "CAD", nk_rect(drm.width / 2 - menu_w / 2, drm.height / 2 - 250, menu_w, 600), cad_flags)) {
            
            if (!show_task_manager && !show_terminal && !show_run && !show_change_password && !show_switch_user) {
                float pad_x = ((float)menu_w - max_w) / 2.0f;
                if (pad_x < 0.0f) pad_x = 0.0f;

                enum nk_text_alignment old_align = ctx->style.button.text_alignment;
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;

                ctx->style.button.text_alignment = NK_TEXT_LEFT;
                ctx->style.button.padding = nk_vec2(pad_x, 15.0f);

                // Bouton 0 : Lock
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 0) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_lock) || trigger_action == 0) {
                    if (user_session.lock_cmd[0]) {
                        /* Fork un processus détaché pour locker la session APRÈS le retour à X11 */
                        pid_t pid = fork();
                        if (pid == 0) {
                            setsid();
                            if (fork() == 0) {
                                /* Attente déterministe (fool-proof) du retour sur le VT de la session */
                                for (int i = 0; i < 50; i++) { // Timeout de sécurité ~5 secondes
                                    struct vt_stat vts;
                                    int tty_fd = open("/dev/tty0", O_RDONLY);
                                    if (tty_fd >= 0) {
                                        if (ioctl(tty_fd, VT_GETSTATE, &vts) == 0) {
                                            if (vts.v_active == journal.original_vt) {
                                                close(tty_fd);
                                                break;
                                            }
                                        }
                                        close(tty_fd);
                                    }
                                    usleep(100000); // 100ms
                                }
                                execute_as_user(user_session.lock_cmd);
                                _exit(0);
                            }
                            _exit(0);
                        }
                        /* Quitter la GUI pour déclencher restore_vt() et revenir sur X11 */
                        running = 0;
                    }
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Bouton 1 : Switch user
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 1) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_switch_user) || trigger_action == 1) {
                    DEBUG_LOG("[GUI] Clic: Switch user");
                    scan_sessions();
                    show_switch_user = 1;
                    show_run = 0;
                    show_task_manager = 0;
                    show_terminal = 0;
                    show_power_menu = 0;
                    show_change_password = 0;
                    su_focus_idx = 0;
                    su_selected_idx = 0;
                    if (trigger_action == 1) trigger_action = -1;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Bouton 2 : Sign out
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 2) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_sign_out) || trigger_action == 2) {
                    DEBUG_LOG("[GUI] Clic: Sign out");
                    /* Fork un processus détaché pour déconnecter l'utilisateur APRÈS le retour à X11 */
                    pid_t pid = fork();
                    if (pid == 0) {
                        setsid();
                        if (fork() == 0) {
                            execute_sign_out();
                            _exit(0);
                        }
                        _exit(0);
                    }
                    /* Quitter la GUI pour déclencher restore_vt() et revenir sur X11 */
                    running = 0;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Bouton 3 : Change password
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 3) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_change_password) || trigger_action == 3) {
                    DEBUG_LOG("[GUI] Clic: Change password");
                    show_change_password = 1;
                    show_run = 0;
                    show_task_manager = 0;
                    show_terminal = 0;
                    show_power_menu = 0;
                    show_switch_user = 0;
                    secure_clear_buffer(old_pwd_buffer, sizeof(old_pwd_buffer));
                    old_pwd_len = 0;
                    secure_clear_buffer(new_pwd_buffer, sizeof(new_pwd_buffer));
                    new_pwd_len = 0;
                    secure_clear_buffer(confirm_pwd_buffer, sizeof(confirm_pwd_buffer));
                    confirm_pwd_len = 0;
                    memset(pwd_msg_buffer, 0, sizeof(pwd_msg_buffer));
                    cp_focus_idx = 0;
                    if (trigger_action == 3) trigger_action = -1;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Bouton 4 : Task manager
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 4) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_task_manager) || trigger_action == 4) {
                    show_task_manager = !show_task_manager;
                    if (show_task_manager) scan_processes(); // Scan immédiat à l'ouverture
                    show_run = 0; // Fermer l'autre panneau
                    show_change_password = 0;
                    show_switch_user = 0;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Bouton 5 : Run command
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 5) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_run_command) || trigger_action == 5) {
                    show_run = !show_run;
                    show_task_manager = 0;
                    show_terminal = 0;
                    show_power_menu = 0;
                    show_change_password = 0;
                    show_switch_user = 0;
                    if (trigger_action == 5) trigger_action = -1;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));

                // Restore main menu text alignment and padding
                ctx->style.button.text_alignment = old_align;
                ctx->style.button.padding = old_btn_pad;
                

                /* Espace vide avant le bouton Cancel */
                nk_layout_row_dynamic(ctx, 80, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_dynamic(ctx, 45, 1);
                if (keyboard_active && selected_idx == 6) ctx->style.button.normal = nk_style_item_color(hover_color);
                
                if (nk_button_label(ctx, current_trans->btn_cancel) || trigger_action == 6) {
                    running = 0;
                }
                
                /* On restaure le style normal pour le prochain tour */
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            } else {
                if (trigger_action >= 0 && trigger_action <= 6) {
                    trigger_action = -1;
                }
            }
        }
            nk_end(ctx);
        }



        /* --- FENETRE TERMINAL --- */
        if (show_terminal && !show_password_prompt && term_screen && term_vte) {
            int term_w = THEME_TERM_W(drm.width);
            int term_h = THEME_TERM_H(drm.height);
            int term_x = THEME_TERM_X(drm.width, term_w);
            int term_y = THEME_TERM_Y(drm.height, term_h);

            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);

            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(5, 5);

            int main_h = term_h - 85;
            nk_flags t_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (show_password_prompt) {
                t_flags |= NK_WINDOW_NO_INPUT;
            }

            // --- TERMINAL TITLE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_TITLE_BG);
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            if (nk_begin(ctx, "TerminalTitle", nk_rect(term_x, term_y - 84, term_w, 36), t_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 4);
                
                // Left Icon (Console)
                nk_layout_row_push(ctx, 24);
                struct nk_rect icon_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_draw_image(canvas, nk_rect(icon_bounds.x, icon_bounds.y, 24, 24), &nk_img_console, nk_white);
                nk_spacing(ctx, 1);
                
                // Title
                nk_layout_row_push(ctx, term_w - 80 - root_mode_btn_w);
                nk_label(ctx, "Terminal", NK_TEXT_LEFT);
                
                // Root toggle button
                nk_layout_row_push(ctx, root_mode_btn_w);
                struct nk_style_item old_btn_style = ctx->style.button.normal;
                struct nk_style_item old_btn_style_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_style_active = ctx->style.button.active;
                
                if (terminal_root_mode) {
                    ctx->style.button.normal = nk_style_item_color(nk_rgb(200, 50, 50));
                    ctx->style.button.hover = nk_style_item_color(nk_rgb(220, 70, 70));
                    ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                } else {
                    ctx->style.button.normal = nk_style_item_color(nk_rgba(0,0,0,0));
                    ctx->style.button.hover = nk_style_item_color(THEME_COLOR_HOVER);
                    ctx->style.button.active = nk_style_item_color(THEME_COLOR_ACTIVE);
                }
                
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;
                ctx->style.button.padding = nk_vec2(6, 4);
                nk_style_push_font(ctx, &rawfb->font_mono->handle);
                if (nk_button_label(ctx, terminal_root_mode ? current_trans->btn_root_mode : current_trans->btn_user_mode)) {
                    if (terminal_root_mode) {
                        // Switch back to user mode
                        close_current_pty();
                        terminal_root_mode = 0;
                        launch_terminal_shell(terminal_mode);
                    } else {
                        // Switch to root mode
                        if (root_authorized) {
                            close_current_pty();
                            terminal_root_mode = 1;
                            launch_terminal_shell(terminal_mode);
                        } else {
                            show_password_prompt = 1;
                            pwd_prompt_source = 1; // terminal
                            secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                            root_pwd_len = 0;
                            root_auth_msg[0] = '\0';
                            pwd_focus_idx = 0;
                            keyboard_active = 1;
                        }
                    }
                }
                nk_style_pop_font(ctx);
                ctx->style.button.padding = old_btn_pad;
                
                ctx->style.button.normal = old_btn_style;
                ctx->style.button.hover = old_btn_style_hover;
                ctx->style.button.active = old_btn_style_active;
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                ctx->style.button.hover = nk_style_item_color(nk_rgb(200, 50, 50));
                ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                
                if (nk_button_label(ctx, "")) {
                    show_terminal = 0;
                }
                
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                nk_draw_image(canvas, nk_rect(btn_bounds.x, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;
            nk_style_pop_font(ctx);

            // --- TERMINAL MODE SELECTOR (ABOVE TERMINAL) ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_vec2 old_mode_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 9);
            
            if (nk_begin(ctx, "TerminalMode", nk_rect(term_x, term_y - 48, term_w, 48), t_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                
                // Bash Button
                nk_layout_row_push(ctx, 75);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                
                if (terminal_mode == 0) {
                    ctx->style.button.normal = nk_style_item_color(nk_rgb(50, 120, 200));
                    ctx->style.button.hover = nk_style_item_color(nk_rgb(70, 140, 220));
                    ctx->style.button.active = nk_style_item_color(nk_rgb(50, 120, 200));
                }
                
                if (nk_button_label(ctx, "bash")) {
                    if (terminal_mode != 0) {
                        terminal_mode = 0;
                        close_current_pty();
                        tsm_screen_clear_sb(term_screen);
                        tsm_screen_erase_screen(term_screen, false);
                        tsm_screen_move_to(term_screen, 0, 0);
                        tsm_screen_reset(term_screen);
                        tsm_vte_reset(term_vte);
                        launch_terminal_shell(0);
                    }
                }
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                // Busybox Button
                nk_layout_row_push(ctx, 100);
                if (busybox_memfd < 0) {
                    /* Init tentée et échouée : bouton grisé */
                    ctx->style.button.normal = nk_style_item_color(nk_rgb(40, 45, 50));
                    ctx->style.button.hover = nk_style_item_color(nk_rgb(40, 45, 50));
                    ctx->style.button.active = nk_style_item_color(nk_rgb(40, 45, 50));
                    struct nk_color old_txt_normal = ctx->style.button.text_normal;
                    struct nk_color old_txt_hover = ctx->style.button.text_hover;
                    struct nk_color old_txt_active = ctx->style.button.text_active;
                    ctx->style.button.text_normal = nk_rgb(100, 100, 100);
                    ctx->style.button.text_hover = nk_rgb(100, 100, 100);
                    ctx->style.button.text_active = nk_rgb(100, 100, 100);
                    
                    nk_button_label(ctx, "busybox");
                    
                    ctx->style.button.normal = old_btn;
                    ctx->style.button.hover = old_btn_hover;
                    ctx->style.button.active = old_btn_active;
                    ctx->style.button.text_normal = old_txt_normal;
                    ctx->style.button.text_hover = old_txt_hover;
                    ctx->style.button.text_active = old_txt_active;
                } else {
                    if (terminal_mode == 1) {
                        ctx->style.button.normal = nk_style_item_color(nk_rgb(50, 120, 200));
                        ctx->style.button.hover = nk_style_item_color(nk_rgb(70, 140, 220));
                        ctx->style.button.active = nk_style_item_color(nk_rgb(50, 120, 200));
                    }
                    if (nk_button_label(ctx, "busybox")) {
                        if (terminal_mode != 1 && busybox_memfd >= 0) {
                            terminal_mode = 1;
                            close_current_pty();
                            tsm_screen_clear_sb(term_screen);
                            tsm_screen_erase_screen(term_screen, false);
                            tsm_screen_move_to(term_screen, 0, 0);
                            tsm_screen_reset(term_screen);
                            tsm_vte_reset(term_vte);
                            launch_terminal_shell(1);
                        }
                    }
                    ctx->style.button.normal = old_btn;
                    ctx->style.button.hover = old_btn_hover;
                    ctx->style.button.active = old_btn_active;
                }
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.padding = old_mode_pad;
            nk_style_pop_font(ctx);

            nk_style_push_font(ctx, &rawfb->font_mono->handle);
            
            if (keyboard_active) {
                nk_window_set_focus(ctx, "Terminal");
            }

            if (nk_begin(ctx, "Terminal", nk_rect(term_x, term_y, term_w, main_h), t_flags)) {
                struct nk_rect bounds = nk_window_get_content_region(ctx);
                nk_layout_space_begin(ctx, NK_STATIC, bounds.h, 1);
                nk_layout_space_push(ctx, nk_rect(0, 0, bounds.w, bounds.h));
                
                struct terminal_render_ctx rctx;
                rctx.ctx = ctx;
                rctx.bounds = bounds;
                
                // Calcul précis de la largeur pour éviter tout chevauchement ou collage
                const struct nk_user_font *font = ctx->style.font;
                rctx.cell_w = font->width(font->userdata, font->height, "W", 1);
                rctx.cell_h = font->height;
                
                /* Taille fixe : le redimensionnement dynamique (qui déclenche des mallocs internes) est strictement supprimé. */
                (void)tsm_screen_get_width(term_screen);
                (void)tsm_screen_get_height(term_screen);

                tsm_screen_draw(term_screen, terminal_draw_cb, &rctx);
                
                nk_layout_space_end(ctx);
            }
            nk_end(ctx);
            
            nk_style_pop_font(ctx);
            
            // On repasse à la police GUI pour le bouton
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            ctx->style.window.padding = nk_vec2(20, 20);
            
            if (nk_begin(ctx, "TerminalClose", nk_rect(term_x, term_y + main_h, term_w, 85), t_flags)) {
                nk_layout_row_dynamic(ctx, 45, 3);
                nk_spacing(ctx, 1);
                if (nk_button_label(ctx, current_trans->btn_close)) {
                    show_terminal = 0;
                }
                nk_spacing(ctx, 1);
            }
            nk_end(ctx);
            
            nk_style_pop_font(ctx);

            ctx->style.window.padding = old_pad;
            ctx->style.window.fixed_background = old_bg;
        }

        /* Fenêtre Indépendante pour le Task Manager */
        if (show_task_manager && !show_password_prompt) {
            int tm_w = THEME_TM_W(drm.width);
            int tm_h = THEME_TM_H(drm.height);
            int tm_x = THEME_TM_X(drm.width, tm_w);
            int tm_y = THEME_TM_Y(drm.height, tm_h);

            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            // Fond légèrement plus foncé que l'écran (20, 50, 80 au lieu de 32, 80, 129)
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);

            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(20, 20); // Marges internes du Task Manager

            nk_style_push_font(ctx, &rawfb->font_small->handle);
            
            // --- TASK MANAGER TITLE BAR ---
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_TITLE_BG);
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            nk_flags tm_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (show_password_prompt) {
                tm_flags |= NK_WINDOW_NO_INPUT;
            }
            if (nk_begin(ctx, "TaskManagerTitle", nk_rect(tm_x, tm_y - 36, tm_w, 36), tm_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 4);
                
                // Left Icon (Task manager)
                nk_layout_row_push(ctx, 24);
                struct nk_rect icon_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_draw_image(canvas, nk_rect(icon_bounds.x, icon_bounds.y, 24, 24), &nk_img_taskmgr, nk_white);
                nk_spacing(ctx, 1);
                
                // Title
                nk_layout_row_push(ctx, tm_w - 80 - root_mode_btn_w);
                nk_label(ctx, current_trans->title_task_manager, NK_TEXT_LEFT);
                
                // Root toggle button
                nk_layout_row_push(ctx, root_mode_btn_w);
                struct nk_style_item old_btn_style = ctx->style.button.normal;
                struct nk_style_item old_btn_style_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_style_active = ctx->style.button.active;
                
                if (taskmgr_root_mode) {
                    ctx->style.button.normal = nk_style_item_color(nk_rgb(200, 50, 50));
                    ctx->style.button.hover = nk_style_item_color(nk_rgb(220, 70, 70));
                    ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                } else {
                    ctx->style.button.normal = nk_style_item_color(nk_rgba(0,0,0,0));
                    ctx->style.button.hover = nk_style_item_color(THEME_COLOR_HOVER);
                    ctx->style.button.active = nk_style_item_color(THEME_COLOR_ACTIVE);
                }
                
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;
                ctx->style.button.padding = nk_vec2(6, 4);
                nk_style_push_font(ctx, &rawfb->font_mono->handle);
                if (nk_button_label(ctx, taskmgr_root_mode ? current_trans->btn_root_mode : current_trans->btn_user_mode)) {
                    if (taskmgr_root_mode) {
                        taskmgr_root_mode = 0;
                    } else {
                        if (root_authorized) {
                            taskmgr_root_mode = 1;
                        } else {
                            show_password_prompt = 1;
                            pwd_prompt_source = 2; // task manager
                            secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                            root_pwd_len = 0;
                            root_auth_msg[0] = '\0';
                            pwd_focus_idx = 0;
                            keyboard_active = 1;
                        }
                    }
                }
                nk_style_pop_font(ctx);
                ctx->style.button.padding = old_btn_pad;
                
                ctx->style.button.normal = old_btn_style;
                ctx->style.button.hover = old_btn_style_hover;
                ctx->style.button.active = old_btn_style_active;
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                ctx->style.button.hover = nk_style_item_color(nk_rgb(200, 50, 50));
                ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                
                if (nk_button_label(ctx, "")) {
                    show_task_manager = 0;
                }
                
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                nk_draw_image(canvas, nk_rect(btn_bounds.x, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;

            int main_h = tm_h - 85;
            
            if (keyboard_active) {
                nk_window_set_focus(ctx, "TaskManager");
            }

            if (nk_begin(ctx, "TaskManager", nk_rect(tm_x, tm_y, tm_w, main_h), tm_flags)) {
                // Info Globales Système
                nk_layout_row_dynamic(ctx, 25, 1);
                char info_str[128];
                char tmp[16];
                safe_copy(info_str, "RAM: ", sizeof(info_str));
                int_to_str((int)used_ram_mb, tmp); safe_concat(info_str, tmp, sizeof(info_str));
                safe_concat(info_str, " / ", sizeof(info_str));
                int_to_str((int)total_ram_mb, tmp); safe_concat(info_str, tmp, sizeof(info_str));
                safe_concat(info_str, " ", sizeof(info_str));
                safe_concat(info_str, current_trans->unit_mb, sizeof(info_str));
                safe_concat(info_str, "   |   SWAP: ", sizeof(info_str));
                int_to_str((int)used_swap_mb, tmp); safe_concat(info_str, tmp, sizeof(info_str));
                safe_concat(info_str, " / ", sizeof(info_str));
                int_to_str((int)total_swap_mb, tmp); safe_concat(info_str, tmp, sizeof(info_str));
                safe_concat(info_str, " ", sizeof(info_str));
                safe_concat(info_str, current_trans->unit_mb, sizeof(info_str));
                safe_concat(info_str, "   |   CPU: ", sizeof(info_str));
                int_to_str(global_cpu_percent, tmp); safe_concat(info_str, tmp, sizeof(info_str));
                safe_concat(info_str, "%", sizeof(info_str));
                nk_label(ctx, info_str, NK_TEXT_CENTERED);

                // En-têtes (MEMORY / CPU)
                struct btn_style_snapshot snap_tabs;
                btn_style_save(ctx, &snap_tabs);

                nk_layout_row_template_begin(ctx, 35);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_push_static(ctx, 48);
                nk_layout_row_template_end(ctx);
                
                ctx->style.button.text_normal = nk_rgb(255, 255, 255);
                ctx->style.button.text_hover = nk_rgb(255, 255, 255);
                ctx->style.button.text_active = nk_rgb(255, 255, 255);
                
                int old_sort_mode = sort_mode_cpu;

                // --- GESTION DU FOCUS CLAVIER POUR LES ONGLETS ---
                int kb_ram = (keyboard_active && tm_focus_group == 0 && tm_selected_tab == 0);
                int kb_cpu = (keyboard_active && tm_focus_group == 0 && tm_selected_tab == 1);
                int kb_search = (keyboard_active && tm_focus_group == 0 && tm_selected_tab == 2);

                // Bouton MEMORY
                struct nk_style_item bg_ram_norm = (sort_mode_cpu == 0) ? nk_style_item_color(nk_rgb(50, 120, 200)) : nk_style_item_color(nk_rgb(30, 50, 80));
                ctx->style.button.normal = kb_ram ? nk_style_item_color((sort_mode_cpu == 0) ? nk_rgb(70, 140, 220) : nk_rgb(60, 80, 110)) : bg_ram_norm;
                ctx->style.button.hover = keyboard_active ? ctx->style.button.normal : snap_tabs.hover;
                
                if (nk_button_label(ctx, "TOP RAM") || (trigger_action == 100 && kb_ram)) { 
                    sort_mode_cpu = 0; 
                    if (trigger_action == 100) trigger_action = -1; 
                }

                // Bouton CPU
                struct nk_style_item bg_cpu_norm = (sort_mode_cpu == 1) ? nk_style_item_color(nk_rgb(50, 120, 200)) : nk_style_item_color(nk_rgb(30, 50, 80));
                ctx->style.button.normal = kb_cpu ? nk_style_item_color((sort_mode_cpu == 1) ? nk_rgb(70, 140, 220) : nk_rgb(60, 80, 110)) : bg_cpu_norm;
                ctx->style.button.hover = keyboard_active ? ctx->style.button.normal : snap_tabs.hover;
                
                if (nk_button_label(ctx, "TOP CPU") || (trigger_action == 100 && kb_cpu)) { 
                    sort_mode_cpu = 1; 
                    if (trigger_action == 100) trigger_action = -1; 
                }
                
                // Bouton SEARCH (Image)
                struct nk_style_item bg_search_norm = (sort_mode_cpu == 2) ? nk_style_item_color(nk_rgb(50, 120, 200)) : nk_style_item_color(nk_rgb(30, 50, 80));
                ctx->style.button.normal = kb_search ? nk_style_item_color((sort_mode_cpu == 2) ? nk_rgb(70, 140, 220) : nk_rgb(60, 80, 110)) : bg_search_norm;
                ctx->style.button.hover = keyboard_active ? ctx->style.button.normal : snap_tabs.hover;
                
                struct nk_rect bounds = nk_widget_bounds(ctx);
                (void)nk_input_is_mouse_hovering_rect(&ctx->input, bounds);
                
                if (nk_button_label(ctx, "") || (trigger_action == 100 && kb_search)) { 
                    sort_mode_cpu = 2; 
                    if (trigger_action == 100) trigger_action = -1; 
                }
                
                if (old_sort_mode != sort_mode_cpu) {
                    scan_processes(); // Force le rafraîchissement immédiat pour éviter l'affichage d'une frame vide
                }
                
                // Dessin manuel de l'image centrée pour éviter le stretch de nk_button_image
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                struct nk_rect img_rect;
                struct nk_image *img_to_draw = &nk_img_find;
                img_rect.w = FIND_W; img_rect.h = FIND_H;
                
                img_rect.x = bounds.x + (bounds.w - img_rect.w) / 2.0f;
                img_rect.y = bounds.y + (bounds.h - img_rect.h) / 2.0f;
                nk_draw_image(canvas, img_rect, img_to_draw, nk_rgb(255, 255, 255));

                // Restauration
                btn_style_restore(ctx, &snap_tabs);
                
                // Marge verticale commune pour décoller la liste / le champ de recherche
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                
                if (sort_mode_cpu == 2) {
                    nk_layout_row_begin(ctx, NK_DYNAMIC, 35, 4);
                    
                    nk_layout_row_push(ctx, 0.15f);
                    nk_spacing(ctx, 1);
                    
                    nk_layout_row_push(ctx, 0.15f);
                    nk_label(ctx, current_trans->label_process_name, NK_TEXT_RIGHT);
                    
                    nk_layout_row_push(ctx, 0.55f);
                    
                    struct nk_style_item old_edit_norm = ctx->style.edit.normal;
                    struct nk_style_item old_edit_hov = ctx->style.edit.hover;
                    struct nk_style_item old_edit_act = ctx->style.edit.active;
                    
                    if (keyboard_active && tm_focus_group == 1) {
                        nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE); // Force le focus clavier (avec mode insertion garanti)
                    } else if (keyboard_active) {
                        nk_edit_unfocus(ctx);
                    }
                    // Si mouse mode (keyboard_active == 0), on ne touche à rien, Nuklear gère le focus nativement.
                    
                    int old_search_len = search_len;
                    nk_edit_string(ctx, NK_EDIT_FIELD, search_buffer, &search_len, 63, nk_filter_default);
                    
                    ctx->style.edit.normal = old_edit_norm;
                    ctx->style.edit.hover = old_edit_hov;
                    ctx->style.edit.active = old_edit_act;
                    
                    if (search_len != old_search_len) {
                        search_buffer[search_len] = '\0'; // TRÈS IMPORTANT: Nuklear ne met pas de \0 à la fin quand on efface !
                        scan_processes(); // Mise à jour instantanée
                    }
                    
                    nk_layout_row_push(ctx, 0.15f);
                    nk_spacing(ctx, 1);
                    
                    nk_layout_row_end(ctx);
                    
                    // Marge verticale après le champ de recherche
                    nk_layout_row_dynamic(ctx, 5, 1);
                    nk_spacing(ctx, 1);
                }

                struct process_info *procs_list = (sort_mode_cpu == 0) ? top_procs_mem : ((sort_mode_cpu == 1) ? top_procs_cpu : top_procs_search);
                int n_procs = (sort_mode_cpu == 0) ? num_top_procs_mem : ((sort_mode_cpu == 1) ? num_top_procs_cpu : num_top_procs_search);
                
                // --- CALCUL DU NOMBRE D'ENTREES ---
                int start_y = (sort_mode_cpu == 2) ? 145 : 97;
                int bottom_y = tm_h - 85; // Limite fixée par la position absolue du bouton Close
                int available_space = bottom_y - start_y;
                int row_footprint = 35 + 4; // 35px (boutons) + 4px (marge Nuklear)
                
                int max_display = available_space / row_footprint;
                if (max_display < 1) max_display = 1;
                if (max_display > MAX_TOP_PROCS) max_display = MAX_TOP_PROCS; // Sécurité tampon BSS
                
                int limit = (n_procs > max_display) ? max_display : n_procs;
                
                // Clamp la ligne sélectionnée si la liste a rétréci
                if (tm_selected_row >= limit) tm_selected_row = (limit > 0) ? limit - 1 : 0;
                
                for (int i = 0; i < limit; i++) {
                    if (procs_list[i].pid <= 0) continue;
                    
                    int is_row_kb = (keyboard_active && tm_focus_group == 2 && tm_selected_row == i);
                    
                    // Ratio statique pour aligner les 4 boutons et le texte (4 colonnes au lieu de 5 !)
                    nk_layout_row_begin(ctx, NK_STATIC, 35, 4);
                    
                    // Sauvegarde des styles globaux
                    struct btn_style_snapshot snap_row;
                    btn_style_save(ctx, &snap_row);
                    
                    int can_manage = (taskmgr_root_mode || (int)procs_list[i].uid == user_session.uid);

                    // Fond commun pour tous les boutons (plus foncé que l'écran TM)
                    ctx->style.button.normal = nk_style_item_color(nk_rgb(10, 25, 45));
                    if (keyboard_active) {
                        ctx->style.button.hover = ctx->style.button.normal;
                        ctx->style.button.active = ctx->style.button.normal;
                    }
                    
                    // Bouton Kill (Texte Rouge)
                    nk_layout_row_push(ctx, 80);
                    struct nk_rect first_btn_bounds = nk_widget_bounds(ctx);
                    struct nk_color c_kill = can_manage ? nk_rgb(255, 80, 80) : nk_rgb(100, 100, 100);
                    int is_col0_kb = (is_row_kb && tm_selected_col == 0); // Kill est la colonne 0 clavier (la plus à gauche)
                    ctx->style.button.normal = is_col0_kb ? nk_style_item_color(nk_rgb(60, 20, 20)) : nk_style_item_color(nk_rgb(10, 25, 45));
                    if (keyboard_active) {
                        ctx->style.button.hover = ctx->style.button.normal;
                        ctx->style.button.active = ctx->style.button.normal;
                    }
                    ctx->style.button.text_normal = c_kill;
                    ctx->style.button.text_hover = c_kill;
                    ctx->style.button.text_active = can_manage ? nk_rgb(255, 120, 120) : nk_rgb(100, 100, 100);
                    if (nk_button_label(ctx, current_trans->btn_kill) || (trigger_action == 100 && is_col0_kb)) {
                        if (can_manage) {
                            // [SECURITY: TOCTOU Risk] The PID might have been reused between the scan and this action.
                            // Acceptable risk for a rescue tool (scan is frequent, pidfd requires kernel 5.1+).
                            kill(procs_list[i].pid, SIGKILL);
                            DEBUG_LOG_INT("[GUI] Processus tué (SIGKILL) : ", procs_list[i].pid);
                            procs_list[i].pid = -1; // Masquer
                        }
                        if (trigger_action == 100) trigger_action = -1;
                    }
                    
                    // Bouton Terminate (Texte Orange)
                    nk_layout_row_push(ctx, 140);
                    struct nk_color c_term = can_manage ? nk_rgb(255, 150, 50) : nk_rgb(100, 100, 100);
                    int is_col1_kb = (is_row_kb && tm_selected_col == 1);
                    ctx->style.button.normal = is_col1_kb ? nk_style_item_color(nk_rgb(60, 40, 20)) : nk_style_item_color(nk_rgb(10, 25, 45));
                    if (keyboard_active) {
                        ctx->style.button.hover = ctx->style.button.normal;
                        ctx->style.button.active = ctx->style.button.normal;
                    }
                    ctx->style.button.text_normal = c_term;
                    ctx->style.button.text_hover = c_term;
                    ctx->style.button.text_active = can_manage ? nk_rgb(255, 180, 100) : nk_rgb(100, 100, 100);
                    if (nk_button_label(ctx, current_trans->btn_terminate) || (trigger_action == 100 && is_col1_kb)) {
                        if (can_manage) {
                            // [SECURITY: TOCTOU Risk] Accepted.
                            kill(procs_list[i].pid, SIGTERM);
                            DEBUG_LOG_INT("[GUI] Processus terminé (SIGTERM) : ", procs_list[i].pid);
                        }
                        if (trigger_action == 100) trigger_action = -1;
                    }
                    
                    // Bouton Pause/Continue (Texte Vert/Bleu)
                    nk_layout_row_push(ctx, 120);
                    int is_paused = (procs_list[i].state == 'T' || procs_list[i].state == 't');
                    struct nk_color c_pause = can_manage ? (is_paused ? nk_rgb(100, 255, 100) : nk_rgb(100, 180, 255)) : nk_rgb(100, 100, 100);
                    int is_col2_kb = (is_row_kb && tm_selected_col == 2);
                    ctx->style.button.normal = is_col2_kb ? nk_style_item_color(nk_rgb(20, 50, 60)) : nk_style_item_color(nk_rgb(10, 25, 45));
                    if (keyboard_active) {
                        ctx->style.button.hover = ctx->style.button.normal;
                        ctx->style.button.active = ctx->style.button.normal;
                    }
                    ctx->style.button.text_normal = c_pause;
                    ctx->style.button.text_hover = c_pause;
                    ctx->style.button.text_active = can_manage ? (is_paused ? nk_rgb(150, 255, 150) : nk_rgb(150, 200, 255)) : nk_rgb(100, 100, 100);
                    if (nk_button_label(ctx, is_paused ? current_trans->btn_continue : current_trans->btn_pause) || (trigger_action == 100 && is_col2_kb)) {
                        if (can_manage) {
                            // [SECURITY: TOCTOU Risk] Accepted.
                            kill(procs_list[i].pid, is_paused ? SIGCONT : SIGSTOP);
                            DEBUG_LOG_INT("[GUI] Processus mis en pause/continu (SIGSTOP/SIGCONT) : ", procs_list[i].pid);
                            procs_list[i].state = is_paused ? 'R' : 'T'; // Mise à jour optimiste
                        }
                        if (trigger_action == 100) trigger_action = -1;
                    }
                    
                    // Restauration des styles
                    btn_style_restore(ctx, &snap_row);
                    
                    // Label PID + Nom + RAM/CPU + State
                    int total_label_w = tm_w - 390;
                    if (total_label_w < 100) total_label_w = 100;
                    
                    int pid_w = 75; // Largeur augmentée pour supporter 7 chiffres
                    int name_w = total_label_w - pid_w;
                    
                    // Détection du survol de la ligne entière
                    int is_row_hovered = (mouse_y >= first_btn_bounds.y && mouse_y < first_btn_bounds.y + first_btn_bounds.h && 
                                          mouse_x >= tm_x && mouse_x <= tm_x + tm_w);
                    int is_col3_kb = (is_row_kb && tm_selected_col == 3);
                    
                    if (keyboard_active && is_row_kb) {
                        struct nk_color row_color;
                        if (is_col3_kb) row_color = nk_rgb(60, 80, 110); // Focus clavier explicite sur le label
                        else row_color = nk_rgb(20, 35, 55); // Survol léger de la ligne via focus clavier
                        
                        struct nk_style_item bg_hover = nk_style_item_color(row_color);
                        ctx->style.button.normal = bg_hover;
                        ctx->style.button.hover = bg_hover;
                        ctx->style.button.active = bg_hover;
                    } else if (!keyboard_active && is_row_hovered) {
                        struct nk_style_item bg_hover = nk_style_item_color(nk_rgb(10, 25, 45)); // Hover classique souris
                        ctx->style.button.normal = bg_hover;
                        ctx->style.button.hover = bg_hover;
                        ctx->style.button.active = bg_hover;
                    } else {
                        struct nk_style_item bg_trans = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                        ctx->style.button.normal = bg_trans;
                        if (keyboard_active) {
                            ctx->style.button.hover = bg_trans;
                            ctx->style.button.active = bg_trans;
                        } else {
                            ctx->style.button.hover = snap_row.hover;
                            ctx->style.button.active = snap_row.active;
                        }
                    }
                    ctx->style.button.text_hover = nk_rgb(255, 255, 255);
                    ctx->style.button.text_active = nk_rgb(255, 255, 255);
                    
                    // --- COLONNE UNIFIEE (PID + NOM) ---
                    // Au lieu de séparer en deux boutons qui se chevauchent lors du dessin séquentiel,
                    // on crée un SEUL grand bouton pour avoir un fond hover parfaitement unifié,
                    // et on dessine le texte bicolore manuellement par-dessus.
                    nk_layout_row_push(ctx, pid_w + name_w + ctx->style.window.spacing.x);
                    
                    char proc_label[128];
                    safe_copy(proc_label, procs_list[i].name, sizeof(proc_label));
                    safe_concat(proc_label, " (", sizeof(proc_label));
                    
                    if (sort_mode_cpu == 0 || sort_mode_cpu == 2) {
                        int ram_mb = (procs_list[i].rss_pages * 4) / 1024;
                        char ram_str[16]; int_to_str(ram_mb, ram_str); safe_concat(proc_label, ram_str, sizeof(proc_label));
                        safe_concat(proc_label, " ", sizeof(proc_label));
                        safe_concat(proc_label, current_trans->unit_mb, sizeof(proc_label));
                    } else {
                        char cpu_str[16]; int_to_str(procs_list[i].cpu_percent, cpu_str); safe_concat(proc_label, cpu_str, sizeof(proc_label));
                        safe_concat(proc_label, "% CPU", sizeof(proc_label));
                    }
                    
                    safe_concat(proc_label, ") [", sizeof(proc_label));
                    char state_str[2] = { procs_list[i].state, '\0' }; safe_concat(proc_label, state_str, sizeof(proc_label));
                    safe_concat(proc_label, "] [", sizeof(proc_label));
                    safe_concat(proc_label, procs_list[i].username, sizeof(proc_label));
                    safe_concat(proc_label, "]", sizeof(proc_label));
                    
                    struct nk_rect label_bounds = nk_widget_bounds(ctx);
                    if (nk_button_label(ctx, "") || (trigger_action == 100 && is_col3_kb)) {
                        if (trigger_action == 100) trigger_action = -1;
                    }
                    
                    // Dessin personnalisé des textes
                    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                    const struct nk_user_font *font = ctx->style.font;
                    float text_h = font->height;
                    float text_y = label_bounds.y + (label_bounds.h - text_h) / 2.0f;
                    
                    char pid_str[16];
                    int_to_str(procs_list[i].pid, pid_str);
                    safe_concat(pid_str, ": ", sizeof(pid_str));
                    
                    // Dessin PID (Gris)
                    struct nk_rect pid_rect = nk_rect(label_bounds.x, text_y, pid_w, text_h);
                    float pid_text_w = font->width(font->userdata, font->height, pid_str, nk_strlen(pid_str));
                    pid_rect.x += pid_w - pid_text_w; // Alignement droit
                    nk_draw_text(canvas, pid_rect, pid_str, nk_strlen(pid_str), font, nk_rgba(0,0,0,0), nk_rgb(150, 150, 150));
                    
                    // Dessin Nom (Blanc)
                    struct nk_rect name_rect = nk_rect(label_bounds.x + pid_w, text_y, name_w, text_h);
                    nk_draw_text(canvas, name_rect, proc_label, nk_strlen(proc_label), font, nk_rgba(0,0,0,0), nk_rgb(255, 255, 255));

                    btn_style_restore(ctx, &snap_row);
                    
                    nk_layout_row_end(ctx);
                }
            }
            nk_end(ctx);
            
            // --- DEUXIEME FENETRE POUR LE BOUTON CLOSE ---
            // Isolation totale du layout pour garantir l'absence d'interférences
            // Placé exactement à la suite de la première fenêtre.
            if (nk_begin(ctx, "TaskManagerClose", nk_rect(tm_x, tm_y + main_h, tm_w, 85), tm_flags)) {
                nk_layout_row_dynamic(ctx, 45, 3);
                nk_spacing(ctx, 1); // Colonne gauche vide
                
                int is_kb_close = (keyboard_active && tm_focus_group == 3);
                struct nk_style_item old_btn_norm = ctx->style.button.normal;
                struct nk_style_item old_btn_hov = ctx->style.button.hover;
                
                ctx->style.button.normal = is_kb_close ? nk_style_item_color(nk_rgb(60, 80, 110)) : old_btn_norm;
                if (keyboard_active) ctx->style.button.hover = ctx->style.button.normal;
                
                if (nk_button_label(ctx, current_trans->btn_close) || (trigger_action == 100 && is_kb_close)) {
                    show_task_manager = 0;
                    if (trigger_action == 100) trigger_action = -1;
                }
                
                ctx->style.button.normal = old_btn_norm;
                ctx->style.button.hover = old_btn_hov;
                
                nk_spacing(ctx, 1); // Colonne droite vide
            }
            nk_end(ctx);
            
            // Consommation résiduelle si aucun bouton n'a intercepté l'action clavier
            if (trigger_action == 100) trigger_action = -1;
            
            ctx->style.window.fixed_background = old_bg; // Restore ORIGINAL background

            nk_style_pop_font(ctx);
            ctx->style.window.padding = old_pad;
        }

        /* Fenêtre Indépendante pour le Menu d'Alimentation */
        if (show_run && snap_show_run && !show_password_prompt) {
            int win_w = THEME_RUN_W(drm.width);
            int win_h = THEME_RUN_H(drm.height);
            int win_x = THEME_RUN_X(drm.width, win_w);
            int win_y = THEME_RUN_Y(drm.height, win_h);
            int main_h = win_h - 85;
            nk_flags run_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (show_password_prompt) {
                run_flags |= NK_WINDOW_NO_INPUT;
            }
            
            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);
            ctx->style.window.padding = nk_vec2(20, 20);
            
            // --- RUN COMMAND TITLE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(nk_rgba(30, 68, 105, 240));
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            if (nk_begin(ctx, "RunCommandTitle", nk_rect(win_x, win_y - 36, win_w, 36), run_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 3);
                
                // Left Icon (Run command)
                nk_layout_row_push(ctx, 24);
                struct nk_rect icon_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_draw_image(canvas, nk_rect(icon_bounds.x, icon_bounds.y, 24, 24), &nk_img_runcommand, nk_white);
                nk_spacing(ctx, 1);
                
                // Title
                nk_layout_row_push(ctx, win_w - 76);
                nk_label(ctx, current_trans->title_run_command, NK_TEXT_LEFT);
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                ctx->style.button.hover = nk_style_item_color(nk_rgb(200, 50, 50));
                ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                
                if (nk_button_label(ctx, "")) {
                    show_run = 0;
                }
                
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                nk_draw_image(canvas, nk_rect(btn_bounds.x, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;
            nk_style_pop_font(ctx);

            if (keyboard_active) {
                nk_window_set_focus(ctx, "RunCommand");
            }

            int is_enter_pressed = 0;
            
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "RunCommand", nk_rect(win_x, win_y, win_w, main_h), run_flags)) {
                
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, current_trans->label_enter_command, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_begin(ctx, NK_STATIC, 35, 1);
                nk_layout_row_push(ctx, win_w - 42); // 40px padding + 2px safety margin to avoid right border clipping
                int edit_state = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, command_buffer, &command_len, 255, nk_filter_default);
                if (edit_state & NK_EDIT_COMMITED) {
                    is_enter_pressed = 1;
                }
                nk_layout_row_end(ctx);
                
                /* Checkbox manuelle : bouton invisible + dessin de l'icône */
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;
                ctx->style.button.padding = nk_vec2(0, 0);
                
                nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
                nk_layout_row_push(ctx, 20);
                struct nk_rect cb_bounds = nk_widget_bounds(ctx);
                if (nk_button_label(ctx, "")) {
                    if (run_as_root) {
                        run_as_root = 0;
                    } else {
                        if (root_authorized) {
                            run_as_root = 1;
                        } else {
                            show_password_prompt = 1;
                            pwd_prompt_source = 3; // run command
                            secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                            root_pwd_len = 0;
                            root_auth_msg[0] = '\0';
                            pwd_focus_idx = 0;
                            keyboard_active = 1;
                        }
                    }
                }
                struct nk_image *check_img = run_as_root ? &nk_img_check2 : &nk_img_check1;
                nk_draw_image(canvas, nk_rect(cb_bounds.x, cb_bounds.y, 20, 20), check_img, nk_white);
                
                nk_layout_row_push(ctx, 250);
                nk_label_colored(ctx, current_trans->label_execute_as_root, NK_TEXT_LEFT, nk_rgb(200, 200, 200));
                nk_layout_row_end(ctx);
                
                ctx->style.button.padding = old_btn_pad;
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);
            
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "RunCommandClose", nk_rect(win_x, win_y + main_h, win_w, 85), run_flags)) {
                nk_layout_row_dynamic(ctx, 45, 2);
                if (nk_button_label(ctx, current_trans->btn_run) || is_enter_pressed) {
                    if (command_len > 0) {
                        safe_copy(user_session.run_cmd, command_buffer, sizeof(user_session.run_cmd));
                        run_command_ready = 1;
                        running = 0;
                    }
                }
                if (nk_button_label(ctx, current_trans->btn_close)) {
                    show_run = 0;
                }
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);
            
            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;
        }

        /* --- FULLSCREEN MODAL OVERLAY (DIM BACKGROUND & BLOCK INTERACTION) --- */
        if (show_password_prompt) {
            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_CURTAIN); // Voile bleu-gris foncé
            ctx->style.window.padding = nk_vec2(0, 0);
            if (nk_begin(ctx, "ModalOverlay", nk_rect(0, 0, drm.width, drm.height), NK_WINDOW_NO_SCROLLBAR)) {
                // Intercept all inputs
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;

            /* Si l'overlay modal a obtenu le focus suite à un clic extérieur,
               on redirige immédiatement le focus vers TOUTES les fenêtres du dialogue actif. */
            if (nk_window_is_active(ctx, "ModalOverlay")) {
                if (show_password_prompt) {
                    nk_window_set_focus(ctx, "RootPasswordTitle");
                    nk_window_set_focus(ctx, "RootPasswordClose");
                    nk_window_set_focus(ctx, "RootPassword");
                }
            }
        }

        /* --- FENETRE DE SAISIE DE MOT DE PASSE ROOT --- */
        if (show_password_prompt) {
            int win_w = THEME_PWD_W(drm.width);
            int win_h = THEME_PWD_H(drm.height);
            int win_x = THEME_PWD_X(drm.width, win_w);
            int win_y = THEME_PWD_Y(drm.height, win_h);
            int main_h = win_h - 85;

            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);
            ctx->style.window.padding = nk_vec2(20, 10);

            // --- TITLE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_TITLE_BG);
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            if (nk_begin(ctx, "RootPasswordTitle", nk_rect(win_x, win_y - 36, win_w, 36), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 2);
                
                // Icon/Title
                nk_layout_row_push(ctx, win_w - 52);
                nk_label(ctx, current_trans->title_privilege_elevation, NK_TEXT_LEFT);
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_style_item old_btn = ctx->style.button.normal;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0,0,0,0));
                if (nk_button_label(ctx, "")) {
                    show_password_prompt = 0;
                    if (pwd_prompt_source == 3) run_as_root = 0;
                }
                ctx->style.button.normal = old_btn;
                
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                nk_draw_image(canvas, nk_rect(btn_bounds.x - 24, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;
            nk_style_pop_font(ctx);

            if (keyboard_active) {
                nk_window_set_focus(ctx, "RootPassword");
            }

            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "RootPassword", nk_rect(win_x, win_y, win_w, main_h), NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
                
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, current_trans->label_enter_admin_pwd, NK_TEXT_CENTERED);
                
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                
                // Text input for password
                nk_layout_row_begin(ctx, NK_STATIC, 35, 3);
                nk_layout_row_push(ctx, 60);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, win_w - 40 - 120);
                
                if (keyboard_active && pwd_focus_idx == 0) {
                    nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
                }
                
                int old_len = root_pwd_len;
                char temp_buf[256];
                for (int i = 0; i < root_pwd_len; ++i) temp_buf[i] = '*';
                temp_buf[root_pwd_len] = '\0';
                
                nk_flags res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, temp_buf, &root_pwd_len, 255, nk_filter_default);
                if (old_len < root_pwd_len) {
                    memcpy(&root_pwd_buffer[old_len], &temp_buf[old_len], root_pwd_len - old_len);
                }
                root_pwd_buffer[root_pwd_len] = '\0';
                
                if (res & NK_EDIT_ACTIVE) {
                    if (!keyboard_active) pwd_focus_idx = 0;
                }
                if (res & NK_EDIT_COMMITED) pwd_enter_pressed = 1;
                
                nk_layout_row_push(ctx, 60);
                nk_spacing(ctx, 1);
                nk_layout_row_end(ctx);
                
                /* Error message area - always 20px reserved */
                nk_layout_row_dynamic(ctx, 20, 1);
                if (root_auth_msg[0]) {
                    nk_label_colored(ctx, root_auth_msg, NK_TEXT_CENTERED, nk_rgb(255, 80, 80));
                } else {
                    nk_spacing(ctx, 1);
                }
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);

            // --- BOTTOM CLOSE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "RootPasswordClose", nk_rect(win_x, win_y + main_h, win_w, 85), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
                nk_layout_row_dynamic(ctx, 45, 2);
                
                struct nk_style_item old_btn_normal = ctx->style.button.normal;
                
                // Authenticate Button
                if (keyboard_active && pwd_focus_idx == 1) ctx->style.button.normal = nk_style_item_color(hover_color);
                int submit_clicked = nk_button_label(ctx, current_trans->btn_authenticate);
                ctx->style.button.normal = old_btn_normal;
                
                // Cancel Button
                if (keyboard_active && pwd_focus_idx == 2) ctx->style.button.normal = nk_style_item_color(hover_color);
                int cancel_clicked = nk_button_label(ctx, current_trans->btn_cancel);
                ctx->style.button.normal = old_btn_normal;
                
                if (submit_clicked || pwd_enter_pressed) {
                    pwd_enter_pressed = 0;
                    root_auth_msg[0] = '\0';
                    int verified = verify_root_password(root_pwd_buffer);
                    if (verified == 1) {
                        root_authorized = 1;
                        show_password_prompt = 0;
                        secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                        root_pwd_len = 0;
                        
                        // Apply state changes depending on source
                        if (pwd_prompt_source == 1) {
                            close_current_pty();
                            terminal_root_mode = 1;
                            launch_terminal_shell(terminal_mode);
                        } else if (pwd_prompt_source == 2) {
                            taskmgr_root_mode = 1;
                        } else if (pwd_prompt_source == 3) {
                            run_as_root = 1;
                        }
                    } else {
                        safe_copy(root_auth_msg, "Authentication failed. Try again.", sizeof(root_auth_msg));
                        secure_clear_buffer(root_pwd_buffer, sizeof(root_pwd_buffer));
                        root_pwd_len = 0;
                    }
                }
                
                if (cancel_clicked) {
                    show_password_prompt = 0;
                    if (pwd_prompt_source == 3) run_as_root = 0;
                }
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);
            
            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;
        }

        /* Fenêtre Indépendante pour le changement de mot de passe */
        if (show_change_password && snap_show_change_password && !show_password_prompt) {
            int win_w = THEME_CP_W(drm.width);
            int win_h = THEME_CP_H(drm.height);
            int win_x = THEME_CP_X(drm.width, win_w);
            int win_y = THEME_CP_Y(drm.height, win_h);
            int main_h = win_h - 85;
            
            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);
            ctx->style.window.padding = nk_vec2(20, 20);
            
            // --- CHANGE PASSWORD TITLE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_TITLE_BG);
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            nk_flags cp_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (nk_begin(ctx, "ChangePasswordTitle", nk_rect(win_x, win_y - 36, win_w, 36), cp_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 3);
                
                // Left Icon (Password)
                nk_layout_row_push(ctx, 24);
                struct nk_rect icon_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_draw_image(canvas, nk_rect(icon_bounds.x, icon_bounds.y, 24, 24), &nk_img_password, nk_white);
                nk_spacing(ctx, 1);
                
                // Title
                nk_layout_row_push(ctx, win_w - 76);
                nk_label(ctx, current_trans->title_change_password, NK_TEXT_LEFT);
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                ctx->style.button.hover = nk_style_item_color(nk_rgb(200, 50, 50));
                ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                
                if (nk_button_label(ctx, "")) {
                    show_change_password = 0;
                }
                
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                nk_draw_image(canvas, nk_rect(btn_bounds.x, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;
            nk_style_pop_font(ctx);

            if (keyboard_active) {
                nk_window_set_focus(ctx, "ChangePassword");
            }

            int is_enter_pressed = 0;
            
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_vec2 cp_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(20, 20);
            if (nk_begin(ctx, "ChangePassword", nk_rect(win_x, win_y, win_w, main_h), 
                cp_flags)) {
                
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                
                if (keyboard_active && (cp_focus_idx < 0 || cp_focus_idx > 2)) {
                    nk_edit_unfocus(ctx);
                }

                if (pwd_msg_buffer[0] != '\0') {
                    nk_layout_row_dynamic(ctx, 20, 1);
                    struct nk_color msg_color = (strstr(pwd_msg_buffer, "successfully") || strstr(pwd_msg_buffer, "succès") || strstr(pwd_msg_buffer, "erfolgreich") || strstr(pwd_msg_buffer, "con éxito") || strstr(pwd_msg_buffer, "con successo") || strstr(pwd_msg_buffer, "com sucesso")) ? nk_rgb(100, 240, 100) : nk_rgb(240, 100, 100);
                    nk_label_colored(ctx, pwd_msg_buffer, NK_TEXT_CENTERED, msg_color);
                    
                    nk_layout_row_dynamic(ctx, 5, 1);
                    nk_spacing(ctx, 1);
                }
                
                // Current Password
                nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                nk_label(ctx, current_trans->label_current_pwd, NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
                
                nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                {
                    nk_flags res;
                    if (keyboard_active && cp_focus_idx == 0) {
                        nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
                    }
                    if (show_passwords) {
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, old_pwd_buffer, &old_pwd_len, 255, nk_filter_default);
                        old_pwd_buffer[old_pwd_len] = '\0';
                    } else {
                        int old_len = old_pwd_len;
                        char temp_buf[256];
                        for (int i = 0; i < old_pwd_len; ++i) temp_buf[i] = '*';
                        temp_buf[old_pwd_len] = '\0';
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, temp_buf, &old_pwd_len, 255, nk_filter_default);
                        if (old_len < old_pwd_len) {
                            memcpy(&old_pwd_buffer[old_len], &temp_buf[old_len], old_pwd_len - old_len);
                        }
                        old_pwd_buffer[old_pwd_len] = '\0';
                    }
                    if (res & NK_EDIT_ACTIVE) {
                        if (!keyboard_active) cp_focus_idx = 0;
                    }
                    if (res & NK_EDIT_COMMITED) is_enter_pressed = 1;
                }
                nk_layout_row_end(ctx);
                
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                
                // New Password
                nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                nk_label(ctx, current_trans->label_new_pwd, NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
                
                nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                {
                    nk_flags res;
                    if (keyboard_active && cp_focus_idx == 1) {
                        nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
                    }
                    if (show_passwords) {
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, new_pwd_buffer, &new_pwd_len, 255, nk_filter_default);
                        new_pwd_buffer[new_pwd_len] = '\0';
                    } else {
                        int old_len = new_pwd_len;
                        char temp_buf[256];
                        for (int i = 0; i < new_pwd_len; ++i) temp_buf[i] = '*';
                        temp_buf[new_pwd_len] = '\0';
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, temp_buf, &new_pwd_len, 255, nk_filter_default);
                        if (old_len < new_pwd_len) {
                            memcpy(&new_pwd_buffer[old_len], &temp_buf[old_len], new_pwd_len - old_len);
                        }
                        new_pwd_buffer[new_pwd_len] = '\0';
                    }
                    if (res & NK_EDIT_ACTIVE) {
                        if (!keyboard_active) cp_focus_idx = 1;
                    }
                    if (res & NK_EDIT_COMMITED) is_enter_pressed = 1;
                }
                nk_layout_row_end(ctx);
                
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                
                // Confirm Password
                nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                nk_label(ctx, current_trans->label_confirm_pwd, NK_TEXT_LEFT);
                nk_layout_row_end(ctx);
                
                nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 300);
                {
                    nk_flags res;
                    if (keyboard_active && cp_focus_idx == 2) {
                        nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
                    }
                    if (show_passwords) {
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, confirm_pwd_buffer, &confirm_pwd_len, 255, nk_filter_default);
                        confirm_pwd_buffer[confirm_pwd_len] = '\0';
                    } else {
                        int old_len = confirm_pwd_len;
                        char temp_buf[256];
                        for (int i = 0; i < confirm_pwd_len; ++i) temp_buf[i] = '*';
                        temp_buf[confirm_pwd_len] = '\0';
                        res = nk_edit_string(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, temp_buf, &confirm_pwd_len, 255, nk_filter_default);
                        if (old_len < confirm_pwd_len) {
                            memcpy(&confirm_pwd_buffer[old_len], &temp_buf[old_len], confirm_pwd_len - old_len);
                        }
                        confirm_pwd_buffer[confirm_pwd_len] = '\0';
                    }
                    if (res & NK_EDIT_ACTIVE) {
                        if (!keyboard_active) cp_focus_idx = 2;
                    }
                    if (res & NK_EDIT_COMMITED) is_enter_pressed = 1;
                }
                nk_layout_row_end(ctx);
                
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                
                /* Checkbox manuelle : bouton invisible + dessin de l'icône */
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;
                ctx->style.button.padding = nk_vec2(0, 0);
                
                nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
                nk_layout_row_push(ctx, 105);
                nk_spacing(ctx, 1);
                nk_layout_row_push(ctx, 20);
                struct nk_rect cb_bounds = nk_widget_bounds(ctx);
                if (nk_button_label(ctx, "")) {
                    show_passwords = !show_passwords;
                    cp_focus_idx = 3;
                }
                struct nk_image *check_img = show_passwords ? &nk_img_check2 : &nk_img_check1;
                nk_draw_image(canvas, nk_rect(cb_bounds.x, cb_bounds.y, 20, 20), check_img, nk_white);
                if (keyboard_active && cp_focus_idx == 3) {
                    nk_stroke_rect(canvas, nk_rect(cb_bounds.x - 2, cb_bounds.y - 2, 24, 24), 0.0f, 1.5f, nk_rgb(255, 255, 255));
                }
                
                nk_layout_row_push(ctx, 280);
                struct nk_color cb_txt_color = (keyboard_active && cp_focus_idx == 3) ? nk_rgb(255, 255, 255) : nk_rgb(200, 200, 200);
                nk_label_colored(ctx, current_trans->label_show_passwords, NK_TEXT_LEFT, cb_txt_color);
                nk_layout_row_end(ctx);
                
                ctx->style.button.padding = old_btn_pad;
            }
            nk_end(ctx);
            ctx->style.window.padding = cp_pad;
            nk_style_pop_font(ctx);
            
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "ChangePasswordClose", nk_rect(win_x, win_y + main_h, win_w, 85), cp_flags)) {
                nk_layout_row_dynamic(ctx, 45, 2);
                
                struct nk_style_item old_btn_item = ctx->style.button.normal;
                
                if (keyboard_active && cp_focus_idx == 4) {
                    ctx->style.button.normal = nk_style_item_color(hover_color);
                }
                if (nk_button_label(ctx, current_trans->btn_apply) || is_enter_pressed || cp_enter_pressed) {
                    cp_enter_pressed = 0;
                    if (new_pwd_len == 0) {
                        safe_copy(pwd_msg_buffer, current_trans->err_pwd_empty, sizeof(pwd_msg_buffer));
                    } else if (strcmp(new_pwd_buffer, confirm_pwd_buffer) != 0) {
                        safe_copy(pwd_msg_buffer, current_trans->err_pwd_mismatch, sizeof(pwd_msg_buffer));
                    } else {
                        safe_copy(pwd_msg_buffer, current_trans->msg_changing_pwd, sizeof(pwd_msg_buffer));
                        change_own_password(old_pwd_buffer, new_pwd_buffer, pwd_msg_buffer, sizeof(pwd_msg_buffer));
                    }
                }
                ctx->style.button.normal = old_btn_item;
                
                if (keyboard_active && cp_focus_idx == 5) {
                    ctx->style.button.normal = nk_style_item_color(hover_color);
                }
                if (nk_button_label(ctx, current_trans->btn_close)) {
                    show_change_password = 0;
                }
                ctx->style.button.normal = old_btn_item;
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);
            
            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;
        }

        /* Fenêtre Indépendante pour le basculement d'utilisateur (Switch user) */
        if (show_switch_user && snap_show_switch_user && !show_password_prompt) {
            int win_w = THEME_SU_W(drm.width);
            int win_h = THEME_SU_H(drm.height);
            int win_x = THEME_SU_X(drm.width, win_w);
            int win_y = THEME_SU_Y(drm.height, win_h);
            int main_h = win_h - 85;
            
            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);
            ctx->style.window.padding = nk_vec2(20, 20);
            
            // --- SWITCH USER TITLE BAR ---
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            struct nk_style_item old_title_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_TITLE_BG);
            struct nk_vec2 old_title_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(10, 6);
            nk_flags su_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (nk_begin(ctx, "SwitchUserTitle", nk_rect(win_x, win_y - 36, win_w, 36), su_flags)) {
                nk_layout_row_begin(ctx, NK_STATIC, 24, 3);
                
                // Left Icon (Console icon is relevant to session switcher)
                nk_layout_row_push(ctx, 24);
                struct nk_rect icon_bounds = nk_widget_bounds(ctx);
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                nk_draw_image(canvas, nk_rect(icon_bounds.x, icon_bounds.y, 24, 24), &nk_img_console, nk_white);
                nk_spacing(ctx, 1);
                
                // Title
                nk_layout_row_push(ctx, win_w - 76);
                nk_label(ctx, current_trans->title_switch_user, NK_TEXT_LEFT);
                
                // Close button
                nk_layout_row_push(ctx, 24);
                struct nk_rect btn_bounds = nk_widget_bounds(ctx);
                struct nk_style_item old_btn = ctx->style.button.normal;
                struct nk_style_item old_btn_hover = ctx->style.button.hover;
                struct nk_style_item old_btn_active = ctx->style.button.active;
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                ctx->style.button.hover = nk_style_item_color(nk_rgb(200, 50, 50));
                ctx->style.button.active = nk_style_item_color(nk_rgb(150, 30, 30));
                
                if (nk_button_label(ctx, "")) {
                    show_switch_user = 0;
                }
                
                ctx->style.button.normal = old_btn;
                ctx->style.button.hover = old_btn_hover;
                ctx->style.button.active = old_btn_active;
                
                nk_draw_image(canvas, nk_rect(btn_bounds.x, btn_bounds.y, 24, 24), &nk_img_close, nk_white);
                
                nk_layout_row_end(ctx);
            }
            nk_end(ctx);
            ctx->style.window.fixed_background = old_title_bg;
            ctx->style.window.padding = old_title_pad;
            nk_style_pop_font(ctx);

            if (keyboard_active) {
                nk_window_set_focus(ctx, "SwitchUser");
            }
            
            if (nk_begin(ctx, "SwitchUser", nk_rect(win_x, win_y, win_w, main_h), su_flags)) {
                nk_style_push_font(ctx, &rawfb->font_small->handle);
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, current_trans->label_active_sessions, NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
                nk_style_pop_font(ctx);
                
                // Liste scrollable des sessions
                nk_layout_row_dynamic(ctx, 180, 1);
                if (nk_group_begin(ctx, "SessionList", NK_WINDOW_BORDER)) {
                    for (int i = 0; i < num_sessions; i++) {
                        // Focus visuel clavier
                        int is_selected = (keyboard_active && su_focus_idx == 0 && su_selected_idx == i);
                        struct nk_style_item old_btn_normal = ctx->style.button.normal;
                        if (is_selected) {
                            ctx->style.button.normal = nk_style_item_color(hover_color);
                        } else {
                            ctx->style.button.normal = nk_style_item_color(nk_rgba(0,0,0,0));
                        }
                        
                        nk_layout_row_dynamic(ctx, 48, 1);
                        struct nk_rect b = nk_widget_bounds(ctx);
                        if (nk_button_label(ctx, "") || (keyboard_active && su_focus_idx == 0 && su_selected_idx == i && cp_enter_pressed)) {
                            // Action : Basculer vers cette session !
                            journal.original_vt = sessions[i].vt;
                            running = 0;
                            show_switch_user = 0;
                        }
                        ctx->style.button.normal = old_btn_normal;
                        
                        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                        
                        // Première ligne : Username (grande police par défaut de la fenêtre)
                        const struct nk_user_font *font_u = ctx->style.font;
                        struct nk_color name_color = nk_rgb(255, 255, 255);
                        nk_draw_text(canvas, nk_rect(b.x + 10, b.y + 2, b.w - 20, 30), sessions[i].username, nk_strlen(sessions[i].username), font_u, nk_rgba(0,0,0,0), name_color);
                        
                        // Deuxième ligne : VT et détails techniques (font_mono)
                        char tech_str[128];
                        safe_copy(tech_str, "VT ", sizeof(tech_str));
                        char vt_num[16]; int_to_str(sessions[i].vt, vt_num); safe_concat(tech_str, vt_num, sizeof(tech_str));
                        safe_concat(tech_str, " | ", sizeof(tech_str));
                        safe_concat(tech_str, sessions[i].proc_name, sizeof(tech_str));
                        
                        if (sessions[i].vt == journal.original_vt) {
                            safe_concat(tech_str, " ", sizeof(tech_str));
                            safe_concat(tech_str, current_trans->label_parent_vt, sizeof(tech_str));
                        }
                        if (sessions[i].is_dm) {
                            safe_concat(tech_str, " ", sizeof(tech_str));
                            safe_concat(tech_str, current_trans->label_login_screen, sizeof(tech_str));
                        } else if (sessions[i].is_graphical) {
                            safe_concat(tech_str, " ", sizeof(tech_str));
                            safe_concat(tech_str, current_trans->label_graphical, sizeof(tech_str));
                        } else {
                            safe_concat(tech_str, " ", sizeof(tech_str));
                            safe_concat(tech_str, current_trans->label_console, sizeof(tech_str));
                        }
                        
                        const struct nk_user_font *font_t = &rawfb->font_mono->handle;
                        struct nk_color tech_color = nk_rgb(180, 200, 220);
                        nk_draw_text(canvas, nk_rect(b.x + 10, b.y + 32, b.w - 20, 16), tech_str, nk_strlen(tech_str), font_t, nk_rgba(0,0,0,0), tech_color);
                    }
                    nk_group_end(ctx);
                }
                
                nk_layout_row_dynamic(ctx, 10, 1);
                nk_spacing(ctx, 1);
                
                // Bouton "New Session" (pousser font_small)
                nk_style_push_font(ctx, &rawfb->font_small->handle);
                nk_layout_row_dynamic(ctx, 35, 1);
                struct nk_style_item old_btn_normal = ctx->style.button.normal;
                if (keyboard_active && su_focus_idx == 1) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_start_new_session) || (keyboard_active && su_focus_idx == 1 && cp_enter_pressed)) {
                    // Fork pour lancer le greeter du DM et quitter CAD
                    pid_t pid = fork();
                    if (pid == 0) {
                        setsid();
                        if (fork() == 0) {
                            sleep(1);
                            execute_as_user("(dbus-send --system --print-reply --dest=org.freedesktop.DisplayManager /org/freedesktop/DisplayManager/Seat0 org.freedesktop.DisplayManager.Seat.SwitchToGreeter || dm-tool switch-to-greeter || gdmflexiserver || lxdm -c USER_SWITCH || tdmctl reserve || kdmctl reserve || sddm-greeter) >/dev/null 2>&1");
                            _exit(0);
                        }
                        _exit(0);
                    }
                    running = 0;
                    show_switch_user = 0;
                }
                ctx->style.button.normal = old_btn_normal;
                nk_style_pop_font(ctx);
            }
            nk_end(ctx);
            
            // Fenêtre inférieure "SwitchUserClose" pour le bouton Close
            nk_style_push_font(ctx, &rawfb->font_small->handle);
            if (nk_begin(ctx, "SwitchUserClose", nk_rect(win_x, win_y + main_h, win_w, 85), su_flags)) {
                nk_layout_row_dynamic(ctx, 45, 1);
                
                struct nk_style_item old_btn_normal = ctx->style.button.normal;
                if (keyboard_active && su_focus_idx == 2) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, current_trans->btn_close) || (keyboard_active && su_focus_idx == 2 && cp_enter_pressed)) {
                    show_switch_user = 0;
                }
                ctx->style.button.normal = old_btn_normal;
            }
            nk_end(ctx);
            nk_style_pop_font(ctx);

            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;
        }

        if (show_power_menu && !show_password_prompt) {
            int pm_w = pm_menu_w;
            int pm_h = 180;
            int pm_x = drm.width - pm_w - 20; // Align right with shutdown button
            int pm_y = drm.height - SHUTDOWN_H - 20 - pm_h; // Bottom edge touches shutdown button top

            struct nk_style_item old_bg = ctx->style.window.fixed_background;
            ctx->style.window.fixed_background = nk_style_item_color(THEME_COLOR_BG);

            struct nk_vec2 old_pad = ctx->style.window.padding;
            ctx->style.window.padding = nk_vec2(20, 20);

            if (keyboard_active) {
                nk_window_set_focus(ctx, "PowerOptions");
            }

            nk_flags pm_menu_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER;
            if (show_password_prompt) {
                pm_menu_flags |= NK_WINDOW_NO_INPUT;
            }
            if (nk_begin(ctx, "PowerOptions", nk_rect(pm_x, pm_y, pm_w, pm_h), pm_menu_flags)) {
                struct nk_vec2 old_btn_pad = ctx->style.button.padding;
                ctx->style.button.padding = nk_vec2(4, 5); // Keep y=5 to maintain 32px icon height (42 - 2*5 = 32), reduce x padding
                
                struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                struct nk_rect b;
                const struct nk_user_font *font = ctx->style.font;
                
                nk_layout_row_dynamic(ctx, 42, 1);
                
                b = nk_widget_bounds(ctx);
                if (keyboard_active && pm_selected_idx == 0) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, "")) {
                    show_power_menu = 0;
                    execute_sleep(drm.fb_id[front_buf]);
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                nk_draw_image(canvas, nk_rect(b.x + 10, b.y + 5, 32, 32), &nk_img_sleep, nk_white);
                nk_draw_text(canvas, nk_rect(b.x + 52, b.y + 6, b.w - 52, 32), current_trans->btn_sleep, strlen(current_trans->btn_sleep), font, nk_rgba(0,0,0,0), nk_white);

                b = nk_widget_bounds(ctx);
                if (keyboard_active && pm_selected_idx == 1) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, "")) {
                    show_power_menu = 0;
                    pid_t pid = fork();
                    if (pid == 0) {
                        setsid();
                        if (fork() == 0) {
                            execute_restart();
                            _exit(0);
                        }
                        _exit(0);
                    }
                    running = 0;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                nk_draw_image(canvas, nk_rect(b.x + 10, b.y + 5, 32, 32), &nk_img_restart, nk_white);
                nk_draw_text(canvas, nk_rect(b.x + 52, b.y + 6, b.w - 52, 32), current_trans->btn_restart, strlen(current_trans->btn_restart), font, nk_rgba(0,0,0,0), nk_white);

                b = nk_widget_bounds(ctx);
                if (keyboard_active && pm_selected_idx == 2) ctx->style.button.normal = nk_style_item_color(hover_color);
                if (nk_button_label(ctx, "")) {
                    show_power_menu = 0;
                    pid_t pid = fork();
                    if (pid == 0) {
                        setsid();
                        if (fork() == 0) {
                            execute_shutdown();
                            _exit(0);
                        }
                        _exit(0);
                    }
                    running = 0;
                }
                ctx->style.button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
                nk_draw_image(canvas, nk_rect(b.x + 10, b.y + 5, 32, 32), &nk_img_power, nk_white);
                nk_draw_text(canvas, nk_rect(b.x + 52, b.y + 6, b.w - 52, 32), current_trans->btn_shutdown, strlen(current_trans->btn_shutdown), font, nk_rgba(0,0,0,0), nk_white);
                
                ctx->style.button.padding = old_btn_pad;
            }
            nk_end(ctx);

            ctx->style.window.fixed_background = old_bg;
            ctx->style.window.padding = old_pad;
        }

        /* Traitement des actions hors Nuklear */
        if (trigger_action == 7) {
            show_terminal = !show_terminal;
            show_task_manager = 0;
            show_change_password = 0;
            show_switch_user = 0;
            show_power_menu = 0;
            DEBUG_LOG("[GUI] Clic: Terminal Button");
            
            if (show_terminal && term_pty == NULL && term_screen && term_vte) {
                // Relancer un shell si le précédent a été fermé
                tsm_screen_clear_sb(term_screen);
                tsm_screen_erase_screen(term_screen, false);
                tsm_screen_move_to(term_screen, 0, 0);
                tsm_screen_reset(term_screen);
                tsm_vte_reset(term_vte);
                pid_t pid = launch_terminal_shell(terminal_mode);
                if (pid < 0) {
                    show_terminal = 0;
                }
            }
        }
        if (trigger_action == 8) {
            show_power_menu = !show_power_menu;
            if (show_power_menu) {
                pm_selected_idx = 0;
            }
            show_task_manager = 0;
            show_terminal = 0;
            show_run = 0;
            show_change_password = 0;
            show_switch_user = 0;
        }

        /* Anti-clic-fantôme : détecter les transitions de dialogue et armer le cooldown */
        if ((!snap_show_switch_user && show_switch_user) ||
            (!snap_show_change_password && show_change_password) ||
            (!snap_show_run && show_run) ||
            (!snap_show_task_manager && show_task_manager) ||
            (!snap_show_terminal && show_terminal) ||
            (!snap_show_power_menu && show_power_menu) ||
            (!snap_show_password_prompt && show_password_prompt)) {
            click_cooldown = 3; /* Supprimer les clics pendant 3 frames après ouverture */
            mouse_acc_len = 0;  /* Vider le buffer PS/2 pour éviter les octets résiduels */
        }

        /* 4. Rendu de l'UI dans le Dumb Buffer arrière (avec un fond bleu foncé) */
        nk_rawfb_render(rawfb, nk_rgb(32, 80, 129), 1);
        
        /* 4.5 Dessin des boutons manuels en bas à droite */
        if (!is_modal) {
            int terminal_hovered = (!keyboard_active && 
                                    mouse_x >= btn_terminal_x && mouse_x <= btn_terminal_x + TERMINAL_W &&
                                    mouse_y >= btn_terminal_y && mouse_y <= btn_terminal_y + TERMINAL_H);
            if (show_terminal || (keyboard_active && selected_idx == 7)) {
                terminal_hovered = 1;
            }
            
            int shutdown_hovered = (!keyboard_active && 
                                    mouse_x >= btn_shutdown_x && mouse_x <= btn_shutdown_x + SHUTDOWN_W &&
                                    mouse_y >= btn_shutdown_y && mouse_y <= btn_shutdown_y + SHUTDOWN_H);
            if (show_power_menu || (keyboard_active && selected_idx == 8)) {
                shutdown_hovered = 1;
            }
            
            // Fond légèrement assombri au survol (même couleur que les boutons principaux : 66, 107, 148)
            if (terminal_hovered) {
                draw_rect(drm.buffer[back_buf], drm.width, drm.height, drm.pitch,
                          btn_terminal_x, btn_terminal_y, TERMINAL_W, TERMINAL_H, THEME_COLOR_HOVER_HEX);
            }
            draw_image(drm.buffer[back_buf], drm.width, drm.height, drm.pitch, 
                       btn_terminal_x, btn_terminal_y, TERMINAL_W, TERMINAL_H, terminal_data);

            if (shutdown_hovered) {
                draw_rect(drm.buffer[back_buf], drm.width, drm.height, drm.pitch,
                          btn_shutdown_x, btn_shutdown_y, SHUTDOWN_W, SHUTDOWN_H, THEME_COLOR_HOVER_HEX);
            }
            draw_image(drm.buffer[back_buf], drm.width, drm.height, drm.pitch, 
                       btn_shutdown_x, btn_shutdown_y, SHUTDOWN_W, SHUTDOWN_H, shutdown_data);
        }

        /* 5. Dessin du curseur Xcursor avec alpha blending par-dessus (seulement si actif) */
        if (!keyboard_active) {
            draw_cursor(drm.buffer[back_buf], drm.width, drm.height, drm.pitch, mouse_x, mouse_y);
        }

        /* 6. Affichage du buffer préparé (SETCRTC la première fois, puis PAGE_FLIP ou DIRTYFB) */
        if (drm.double_buffered) {
            if (frame_count == 1) {
                struct drm_mode_crtc crtc = {0};
                crtc.crtc_id = drm.crtc_id;
                crtc.fb_id = drm.fb_id[back_buf];
                crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&drm.conn_id;
                crtc.count_connectors = 1;
                crtc.mode = drm.mode;
                crtc.mode_valid = 1;
                drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
            } else {
                struct drm_mode_crtc_page_flip flip = {0};
                flip.fb_id = drm.fb_id[back_buf];
                flip.crtc_id = drm.crtc_id;
                flip.flags = 0;
                if (drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) < 0) {
                    DEBUG_LOG("[WARNING] DRM_IOCTL_MODE_PAGE_FLIP failed. Falling back to single buffering.");
                    drm.double_buffered = 0;
                    // Forcer la synchronisation avec le premier buffer
                    struct drm_mode_crtc crtc = {0};
                    crtc.crtc_id = drm.crtc_id;
                    crtc.fb_id = drm.fb_id[0];
                    crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&drm.conn_id;
                    crtc.count_connectors = 1;
                    crtc.mode = drm.mode;
                    crtc.mode_valid = 1;
                    drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
                }
            }
        }
        
        if (!drm.double_buffered) {
            struct drm_mode_fb_dirty_cmd dirty = {0};
            dirty.fb_id = drm.fb_id[0];
            dirty.num_clips = 0;
            drm_ioctl(journal.drm_fd, DRM_IOCTL_MODE_DIRTYFB, &dirty);
        }
        
        if (prev_show_terminal && !show_terminal) {
            cleanup_terminal_session();
        }
        
        front_buf = back_buf;
        
        /* Limitation à environ 60 FPS pour soulager le CPU de manière sûre vis à vis de SCHED_FIFO */
        struct timespec ts = {0, 16000000};
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
    }

    /* --- Nettoyage terminal --- */
    close_current_pty();
    if (term_vte) {
        tsm_vte_unref(term_vte);
        term_vte = NULL;
    }
    if (term_screen) {
        tsm_screen_unref(term_screen);
        term_screen = NULL;
    }

    if (busybox_memfd >= 0) {
        close(busybox_memfd);
        busybox_memfd = -1;
    }

    if (journal.mouse_fd >= 0) close(journal.mouse_fd);
    if (journal.kbd_fd >= 0) close(journal.kbd_fd);
    nk_rawfb_shutdown(rawfb);
    
    DEBUG_LOG("Nettoyage et restauration de l'affichage normal.");
    cleanup_drm();
    
    if (run_command_ready && user_session.run_cmd[0] != '\0') {
        DEBUG_LOG("Exécution asynchrone post-GUI de la commande demandée.");
        if (fork() == 0) {
            if (fork() == 0) {
                sleep(1); /* Laisser le temps à DRM de se fermer et à VT de switcher */
                if (run_as_root) {
                    execute_command_safe(user_session.run_cmd, 1);
                } else {
                    execute_as_user(user_session.run_cmd);
                }
                _exit(0);
            }
            _exit(0);
        }
        _exit(0);
    }
    
    current_state = STATE_INIT;
    return 0;
}


