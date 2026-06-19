#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <cwchar>
#include <langinfo.h>
// Enable the X/Open wide-character curses API (cchar_t, setcchar, add_wch) for
// the Unicode hollow-square glyph. Defined right before <ncurses.h> so it only
// affects the curses header, not the C++ standard headers above. Links against
// the wide library (-lncursesw); verified on Linux, FreeBSD, OpenBSD, Solaris.
#define _XOPEN_SOURCE_EXTENDED 1
#include <ncurses.h>
#include "../include/monitor.h"

// Color pair ids. Pair 0 is reserved by ncurses for the terminal default
// foreground-on-default-background, which is always legible on whatever
// background the user runs (black, white, or anything else).
enum {
    CP_LOW = 1,   // green  - low utilization
    CP_MED = 2,   // yellow - moderate utilization
    CP_HIGH = 3,  // red    - high utilization
    CP_LABEL = 4  // cyan   - row labels / frame / headings
};

static bool g_color_enabled = false;
// True when the locale is UTF-8, so we can draw the Unicode hollow square
// (U+25A1). Otherwise we fall back to a solid ACS block.
static bool g_use_unicode = false;

// Initialize colors using the terminal's *default* background (-1) so the
// output blends with both light and dark themes. Honors the NO_COLOR
// convention (https://no-color.org) and degrades to monochrome when the
// terminal can't do color.
static bool init_colors() {
    if (std::getenv("NO_COLOR") != nullptr) return false;
    if (!has_colors()) return false;
    start_color();
    // Let pair 0 / unset backgrounds fall through to the real terminal bg.
    use_default_colors();
    init_pair(CP_LOW, COLOR_GREEN, -1);
    init_pair(CP_MED, COLOR_YELLOW, -1);
    init_pair(CP_HIGH, COLOR_RED, -1);
    init_pair(CP_LABEL, COLOR_CYAN, -1);
    return true;
}

static int color_pair_for(usage_level_t lvl) {
    switch (lvl) {
        case USAGE_HIGH: return CP_HIGH;
        case USAGE_MED:  return CP_MED;
        case USAGE_LOW:
        default:         return CP_LOW;
    }
}

// Draw a bracketed gauge whose filled cells are hollow squares (Unicode U+25A1)
// outlined in the utilization color — green (good), yellow (warning), red
// (alert) — so the bar's color reflects its current state. Empty cells are a dim
// ACS checkerboard. The percentage is printed at `px`; the track is [bx, bx+bw).
//
// The hollow square uses the wide curses API (cchar_t/add_wch, -lncursesw) and a
// UTF-8 locale; on a non-UTF-8 terminal it falls back to a solid ACS block. The
// brackets, track and frame stay plain ACS, so the rest renders everywhere.
static void draw_bar(int row, int px, int bx, int bw, double pct) {
    // Percentage label stays in the default color so it is always readable.
    mvprintw(row, px, "%3d%%", (int)std::round(pct));
    if (bw < 3) return;                      // need room for at least "[x]"
    int inner = bw - 2;                      // cells between the brackets
    int filled = bar_fill_cells(pct, inner);
    int pair = color_pair_for(usage_level(pct));   // good / warning / alert color
    short sqpair = g_color_enabled ? (short)pair : 0;

    // Brackets in the accent color (default color when colorless).
    if (g_color_enabled) attron(COLOR_PAIR(CP_LABEL));
    mvaddch(row, bx, '[');
    mvaddch(row, bx + bw - 1, ']');
    if (g_color_enabled) attroff(COLOR_PAIR(CP_LABEL));

    chtype track = ACS_CKBOARD | A_DIM;      // dim checkerboard for empty cells
    for (int i = 0; i < inner; i++) {
        move(row, bx + 1 + i);
        if (i >= filled) { addch(track); continue; }
        if (g_use_unicode) {
            cchar_t sq;
            wchar_t wch[2] = { 0x25A1, 0 };  // ▢ WHITE SQUARE (hollow outline)
            setcchar(&sq, wch, A_BOLD, sqpair, NULL);
            add_wch(&sq);
        } else {
            // Non-UTF-8 fallback: a solid colored block in the same color.
            chtype blk = ACS_BLOCK | A_BOLD;
            if (g_color_enabled) blk |= COLOR_PAIR(pair);
            addch(blk);
        }
    }
}

// Print a label in the accent color (falls back to default when colorless).
static void draw_label(int row, int col, const char *text) {
    if (g_color_enabled) attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, col, "%s", text);
    if (g_color_enabled) attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
}

