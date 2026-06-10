#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <termios.h>
#include <grp.h>

#ifdef USE_ZX0
#include "decompressors/zx0/zx0_decompress.h"
#else
#include "decompressors/lz4/codec_lz4.h"
#endif
#include "gui_payload.h"

#ifndef ENABLE_DEBUG
#define printf(...) ((void)0)
#define fprintf(...) ((void)0)
#define perror(...) ((void)0)
#endif

static int gui_fd = -1;
static char fallback_path[256] = {0};

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define test_bit(bit, array) ((array[(bit)/BITS_PER_LONG] >> ((bit)%BITS_PER_LONG)) & 1)

static char detected_lang[3] = "en";

static void detect_system_language(char *out_lang, size_t max_len) {
    if (!out_lang || max_len < 3) return;
    out_lang[0] = 'e';
    out_lang[1] = 'n';
    out_lang[2] = '\0';

    // 1. Check environment variable LANG
    const char *env_lang = getenv("LANG");
    if (env_lang && strlen(env_lang) >= 2) {
        out_lang[0] = env_lang[0];
        out_lang[1] = env_lang[1];
        // Convert to lowercase
        if (out_lang[0] >= 'A' && out_lang[0] <= 'Z') out_lang[0] += 32;
        if (out_lang[1] >= 'A' && out_lang[1] <= 'Z') out_lang[1] += 32;
        return;
    }

    // 2. Check /etc/locale.conf
    int fd = open("/etc/locale.conf", O_RDONLY);
    if (fd >= 0) {
        char buf[256];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = buf;
            while (line < buf + n) {
                // Find end of line
                char *next_line = strchr(line, '\n');
                if (next_line) {
                    *next_line = '\0';
                    next_line++;
                } else {
                    next_line = buf + n;
                }

                // Trim leading whitespace
                while (*line == ' ' || *line == '\t') line++;

                if (strncmp(line, "LANG=", 5) == 0) {
                    char *val = line + 5;
                    // Handle potential quotes
                    if (*val == '"' || *val == '\'') val++;
                    if (strlen(val) >= 2) {
                        out_lang[0] = val[0];
                        out_lang[1] = val[1];
                        // Convert to lowercase
                        if (out_lang[0] >= 'A' && out_lang[0] <= 'Z') out_lang[0] += 32;
                        if (out_lang[1] >= 'A' && out_lang[1] <= 'Z') out_lang[1] += 32;
                        return;
                    }
                }
                line = next_line;
            }
        }
    }
}

static void safe_copy(char *dst, const char *src, size_t max_len) {
    if (!dst || max_len == 0) return;
    size_t i = 0;
    while (src && src[i] && i < max_len - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/*
 * Recherche automatique du périphérique clavier.
 * Parcourt /dev/input/event* et utilise les ioctls pour vérifier
 * si le périphérique possède les capacités d'un clavier et les touches nécessaires.
 */
int find_keyboard(char *out_path, size_t max_len) {
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        perror("Impossible d'ouvrir /dev/input");
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) == 0) {
            char path[512];
            snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                unsigned long evbit[NBITS(EV_MAX)] = {0};
                unsigned long keybit[NBITS(KEY_MAX)] = {0};
                
                // On récupère les bits de capacités (EV) et les bits de touches (KEY)
                if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) >= 0 &&
                    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) {
                    
                    // Un clavier doit supporter EV_KEY et posséder CTRL, ALT et DELETE
                    if (test_bit(EV_KEY, evbit) && 
                        test_bit(KEY_LEFTCTRL, keybit) && 
                        test_bit(KEY_LEFTALT, keybit) && 
                        test_bit(KEY_DELETE, keybit)) {
                        
                        char name[256] = "Inconnu";
                        if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0) {
                            name[sizeof(name) - 1] = '\0';
                        }
                        printf("Clavier détecté : %s (%s)\n", path, name);
                        
                        safe_copy(out_path, path, max_len);
                        close(fd);
                        closedir(dir);
                        return 0; // Succès
                    }
                }
                close(fd);
            }
        }
    }
    
    closedir(dir);
    return -1; // Aucun clavier trouvé
}

static int is_alphanumeric(const char *s) {
    if (!s || *s == '\0') return 0;
    while (*s) {
        if (!((*s >= 'a' && *s <= 'z') || 
              (*s >= 'A' && *s <= 'Z') || 
              (*s >= '0' && *s <= '9'))) {
            return 0;
        }
        s++;
    }
    return 1;
}

