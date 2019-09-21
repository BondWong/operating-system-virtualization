#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>

const unsigned long ABUNDANCE_THRESHOLD = 100 * 1024;
const unsigned long HOST_MINIMUM = 50 * 1024;
const unsigned long MEMORY_CHANGE_DELTA = 20 * 1024;

struct MemStat {
  virDomainPtr domain; // pointer to domain
  virDomainInfoPtr domainInfo; // pointer to domain info
  unsigned long memory; // available mmeory size of a domain
};

typedef struct MemStat* MemStatPtr;

int comparator(const void* p1, const void* p2) {
  MemStatPtr p = (MemStatPtr)p1;
  MemStatPtr q = (MemStatPtr)p2;

  return q->memory - p->memory;
}

void getAndSortMemStat(virConnectPtr conn, MemStatPtr memStats, const int* activeDomains, int domainCnt, int interval) {
  for (int i = 0; i < domainCnt; i++) {
    // get domain
    virDomainPtr domain = virDomainLookupByID(conn, activeDomains[i]);
    virDomainSetMemoryStatsPeriod(domain, interval, 1);
    // get domain info
    virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
    virDomainGetInfo(domain, domainInfo);
    // get domain memory stats
    virDomainMemoryStatStruct memStatStruct[VIR_DOMAIN_MEMORY_STAT_NR];
    virDomainMemoryStats(domain, memStatStruct, VIR_DOMAIN_MEMORY_STAT_NR, 0);
    // store to memStats array
    memStats[i].domain = domain;
    memStats[i].domainInfo = domainInfo;
    memStats[i].memory = memStatStruct[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val;
  }

  // sort memStats in decending order based on available memory size
  qsort((void *)memStats, domainCnt, sizeof(struct MemStat), comparator);
  for (int i = 0; i < domainCnt; i++) {
    fprintf(stdout, "domain %s -- available memory %lu / assigned memory %lu / total memory %lu\n",
      virDomainGetName(memStats[i].domain), memStats[i].memory, memStats[i].domainInfo->memory, memStats[i].domainInfo->maxMem);
  }
}

  void rebalanceMemory(MemStatPtr memStats, int *activeDomains, int domainCnt, unsigned long long freeMemory) {
    unsigned long remain = 0;
    for (int i = 0; i < domainCnt; i++) {
      if (memStats[i].memory > ABUNDANCE_THRESHOLD) {
        // hypervisor inflats balloon to reclaim memory
        remain += MEMORY_CHANGE_DELTA;
        unsigned long newMemorySize = memStats[i].domainInfo->memory - MEMORY_CHANGE_DELTA;
        fprintf(stdout, "Reclaiming memeory %lu from domain %s \n", MEMORY_CHANGE_DELTA, virDomainGetName(memStats[i].domain));
        virDomainSetMemory(memStats[i].domain, newMemorySize);
        fprintf(stdout, "New memory size is %lu\n", memStats[i].domainInfo->memory);
      } else {
        // hypervisor deflats balloon to assign memory
        remain -= MEMORY_CHANGE_DELTA;
        // if hypervisor itself is starving, don't assign
        if (remain <= 0 && freeMemory <= HOST_MINIMUM) {
          fprintf(stdout, "Insufficient memory in the hypervisor skipping \n");
          break;
        }
        unsigned long newMemorySize = memStats[i].domainInfo->memory + MEMORY_CHANGE_DELTA;
        fprintf(stdout, "Assigning memeory %lu to domain %s \n", MEMORY_CHANGE_DELTA, virDomainGetName(memStats[i].domain));
        virDomainSetMemory(memStats[i].domain, newMemorySize);
        fprintf(stdout, "New memory size is %lu\n", memStats[i].domainInfo->memory);
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
    // profile memory stats and sort memstat of each VMs in desneding order
    fprintf(stdout, "%s\n", "Getting domain memeory stat");
    MemStatPtr memStats = malloc(sizeof(struct MemStat) * domainCnt);
    getAndSortMemStat(conn, memStats, activeDomains, domainCnt, interval);

    // run rebalancing algorithm
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
