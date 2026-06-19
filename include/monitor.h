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

// A monitored disk filesystem: backing device, mount point, and used-space %.
typedef struct {
    char name[64];    // device, e.g. "/dev/sda1" or a ZFS dataset "zroot/ROOT"
    char mount[96];   // mount point, e.g. "/" or "/home"
    double used_pct;  // 0..100, space used / total
} diskinfo_t;

// Used-space percent (0..100) given total and free blocks. Pure, no I/O:
// clamps free<=total and guards total==0. Same units cancel, so block size
// doesn't matter.
double disk_usage_pct(unsigned long long total_blocks, unsigned long long free_blocks);

// Auto-discover the active, real (disk-backed) mounted filesystems and fill up
// to `max` diskinfo_t entries. Returns the count filled (>=0), or -1 on error.
// Implementation is OS-specific (Linux mounts, BSD getmntinfo, Solaris mnttab);
// pseudo/virtual filesystems (tmpfs, procfs, devfs, …) are skipped.
int get_disks(diskinfo_t *out, int max);

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H