static int run_cmd_capture(uid_t run_uid, gid_t run_gid, const char *username, const char *path, char *const argv[], const char *display, char *out_buf, size_t out_len) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;
    
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        
        if (run_uid != 0) {
            if (username) {
                initgroups(username, run_gid);
            }
            setgid(run_gid);
            setuid(run_uid);
        }
        
        char display_var[128];
        char home_var[256] = "HOME=/";
        char user_var[128] = "USER=nobody";
        char path_var[] = "PATH=/usr/bin:/bin:/usr/sbin:/sbin:/opt/trinity/bin";
        
        snprintf(display_var, sizeof(display_var), "DISPLAY=%s", display ? display : ":0");
        struct passwd *pw = getpwuid(run_uid);
        if (pw) {
            snprintf(home_var, sizeof(home_var), "HOME=%s", pw->pw_dir);
            snprintf(user_var, sizeof(user_var), "USER=%s", pw->pw_name);
        }
        
        char *envp[] = {
            display_var,
            home_var,
            user_var,
            path_var,
            NULL
        };
        
        execve(path, argv, envp);
        _exit(127);
    }
    
    // Parent
    close(pipefd[1]);
    size_t total = 0;
    while (total < out_len - 1) {
        ssize_t n = read(pipefd[0], out_buf + total, out_len - 1 - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        total += n;
    }
    out_buf[total] = '\0';
    close(pipefd[0]);
    
    int status = -1;
    int wp_ret;
    while ((wp_ret = waitpid(pid, &status, 0)) < 0) {
        if (errno != EINTR) break;
    }
    if (wp_ret <= 0) return -1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

static int run_cmd_search_capture(const char *file, char *const argv[], char *out_buf, size_t out_len) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;
    
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        
        execvp(file, argv);
        _exit(127);
    }
    
    // Parent
    close(pipefd[1]);
    size_t total = 0;
    while (total < out_len - 1) {
        ssize_t n = read(pipefd[0], out_buf + total, out_len - 1 - total);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        total += n;
    }
    out_buf[total] = '\0';
    close(pipefd[0]);
    
    int status = -1;
    int wp_ret;
    while ((wp_ret = waitpid(pid, &status, 0)) < 0) {
        if (errno != EINTR) break;
    }
    if (wp_ret <= 0) return -1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

/*
 * Détection universelle de l'état de verrouillage de session.
 * Gère les verrous X11 et Wayland courants, Trinity (DCOP), et les sessions systemd (loginctl).
 */
