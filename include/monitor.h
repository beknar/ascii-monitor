#ifndef MONITOR_H
#define MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long mem_total_kb;
    unsigned long mem_available_kb;
} meminfo_t;

// Populate meminfo_t; returns 0 on success
int get_meminfo(meminfo_t *m);

typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} cpu_times_t;

// Number of logical CPUs
int get_num_cpus();

// Fill cpu_times_t for cpu index (0..n-1). Returns 0 on success
int get_cpu_times(int cpu, cpu_times_t *t);

// Compute CPU utilization percentage (0..100) between two cumulative samples.
double calc_cpu_usage(const cpu_times_t *a, const cpu_times_t *b);

// Usage severity levels used to pick a display color. Ordered low -> high.
typedef enum {
    USAGE_LOW = 0,   // comfortable
    USAGE_MED = 1,   // getting busy
    USAGE_HIGH = 2   // saturated
} usage_level_t;

// Map a percentage (0..100) to a severity level. Pure function, no I/O.
usage_level_t usage_level(double pct);

// Number of filled cells for a bar `cells` wide at `pct` percent, rounded and
// clamped to [0, cells]. Pure function, no I/O. Used by the bar renderer.
int bar_fill_cells(double pct, int cells);

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H
