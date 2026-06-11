#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/monitor.h"

int get_meminfo(meminfo_t *m) {
    if (!m) return -1;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if (sscanf(line, "MemTotal: %lu kB", &m->mem_total_kb) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", &m->mem_available_kb) == 1) continue;
    }
    free(line);
    fclose(f);
    if (m->mem_total_kb == 0) return -1;
    return 0;
}

int get_num_cpus() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (int)n : 1;
}

int get_cpu_times(int cpu, cpu_times_t *t) {
    if (!t) return -1;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char buf[512];
    int found = 0;
    char target[32];
    if (cpu == -1) {
        strcpy(target, "cpu");
    } else {
        snprintf(target, sizeof(target), "cpu%d", cpu);
    }
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, target, strlen(target)) == 0 && (buf[strlen(target)] == ' ' || buf[strlen(target)] == '\t')) {
            // parse fields
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            int rc = sscanf(buf + strlen(target), "%llu %llu %llu %llu %llu %llu %llu %llu",
                &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            if (rc >= 4) {
                t->user = user; t->nice = nice; t->system = system; t->idle = idle;
                t->iowait = (rc >=5) ? iowait : 0;
                t->irq = (rc >=6) ? irq : 0;
                t->softirq = (rc >=7) ? softirq : 0;
                t->steal = (rc >=8) ? steal : 0;
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found ? 0 : -1;
}