int is_screen_locked() {
    int locked = 0;
    
    // 1. Détection des processus de verrouillage transitoires (pas de faux positifs)
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
            int fd = open(path, O_RDONLY);
            if (fd >= 0) {
                char comm[256] = {0};
                int n = read(fd, comm, sizeof(comm) - 1);
                if (n > 0) {
                    if (comm[n - 1] == '\n') comm[n - 1] = '\0';
                    
                    if (strstr(comm, "swaylock") || 
                        strstr(comm, "i3lock") || 
                        strstr(comm, "slock") || 
                        strstr(comm, "hyprlock") ||
                        strstr(comm, "xlock") ||
                        strstr(comm, "xsecurelock") || 
                        strstr(comm, "gtklock") ||
                        strstr(comm, "lxlock") ||
                        strstr(comm, "kscreenlocker_greet") ||
                        strstr(comm, "gnome-screensaver-dialog") ||
                        strstr(comm, "mate-screensaver-dialog") ||
                        strstr(comm, "xfce4-screensaver-dialog") ||
                        strstr(comm, "cinnamon-screensaver-dialog") ||
                        strstr(comm, "xscreensaver-auth")) {
                        locked = 1;
                    }
                }
                close(fd);
            }
            if (locked) break;
        }
    }
    closedir(dir);
    if (locked) return 1;

    // 2. Détection pour Trinity (TDE) via dcop
    int has_kdesktop_lock = 0;
    uid_t tde_uid = 0;
    pid_t tde_pid = 0;
    
    dir = opendir("/proc");
    if (dir) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] >= '1' && ent->d_name[0] <= '9') {
                char path[512];
                snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
                int fd = open(path, O_RDONLY);
                if (fd >= 0) {
                    char comm[256] = {0};
                    int n = read(fd, comm, sizeof(comm) - 1);
                    if (n > 0) {
                        if (comm[n - 1] == '\n') comm[n - 1] = '\0';
                        if (strcmp(comm, "kdesktop_lock") == 0) {
                            has_kdesktop_lock = 1;
                            tde_pid = (pid_t)strtol(ent->d_name, NULL, 10);
                            struct stat st;
                            snprintf(path, sizeof(path), "/proc/%s", ent->d_name);
                            if (stat(path, &st) == 0) {
                                tde_uid = st.st_uid;
                            }
                        }
                    }
                    close(fd);
                }
                if (has_kdesktop_lock) break;
            }
        }
        closedir(dir);
    }

    if (has_kdesktop_lock && tde_uid > 0) {
        struct passwd *pw = getpwuid(tde_uid);
        if (pw) {
            char cmd[512];
            char display_env[64] = ":0";
            snprintf(cmd, sizeof(cmd), "/proc/%d/environ", tde_pid);
            int env_fd = open(cmd, O_RDONLY);
            if (env_fd >= 0) {
                char env_buf[4096];
                int len = read(env_fd, env_buf, sizeof(env_buf) - 1);
                if (len > 0) {
                    env_buf[len] = '\0';
                    char *p = env_buf;
                    while (p < env_buf + len) {
                        char *end = memchr(p, '\0', env_buf + len - p);
                        if (!end) break;
                        if (strncmp(p, "DISPLAY=", 8) == 0) {
                            safe_copy(display_env, p + 8, sizeof(display_env));
                            break;
                        }
                        p = end + 1;
                    }
                }
                close(env_fd);
            }

            char dcop_out[64] = {0};
            char *dcop_argv[] = {"/opt/trinity/bin/dcop", "kdesktop", "KScreensaverIface", "isEnabled", NULL};
            if (run_cmd_capture(pw->pw_uid, pw->pw_gid, pw->pw_name, "/opt/trinity/bin/dcop", dcop_argv, display_env, dcop_out, sizeof(dcop_out)) == 0) {
                if (strstr(dcop_out, "true")) {
                    locked = 1;
                }
            }
        }
        if (locked) return 1;
    }

    // 3. Détection universelle systemd via loginctl (LockedHint)
    char sessions_buf[4096] = {0};
    char *list_sess_argv[] = {"loginctl", "list-sessions", "--no-legend", NULL};
    if (run_cmd_search_capture("loginctl", list_sess_argv, sessions_buf, sizeof(sessions_buf)) == 0) {
        char *line = sessions_buf;
        while (line && *line) {
            char *next_line = strchr(line, '\n');
            if (next_line) {
                *next_line = '\0';
                next_line++;
            }
            
            char sess_id[64] = {0};
            if (sscanf(line, "%63s", sess_id) == 1) {
                if (is_alphanumeric(sess_id)) {
                    char show_buf[256] = {0};
                    char *show_sess_argv[] = {"loginctl", "show-session", sess_id, "-p", "LockedHint", NULL};
                    if (run_cmd_search_capture("loginctl", show_sess_argv, show_buf, sizeof(show_buf)) == 0) {
                        if (strstr(show_buf, "LockedHint=yes")) {
                            locked = 1;
                        }
                    }
                }
            }
            if (locked) break;
            line = next_line;
        }
    }

    return locked;
}

/*
 * Sécurisation du Watchdog (Hardening).
 * Empêche le démon d'être tué en cas de saturation de RAM ou de CPU.
 */
void harden_daemon() {
    /* 1. Mlockall (Anti-Swap) */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        fprintf(stderr, "[WARNING] Échec de mlockall (le Watchdog pourrait être swappé).\n");
    } else {
        printf("Mémoire verrouillée en RAM (mlockall).\n");
    }
    
    /* 2. Immunité absolue contre le OOM Killer (-1000) */
    int oom_fd = open("/proc/self/oom_score_adj", O_WRONLY);
    if (oom_fd >= 0) {
        if (write(oom_fd, "-1000", 5) <= 0) {
            fprintf(stderr, "[WARNING] Échec de l'écriture dans oom_score_adj.\n");
        } else {
            printf("Immunité OOM Killer (-1000) acquise.\n");
        }
        close(oom_fd);
    } else {
        fprintf(stderr, "[WARNING] Impossible d'ouvrir oom_score_adj.\n");
    }
    
    /* 3. Élévation de priorité maximale (Temps Réel) */
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        fprintf(stderr, "Avertissement : Échec SCHED_FIFO, fallback setpriority(-20).\n");
        if (setpriority(PRIO_PROCESS, 0, -20) < 0) {
            fprintf(stderr, "[WARNING] Échec de setpriority.\n");
        }
    } else {
        printf("Priorité temps réel (SCHED_FIFO) acquise.\n");
    }
}

