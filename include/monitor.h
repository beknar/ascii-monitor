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

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H
