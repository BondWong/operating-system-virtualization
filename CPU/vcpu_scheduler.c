#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>

struct pCPUStats {
  unsigned long long CPUTimeDelta; // time delta from last interval
  int pCPU; // pCPU number
  int * domainIds; // id of doamins that use this pCPU
  int domainIdCnt;
};

struct vCPUStats {
  unsigned long long CPUTimeDelta; // time delta from last interval
  int domainID;
};

typedef struct pCPUStats* pCPUStatsPtr;
typedef struct vCPUStats* vCPUStatsPtr;

int sampleDomainInfo(int domainCnt, int* activeDomains, virDomainInfoPtr prevDomainInfo,
  virDomainInfoPtr curDomainInfo, pCPUStatsPtr pCPUStats, vCPUStatsPtr vCPUStats) {
  if (curDomainInfo == NULL) {
    virDomainInfoPtr curDomainInfo = malloc(sizeof(virDomainInfo) * domainCnt);
    virDomainInfoPtr prevDomainInfo = malloc(sizeof(virDomainInfo) * domainCnt);
    memset(curDomainInfo, 0, sizeof(virDomainInfo) * domainCnt);
    memset(prevDomainInfo, 0, sizeof(virDomainInfo) * domainCnt);
  }

  // write cur domain info to prev domain info,  and update domain info
  for (int i = 0; i < domainCnt; i++) {
    virDomainPtr domain = virDomainLookupByID(conn, activeDomains[i]);
    if (virDomainGetInfo(domain, curDomainInfo[i]) == -1) {
      fprintf(stderr, "Failed to get domain info");
      exit(1);
    }
    int pCPU = curDomainInfo[i].cpu;
    unsigned long long pCPUTimEnd = curDomainInfo[i].cpuTime;
    unsigned long long pCPUTimStart = prevDomainInfo[i].cpuTime;
    unsigned long long delta = pCPUTimEnd - pCPUTimStart;
    pCPUStats[pCPU].CPUTimeDelta += delta;
    pCPUStats[pCPU].pCPU = pCPU;
    pCPUStats[pCPU].domainIdCnt++;
    int* domainIds = malloc(sizeof(int) * pCPUStats[pCPU].domainIdCnt);
    if (pCPUStats[pCPU].domainIdCnt > 1) {
      memcpy(domainIds, pCPUStats[pCPU].domainIds, sizeof(int) * pCPUStats[pCPU].domainIdCnt - 1);
      free(pCPUStats[pCPU].domainIds);
    }
    domainIds[pCPUStats[pCPU].domainIdCnt - 1] = activeDomains[i];
    pCPUStats[pCPU].domainIds = domainIds;
    vCPUStats[i] = activeDomains[i];
    vCPUStats[i].CPUTimeDelta = delta;

    fprintf(stdout, "guest domain %d -- %s -- vCPU usage %llu assigned to pCPU %d pCPU usage %llu\n",
      activeDomains[i], virDomainGetName(domainPtr), vCPUStats[i].CPUTimeDelta, pCPUStats[pCPU].pCPU, pCPUStats[pCPU].CPUTimeDelta);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Error: Invalid arugment");
    exit(1);
  }

  int interval = atoi(argv[1]);

  // get connection to hypervisor
  virConnectPtr conn;
  conn = virConnectOpen("qemu:///system");
  if (conn == NULL) {
    fprintf(stderr, "Failed to open connection to hypervisor\n");
    exit(1);
  }

  virDomainInfoPtr curDomainInfo = NULL;
  virDomainInfoPtr prevDomainInfo = NULL;
  int domainCnt = virConnectNumOfDomains(conn);
  while(domainCnt > 0) {
    // get all active running virtual machines
    int *activeDomains = malloc(sizeof(int) * domainCnt);
    virConnectListDomains(conn, activeDomains, domainCnt);

    pCPUStatsPtr pCPUStats = malloc(sizeof(struct pCPUStats) * 4);
    pCPUStatsPtr vCPUStats = malloc(sizeof(struct pCPUStats) * domainCnt);
    sampleDomainInfo(domainCnt, activeDomains, prevDomainInfo, curDomainInfo, pCPUStats, vCPUStats)
    // get each pCPU states
    // sort them from buiest to freeist
    // iterate the list and move job from busy ones to free ones
    free(pCPUStats);
    free(vCPUStats);
    sleep(interval);
    int domainCnt = virConnectNumOfDomains(conn);
  }

  virConnectClose(conn);
  if (curDomainInfo != NULL) free(curDomainInfo);
  if (prevDomainInfo != NULL) free(prevDomainInfo);
  return 0;
}