static void cleanup_on_exit(void) {
    if (fallback_path[0]) {
        unlink(fallback_path);
    }
}

static volatile sig_atomic_t terminate_requested = 0;

static void signal_handler(int sig) {
    (void)sig;
    terminate_requested = 1;
}

static int prepare_gui_memfd(void) {
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif
#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

    int fd = memfd_create("lnxcad_rescue_gui", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    int is_memfd = 1;

    if (fd < 0) {
        fprintf(stderr, "[WARNING] memfd_create failed: %s. Falling back to temporary file.\n", strerror(errno));
        is_memfd = 0;

        snprintf(fallback_path, sizeof(fallback_path), "/dev/shm/lnxcad_rescue_gui.%d", getpid());
        fd = open(fallback_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0700);
        if (fd < 0) {
            snprintf(fallback_path, sizeof(fallback_path), "/tmp/lnxcad_rescue_gui.%d", getpid());
            fd = open(fallback_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0700);
        }
        if (fd < 0) {
            perror("Failed to create fallback file");
            return -1;
        }
    }

    unsigned char *dec_buf = malloc(GUI_DECOMPRESSED_SIZE);
    if (!dec_buf) {
        perror("malloc failed for decompression buffer");
        close(fd);
        if (fallback_path[0]) unlink(fallback_path);
        return -1;
    }

    int dec_sz = 0;
#ifdef USE_ZX0
    printf("Decompressing GUI payload using ZX0...\n");
    dec_sz = zx0_decompress_to(gui_compressed_data, GUI_COMPRESSED_SIZE, dec_buf, GUI_DECOMPRESSED_SIZE);
#else
    printf("Decompressing GUI payload using LZ4...\n");
    dec_sz = lz4_decompress((const char *)gui_compressed_data, (char *)dec_buf, GUI_COMPRESSED_SIZE, GUI_DECOMPRESSED_SIZE);
#endif

    if (dec_sz != GUI_DECOMPRESSED_SIZE) {
        fprintf(stderr, "Decompression failed! Expected size: %d, got: %d\n", GUI_DECOMPRESSED_SIZE, dec_sz);
        free(dec_buf);
        close(fd);
        if (fallback_path[0]) unlink(fallback_path);
        return -1;
    }

    ssize_t written = 0;
    while (written < dec_sz) {
        ssize_t n = write(fd, dec_buf + written, dec_sz - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("write failed");
            free(dec_buf);
            close(fd);
            if (fallback_path[0]) unlink(fallback_path);
            return -1;
        }
        written += n;
    }

    free(dec_buf);

    if (is_memfd) {
#ifndef F_ADD_SEALS
#define F_ADD_SEALS 1033
#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#define F_SEAL_WRITE 0x0008
#endif
        if (fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) < 0) {
            perror("failed to seal memfd");
            close(fd);
            return -1;
        }
        printf("GUI binary successfully decompressed into a sealed memfd.\n");
    } else {
        printf("GUI binary successfully decompressed into temporary file %s.\n", fallback_path);
    }

    return fd;
}

/*
 * Fonction qui écoute les événements d'un périphérique d'entrée
 * et détecte la combinaison Ctrl + Alt + Suppr.
 */
