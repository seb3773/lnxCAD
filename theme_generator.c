#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Structures for theme data */
typedef struct {
    int r, g, b, a;
} ColorRGBA;

typedef struct {
    int nw, dw, nh, dh;
} SizeInfo;

/* Trim leading and trailing whitespace */
static char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* Parse color in format R, G, B, A */
static int parse_color(const char *val, ColorRGBA *color) {
    int r = 0, g = 0, b = 0, a = 255;
    int n = sscanf(val, "%d , %d , %d , %d", &r, &g, &b, &a);
    if (n >= 3) {
        color->r = r;
        color->g = g;
        color->b = b;
        color->a = a;
        return 1;
    }
    return 0;
}

/* Parse size in format nw, dw, nh, dh */
static int parse_size(const char *val, SizeInfo *size) {
    int nw = 0, dw = 0, nh = 0, dh = 0;
    int n = sscanf(val, "%d , %d , %d , %d", &nw, &dw, &nh, &dh);
    if (n >= 2) {
        size->nw = nw;
        size->dw = dw;
        size->nh = nh;
        size->dh = dh;
        return 1;
    }
    return 0;
}

int main() {
    /* Fallback default values (Theme base) */
    ColorRGBA color_bg = {20, 50, 80, 240};
    ColorRGBA color_title_bg = {30, 68, 105, 240};
    ColorRGBA color_hover = {66, 107, 148, 255};
    ColorRGBA color_active = {50, 80, 110, 255};
    ColorRGBA color_curtain = {15, 35, 55, 200};

    SizeInfo size_terminal = {5, 9, 5, 9};
    SizeInfo size_taskmgr = {1, 2, 3, 4};
    SizeInfo size_run = {550, 0, 240, 0};
    SizeInfo size_pwd = {450, 0, 240, 0};
    SizeInfo size_cp = {550, 0, 410, 0};
    SizeInfo size_su = {550, 0, 410, 0};

    /* Open cad_theme.conf */
    FILE *f = fopen("assets_build/cad_theme.conf", "r");
    if (!f) {
        fprintf(stderr, "[Warning] Failed to open assets_build/cad_theme.conf, using default theme parameters.\n");
    } else {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *p = trim(line);
            if (p[0] == '\0' || p[0] == '#') continue;

            char *eq = strchr(p, '=');
            if (!eq) continue;

            *eq = '\0';
            char *key = trim(p);
            char *val = trim(eq + 1);

            if (strcmp(key, "color_bg") == 0) parse_color(val, &color_bg);
            else if (strcmp(key, "color_title_bg") == 0) parse_color(val, &color_title_bg);
            else if (strcmp(key, "color_hover") == 0) parse_color(val, &color_hover);
            else if (strcmp(key, "color_active") == 0) parse_color(val, &color_active);
            else if (strcmp(key, "color_curtain") == 0) parse_color(val, &color_curtain);
            else if (strcmp(key, "size_terminal") == 0) parse_size(val, &size_terminal);
            else if (strcmp(key, "size_taskmgr") == 0) parse_size(val, &size_taskmgr);
            else if (strcmp(key, "size_run") == 0) parse_size(val, &size_run);
            else if (strcmp(key, "size_pwd") == 0) parse_size(val, &size_pwd);
            else if (strcmp(key, "size_cp") == 0) parse_size(val, &size_cp);
            else if (strcmp(key, "size_su") == 0) parse_size(val, &size_su);
        }
        fclose(f);
    }

    /* Write output file cad_theme.h */
    FILE *out = fopen("cad_theme.h", "w");
    if (!out) {
        perror("Failed to create cad_theme.h");
        return 1;
    }

    fprintf(out, "/* Generated automatically by theme_generator - DO NOT EDIT MANUALLY */\n");
    fprintf(out, "#ifndef CAD_THEME_H\n");
    fprintf(out, "#define CAD_THEME_H\n\n");

    /* Output colors */
    fprintf(out, "/* Colors */\n");
    fprintf(out, "#define THEME_COLOR_BG nk_rgba(%d, %d, %d, %d)\n", color_bg.r, color_bg.g, color_bg.b, color_bg.a);
    fprintf(out, "#define THEME_COLOR_TITLE_BG nk_rgba(%d, %d, %d, %d)\n", color_title_bg.r, color_title_bg.g, color_title_bg.b, color_title_bg.a);
    fprintf(out, "#define THEME_COLOR_HOVER nk_rgba(%d, %d, %d, %d)\n", color_hover.r, color_hover.g, color_hover.b, color_hover.a);
    fprintf(out, "#define THEME_COLOR_HOVER_HEX 0x%02X%02X%02X\n", color_hover.r, color_hover.g, color_hover.b);
    fprintf(out, "#define THEME_COLOR_ACTIVE nk_rgba(%d, %d, %d, %d)\n", color_active.r, color_active.g, color_active.b, color_active.a);
    fprintf(out, "#define THEME_COLOR_CURTAIN nk_rgba(%d, %d, %d, %d)\n\n", color_curtain.r, color_curtain.g, color_curtain.b, color_curtain.a);

    /* Helper macro output logic */
    #define WRITE_LAYOUT_MACROS(name, size) \
        if (size.dw > 0) { \
            fprintf(out, "#define THEME_" #name "_W(sw) (((sw) * %d) / %d)\n", size.nw, size.dw); \
        } else { \
            fprintf(out, "#define THEME_" #name "_W(sw) (%d)\n", size.nw); \
        } \
        if (size.dh > 0) { \
            fprintf(out, "#define THEME_" #name "_H(sh) (((sh) * %d) / %d)\n", size.nh, size.dh); \
        } else { \
            fprintf(out, "#define THEME_" #name "_H(sh) (%d)\n", size.nh); \
        } \
        fprintf(out, "#define THEME_" #name "_X(sw, w) (((sw) - (w)) / 2)\n"); \
        if (strcmp(#name, "SU") == 0) { \
            fprintf(out, "#define THEME_SU_Y(sh, h) (((sh) - ((h) + 85)) / 2)\n"); \
        } else { \
            fprintf(out, "#define THEME_" #name "_Y(sh, h) (((sh) - (h)) / 2)\n"); \
        } \
        fprintf(out, "\n")

    WRITE_LAYOUT_MACROS(TERM, size_terminal);
    WRITE_LAYOUT_MACROS(TM, size_taskmgr);
    WRITE_LAYOUT_MACROS(RUN, size_run);
    WRITE_LAYOUT_MACROS(PWD, size_pwd);
    WRITE_LAYOUT_MACROS(CP, size_cp);
    WRITE_LAYOUT_MACROS(SU, size_su);

    fprintf(out, "#endif /* CAD_THEME_H */\n");
    fclose(out);

    printf("Generated cad_theme.h successfully.\n");
    return 0;
}