// Draw the bordered frame (ACS line-drawing) and a centered title on the top
// edge. The border colors with the accent pair when color is on.
static void draw_frame(int rows, int cols) {
    (void)rows;
    if (g_color_enabled) attron(COLOR_PAIR(CP_LABEL));
    box(stdscr, 0, 0);
    if (g_color_enabled) attroff(COLOR_PAIR(CP_LABEL));

    const char *title = " ascii-monitor ";
    int tx = (cols - (int)std::strlen(title)) / 2;
    if (tx < 1) tx = 1;
    if (g_color_enabled) attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(0, tx, "%s", title);
    if (g_color_enabled) attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    // Adopt the user's locale so wide-char curses can render the Unicode hollow
    // square; only enable it when the locale is actually UTF-8.
    setlocale(LC_ALL, "");
    const char *cs = nl_langinfo(CODESET);
    g_use_unicode = cs && (strstr(cs, "UTF-8") || strstr(cs, "utf-8") ||
                           strstr(cs, "UTF8")  || strstr(cs, "utf8"));

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    g_color_enabled = init_colors();

    int ncpus = get_num_cpus();
    std::vector<cpu_times_t> prev(ncpus), cur(ncpus);
    for (int i = 0; i < ncpus; i++) get_cpu_times(i, &prev[i]);

    meminfo_t mem_prev = {0};
    get_meminfo(&mem_prev);

    const int refresh_ms = 1000;

    while (true) {
        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < ncpus; i++) get_cpu_times(i, &cur[i]);
        meminfo_t mem; get_meminfo(&mem);

        erase();
        int rows, cols; getmaxyx(stdscr, rows, cols);

        // Draw a frame when there's room; otherwise fall back to edge-to-edge.
        bool border = (rows >= 5 && cols >= 24);
        int oy = border ? 1 : 0;             // content origin row
        int ox = border ? 1 : 0;             // content origin column
        int right = border ? cols - 2 : cols - 1;   // last usable inner column
        int footer_row = border ? rows - 2 : rows - 1;
        if (border) draw_frame(rows, cols);

        // Memory line: label, percent, block bar, then the MB readout flush right.
        int mrow = oy;
        if (mrow < footer_row) {
            double mem_used_kb = (double)(mem.mem_total_kb - mem.mem_available_kb);
            double mem_pct = (mem.mem_total_kb > 0)
                ? (mem_used_kb * 100.0 / (double)mem.mem_total_kb) : 0.0;
            draw_label(mrow, ox, "Memory:");

            char mb[48];
            snprintf(mb, sizeof(mb), "%lu/%lu MB",
                     (mem.mem_total_kb - mem.mem_available_kb) / 1024,
                     mem.mem_total_kb / 1024);
            int mblen = (int)std::strlen(mb);
            int mb_x = right - mblen + 1;    // so the readout ends at `right`

            int px = ox + 8;                 // after "Memory: "
            int bx = px + 5;                 // after "NNN% "
            int bw = (mb_x - 1) - bx;        // leave a 1-col gap before the MB text
            draw_bar(mrow, px, bx, bw, mem_pct);
            if (mb_x > bx) mvprintw(mrow, mb_x, "%s", mb);
        }

        // One block bar per logical CPU, each spanning the full inner width.
        for (int i = 0; i < ncpus; i++) {
            int r = oy + 1 + i;
            if (r >= footer_row) break;      // keep clear of the footer/border
            double pct = calc_cpu_usage(&prev[i], &cur[i]);
            char label[16];
            snprintf(label, sizeof(label), "CPU%02d:", i);
            draw_label(r, ox, label);
            int px = ox + 7;                 // after "CPUNN: "
            int bx = px + 5;                 // after "NNN% "
            int bw = right - bx + 1;
            draw_bar(r, px, bx, bw, pct);
        }

        // Auto-discovered disk filesystems, one bar of used-space each, below the
        // CPUs. The set is whatever this host actually has mounted (it differs by
        // OS and host), so we just ask get_disks() each refresh.
        diskinfo_t disks[24];
        int nd = get_disks(disks, 24);       // -1 on error -> loop simply skips
        for (int i = 0; i < nd; i++) {
            int dr = oy + 1 + ncpus + i;
            if (dr >= footer_row) break;     // out of vertical room
            char dl[16];
            snprintf(dl, sizeof(dl), "%-8.8s", disks[i].mount);   // mount-point label
            draw_label(dr, ox, dl);
            char dev[28];
            snprintf(dev, sizeof(dev), "%.24s", disks[i].name);   // device, flush right
            int devlen = (int)std::strlen(dev);
            int dev_x = right - devlen + 1;
            int px = ox + 9;                 // after the 8-char mount + a space
            int bx = px + 5;                 // after "NNN% "
            int bw = (dev_x - 1) - bx;       // leave a 1-col gap before the device
            draw_bar(dr, px, bx, bw, disks[i].used_pct);
            if (dev_x > bx) mvprintw(dr, dev_x, "%s", dev);
        }

        // Footer keeps the literal "quit" and the "[mono]" marker (the
        // integration test greps for both).
        mvprintw(footer_row, ox, "Press 'q' to quit | %d ms refresh%s",
                 refresh_ms, g_color_enabled ? "" : "  [mono]");
        refresh();

        // copy cur to prev
        prev = cur;

        // check for key
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleep_for = std::chrono::milliseconds(refresh_ms) - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        if (sleep_for.count() > 0) std::this_thread::sleep_for(sleep_for);
    }

    endwin();
    return 0;
}