int listen_for_cad(const char *device_path) {
    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Erreur lors de l'ouverture du périphérique %s : %s\n", device_path, strerror(errno));
        return -1;
    }

    struct input_event ev;
    int ctrl_pressed = 0;
    int alt_pressed = 0;
    int del_pressed = 0;

    printf("Écoute des événements sur %s (En attente de Ctrl+Alt+Suppr)...\n", device_path);

    while (!terminate_requested) {
        size_t bytes_read = 0;
        int err = 0;
        
        while (bytes_read < sizeof(struct input_event)) {
            ssize_t n = read(fd, (char *)&ev + bytes_read, sizeof(struct input_event) - bytes_read);
            if (n < 0) {
                if (errno == EINTR) {
                    if (terminate_requested) {
                        err = 1;
                        break;
                    }
                    continue;
                }
                err = 1;
                break;
            }
            if (n == 0) {
                err = 1;
                break;
            }
            bytes_read += n;
        }

        if (err) break;

        if (ev.type == EV_KEY) {
            int is_pressed = (ev.value == 1 || ev.value == 2);

            if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                ctrl_pressed = is_pressed;
            } else if (ev.code == KEY_LEFTALT) {
                alt_pressed = is_pressed;
            } else if (ev.code == KEY_DELETE) {
                del_pressed = is_pressed;
            }

            if (ctrl_pressed && alt_pressed && del_pressed) {
                printf("\n>>> CAD Détecté ! <<<\n");
                
                if (is_screen_locked()) {
                    printf("ERREUR : Écran de verrouillage détecté.\n");
                    printf("Lancement de cad_rescue_gui ignoré pour des raisons de sécurité (Anti-Bypass).\n\n");
                } else {
                    printf("Lancement de cad_rescue_gui depuis la RAM...\n");
                    pid_t pid = fork();
                    if (pid == 0) {
                        /* Dans le fils : Lancement de l'outil graphique */
                        close(fd);

                        static char lang_env[32];
                        snprintf(lang_env, sizeof(lang_env), "CAD_LANG=%s", detected_lang);
                        putenv(lang_env);

                        char *child_argv[] = { "lnxcad_rescue_gui", NULL };
                        extern char **environ;

                        if (fallback_path[0]) {
                            execve(fallback_path, child_argv, environ);
                        } else {
                            fexecve(gui_fd, child_argv, environ);

                            // Fallback si fexecve échoue (ex: noyau très ancien sans fexecve natif)
                            char fd_path[64];
                            snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", gui_fd);
                            fcntl(gui_fd, F_SETFD, 0); // enlever FD_CLOEXEC pour le fallback execve
                            execve(fd_path, child_argv, environ);
                        }

                        perror("Erreur d'exécution de cad_rescue_gui");
                        _exit(127);
                    } else if (pid > 0) {
                        /* Dans le père : Attente de la fermeture de la GUI */
                        while (waitpid(pid, NULL, 0) < 0) {
                            if (errno != EINTR) break;
                        }
                        if (isatty(0)) {
                            tcflush(0, TCIFLUSH);
                        }
                        printf("\ncad_rescue_gui terminé. Reprise de l'écoute...\n");
                    }
                }
                
                /* Debounce pour éviter des lancements multiples */
                for (int i = 0; i < 20 && !terminate_requested; i++) {
                    usleep(100000);
                }
                
                /* Reset des états */
                ctrl_pressed = alt_pressed = del_pressed = 0;
            }
        }
    }

    close(fd);
    return -1; // Perte de connexion ou erreur de lecture
}

int main(int argc, char **argv) {
    char kbd_path[256] = {0};
    int force_path = 0;

    // Détection de la langue système
    detect_system_language(detected_lang, sizeof(detected_lang));

    // Si on a passé un chemin en argument, on l'utilise de force
    if (argc >= 2) {
        safe_copy(kbd_path, argv[1], sizeof(kbd_path));
        force_path = 1;
    }

    // Élévation de priorité et sécurisation du démon
    harden_daemon();

    // Décompression de la GUI en RAM
    gui_fd = prepare_gui_memfd();
    if (gui_fd < 0) {
        fprintf(stderr, "[ERROR] Impossible de préparer la GUI de secours en RAM.\n");
        return 1;
    }

    // Configuration des signaux et atexit pour nettoyer les fichiers temporaires
    atexit(cleanup_on_exit);
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Boucle infinie pour gérer le hotplugging / les déconnexions du clavier
    while (!terminate_requested) {
        if (!force_path) {
            if (find_keyboard(kbd_path, sizeof(kbd_path)) != 0) {
                fprintf(stderr, "Aucun clavier compatible trouvé. Nouvelle tentative dans 2 secondes...\n");
                for (int i = 0; i < 20 && !terminate_requested; i++) {
                    usleep(100000);
                }
                continue;
            }
        }

        // Lancement de l'écoute
        int result = listen_for_cad(kbd_path);
        if (result < 0) {
            if (terminate_requested) break;
            fprintf(stderr, "Perte de connexion ou erreur sur le périphérique %s. Tentative de reconnexion dans 2 secondes...\n", kbd_path);
        }
        for (int i = 0; i < 20 && !terminate_requested; i++) {
            usleep(100000);
        }
    }
    
    return 0;
}
