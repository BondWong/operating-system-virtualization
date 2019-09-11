#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>

const unsigned long ABUNDANCE_THRESHOLD = 100 * 1024; // 500 MB

struct MemStat {
  virDomainPtr domain;
  virDomainInfoPtr domainInfo;
  unsigned long memory;
};

typedef struct MemStat* MemStatPtr;

int comparator(const void* p1, const void* p2) {
  MemStatPtr p = (MemStatPtr)p1;
  MemStatPtr q = (MemStatPtr)p2;

  return q->memory - p->memory;
}

void getAndSortMemStat(virConnectPtr conn, MemStatPtr memStats, const int* activeDomains, int domainCnt, int interval) {
  for (int i = 0; i < domainCnt; i++) {
    virDomainPtr domain = virDomainLookupByID(conn, activeDomains[i]);
    virDomainSetMemoryStatsPeriod(domain, interval, 1);
    virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
    virDomainGetInfo(domain, domainInfo);
    virDomainMemoryStatStruct memStatStruct[VIR_DOMAIN_MEMORY_STAT_NR];
    virDomainMemoryStats(domain, memStatStruct, VIR_DOMAIN_MEMORY_STAT_NR, 0);
    memStats[i].domain = domain;
    memStats[i].domainInfo = domainInfo;
    memStats[i].memory = memStatStruct[VIR_DOMAIN_MEMORY_STAT_UNUSED].val;
  }

  qsort((void *)memStats, domainCnt, sizeof(struct MemStat), comparator);
  for (int i = 0; i < domainCnt; i++) {
    fprintf(stdout, "domain %s -- unused memory %lu / %lu \n",
      virDomainGetName(memStats[i].domain), memStats[i].memory, memStats[i].domainInfo->maxMem);
  }
}

  void rebalanceMemory(MemStatPtr memStats, int *activeDomains, int domainCnt, unsigned long long freeMemory) {
    unsigned long remain = 0;
    for (int i = 0; i < domainCnt; i++) {
      if (memStats[i].memory == ABUNDANCE_THRESHOLD) {
        continue;
      } else if (memStats[i].memory > ABUNDANCE_THRESHOLD) {
        unsigned long reclaim = memStats[i].memory - ABUNDANCE_THRESHOLD;
        remain += reclaim;
        // hypervisor inflats balloon to reclaim memory
        unsigned long newMemorySize = memStats[i].domainInfo->maxMem - reclaim;
        fprintf(stdout, "Reclaiming memeory %lu from domain %s \n", reclaim, virDomainGetName(memStats[i].domain));
        virDomainSetMaxMemory(memStats[i].domain, newMemorySize);
        fprintf(stdout, "New memory size is %lu\n", newMemorySize);
      } else {
        unsigned long assign = ABUNDANCE_THRESHOLD - memStats[i].memory;
        remain -= assign;
        // if hypervisor itself is starving, don't assign
        if (remain <= 0 && freeMemory <= ABUNDANCE_THRESHOLD) {
          fprintf(stdout, "Insufficient memory in the hypervisor skipping \n");
          break;
        }
        // hypervisor deflats balloon to assign memory
        unsigned long newMemorySize = memStats[i].domainInfo->maxMem + assign;
        fprintf(stdout, "Assigning memeory %lu from domain %s \n", assign, virDomainGetName(memStats[i].domain));
        virDomainSetMaxMemory(memStats[i].domain, newMemorySize);
        fprintf(stdout, "New memory size is %lu\n", newMemorySize);
      }
    }
  }

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Error: Invalid arugment\n");
    exit(1);
  }

  int interval = atoi(argv[1]);
  virConnectPtr conn;
  conn = virConnectOpen("qemu:///system");
  if (conn == NULL) {
    fprintf(stderr, "Failed to open connection to hypervisor\n");
    exit(1);
  }

  int domainCnt = virConnectNumOfDomains(conn);
  int *activeDomains = malloc(sizeof(int) * domainCnt);
  virConnectListDomains(conn, activeDomains, domainCnt);

  while (domainCnt > 0) {
    // sort memstat of each VMs in desneding order
    fprintf(stdout, "%s\n", "Getting domain memeory stat");
    MemStatPtr memStats = malloc(sizeof(struct MemStat) * domainCnt);
    getAndSortMemStat(conn, memStats, activeDomains, domainCnt, interval);
    // iterate each, collect memory from those have wasteful memory, assign to those are starving
    // during the iteration, if memory remain is negative, start using hypervisor memory
    // in the end, if remaining is positive, assign back to hypervisor
    fprintf(stdout, "%s\n", "Rebalancing domain memeory");
    unsigned long long freeMemory = virNodeGetFreeMemory(conn) / 1024;
    rebalanceMemory(memStats, activeDomains, domainCnt, freeMemory);
    free(memStats);
    sleep(interval);
  }

  free(activeDomains);
  virConnectClose(conn);
  return 0;
}
