#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ncurses.h>
#include "../include/monitor.h"

// Color pair ids. Pair 0 is reserved by ncurses for the terminal default
// foreground-on-default-background, which is always legible on whatever
// background the user runs (black, white, or anything else).
enum {
    CP_LOW = 1,   // green  - low utilization
    CP_MED = 2,   // yellow - moderate utilization
    CP_HIGH = 3,  // red    - high utilization
    CP_LABEL = 4  // cyan   - row labels / headings
};

static bool g_color_enabled = false;

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

static void draw_bar(int row, int col_percent, int width, double pct) {
    // Percentage label stays in the default color so it is always readable.
    mvprintw(row, col_percent, "%3d%% ", (int)std::round(pct));
    int barx = col_percent + 4;
    int barw = width - (barx + 1);
    if (barw < 2) return;
    int filled = (int)std::round((pct / 100.0) * barw);
    if (filled < 0) filled = 0;
    if (filled > barw) filled = barw;

    int pair = color_pair_for(usage_level(pct));
    move(row, barx);
    if (g_color_enabled) attron(COLOR_PAIR(pair) | A_BOLD);
    for (int i = 0; i < filled; i++) addch('#');
    if (g_color_enabled) attroff(COLOR_PAIR(pair) | A_BOLD);
    for (int i = filled; i < barw; i++) addch(' ');
}

// Print a label in the accent color (falls back to default when colorless).
static void draw_label(int row, int col, const char *text) {
    if (g_color_enabled) attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, col, "%s", text);
    if (g_color_enabled) attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
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
        // Memory line at top
        double mem_used_kb = (double)(mem.mem_total_kb - mem.mem_available_kb);
        double mem_pct = (mem.mem_total_kb > 0) ? (mem_used_kb * 100.0 / (double)mem.mem_total_kb) : 0.0;
        draw_label(0, 0, "Memory: ");
        draw_bar(0, 8, cols, mem_pct);
        mvprintw(0, cols - 30, "%lu/%lu MB", (mem.mem_total_kb - mem.mem_available_kb) / 1024, mem.mem_total_kb / 1024);

        // CPU lines
        for (int i = 0; i < ncpus && i + 2 < rows; i++) {
            double pct = calc_cpu_usage(&prev[i], &cur[i]);
            char label[16];
            snprintf(label, sizeof(label), "CPU%02d: ", i);
            draw_label(i + 1, 0, label);
            draw_bar(i + 1, 7, cols, pct);
        }

        mvprintw(rows - 1, 0, "Press 'q' to quit. Update interval: %d ms%s",
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
