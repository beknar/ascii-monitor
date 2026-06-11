#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include <ncurses.h>
#include "../include/monitor.h"

static double calc_cpu_usage(const cpu_times_t &a, const cpu_times_t &b) {
    unsigned long long idle_a = a.idle + a.iowait;
    unsigned long long idle_b = b.idle + b.iowait;
    unsigned long long nonidle_a = a.user + a.nice + a.system + a.irq + a.softirq + a.steal;
    unsigned long long nonidle_b = b.user + b.nice + b.system + b.irq + b.softirq + b.steal;
    unsigned long long total_a = idle_a + nonidle_a;
    unsigned long long total_b = idle_b + nonidle_b;
    unsigned long long totald = total_b - total_a;
    unsigned long long idled = idle_b - idle_a;
    if (totald == 0) return 0.0;
    double cpu_percentage = (double)(totald - idled) * 100.0 / (double)totald;
    return cpu_percentage;
}

static void draw_bar(int row, int col_percent, int width, double pct) {
    // left: percentage printed at col_percent, then bar starts after a space
    mvprintw(row, col_percent, "%3d%% ", (int)std::round(pct));
    int barx = col_percent + 4;
    int barw = width - (barx + 1);
    if (barw < 2) return;
    int filled = (int)std::round((pct/100.0) * barw);
    move(row, barx);
    for (int i=0;i<filled;i++) addch('#');
    for (int i=filled;i<barw;i++) addch(' ');
}

int main(int argc, char **argv) {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);

    int ncpus = get_num_cpus();
    std::vector<cpu_times_t> prev(ncpus), cur(ncpus);
    for (int i=0;i<ncpus;i++) get_cpu_times(i, &prev[i]);

    meminfo_t mem_prev = {0};
    get_meminfo(&mem_prev);

    const int refresh_ms = 1000;

    while (true) {
        auto start = std::chrono::steady_clock::now();

        for (int i=0;i<ncpus;i++) get_cpu_times(i, &cur[i]);
        meminfo_t mem; get_meminfo(&mem);

        erase();
        int rows, cols; getmaxyx(stdscr, rows, cols);
        int left_col = 1;
        int usable_width = cols - left_col - 2;
        // Memory line at top
        double mem_used_kb = (double)(mem.mem_total_kb - mem.mem_available_kb);
        double mem_pct = (mem.mem_total_kb>0) ? (mem_used_kb*100.0 / (double)mem.mem_total_kb) : 0.0;
        mvprintw(0, 0, "Memory: ");
        draw_bar(0, 8, cols, mem_pct);
        mvprintw(0, cols-30, "%lu/%lu MB", (mem.mem_total_kb - mem.mem_available_kb)/1024, mem.mem_total_kb/1024);

        // CPU lines
        for (int i=0;i<ncpus && i+2 < rows;i++) {
            double pct = calc_cpu_usage(prev[i], cur[i]);
            mvprintw(i+1, 0, "CPU%02d: ", i);
            draw_bar(i+1, 7, cols, pct);
        }

        mvprintw(rows-1, 0, "Press 'q' to quit. Update interval: %d ms", refresh_ms);
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
