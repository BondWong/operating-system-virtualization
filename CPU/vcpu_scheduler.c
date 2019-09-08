#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>

struct pCPUStats {
  unsigned long long CPUTimeDelta; // time delta from last interval
  int pCPU; // pCPU number
  int * domainIds; // id of doamins that use this pCPU
  int domainIdCnt;
};

struct vCPUStats {
  unsigned long long cpuTime;
  unsigned long long CPUTimeDelta; // time delta from last interval
  int domainID;
};

typedef struct pCPUStats* pCPUStatsPtr;
typedef struct vCPUStats* vCPUStatsPtr;

int findById(vCPUStatsPtr vCPUStats, int size, int id) {
  for (int i = 0; i < size; i++) if (vCPUStats[i].domainID == id) return i;
  return -1;
}

int sampleDomainInfo(virConnectPtr conn, int domainCnt, int* activeDomains,
  pCPUStatsPtr pCPUStats, vCPUStatsPtr preVCPUStats, vCPUStatsPtr curVCPUStats) {
  // write cur domain info to prev domain info,  and update domain info
  for (int i = 0; i < domainCnt; i++) {
    virDomainPtr domain = virDomainLookupByID(conn, activeDomains[i]);
    virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
    if (virDomainGetInfo(domain, domainInfo) == -1) {
      fprintf(stderr, "Failed to get domain info\n");
      exit(1);
    }
    if (domainInfo->nrVirtCpu != 1) {
      fprintf(stderr, "Error, vCPU not equal to 1\n");
      exit(1);
    }
    virVcpuInfoPtr vcpuInfo = malloc(sizeof(virVcpuInfo));
    virDomainGetVcpus(domain, vcpuInfo, domainInfo->nrVirtCpu, NULL, 0);
    int preStatsIdx = findById(preVCPUStats, domainCnt, activeDomains[i]);
    int pCPU = vcpuInfo->cpu;
    unsigned long long pCPUTimStart = preStatsIdx == -1 ? 0 : preVCPUStats[preStatsIdx].cpuTime;
    unsigned long long delta = vcpuInfo->cpuTime - pCPUTimStart;
    curVCPUStats[i].domainID = activeDomains[i];
    curVCPUStats[i].CPUTimeDelta = delta;
    curVCPUStats[i].cpuTime =vcpuInfo->cpuTime;
    pCPUStats[pCPU].CPUTimeDelta += delta;
    pCPUStats[pCPU].pCPU = pCPU;
    pCPUStats[pCPU].domainIdCnt++;
    pCPUStats[pCPU].domainIds = malloc(sizeof(int) * domainCnt);
    pCPUStats[pCPU].domainIds[pCPUStats[pCPU].domainIdCnt - 1] = activeDomains[i];

    fprintf(stdout, "guest domain %d -- %s -- vCPU usage %llu assigned to pCPU %d pCPU usage %llu\n",
      activeDomains[i], virDomainGetName(domain), curVCPUStats[i].CPUTimeDelta, pCPUStats[pCPU].pCPU, pCPUStats[pCPU].CPUTimeDelta);
    free(domainInfo);
    free(vcpuInfo);
  }
  memcpy(preVCPUStats, curVCPUStats, sizeof(struct vCPUStats) * domainCnt);

  return 0;
}

int rebalance(pCPUStatsPtr pCPUStats, int pCPUCnt, vCPUStatsPtr curVCPUInfo, int vCPUCnt) {
  int preFrom = -1, preTo = -1, curFrom = -1, curTo = -1;
  int shouldStop = 0;
  while (shouldStop == 0) {
    unsigned long long curMax = 0;
    unsigned long long curMin = -1;
    int maxIdx = -1;
    int minIdx = -1;
    for (int i = 0; i < pCPUCnt; i++) {
      if (pCPUStats[i].CPUTimeDelta > curMax) {
        curMax = pCPUStats[i].CPUTimeDelta;
        maxIdx = i;
      }
      if (curMin == -1 || pCPUStats[i].CPUTimeDelta < curMin) {
        curMin = pCPUStats[i].CPUTimeDelta;
        minIdx = i;
      }
    }
    preFrom = curFrom;
    preTo = curTo;
    curFrom = pCPUStats[maxIdx].pCPU;
    curTo = pCPUStats[minIdx].pCPU;
    if (curTo == preFrom && curFrom == preTo) shouldStop = 1;
    if (pCPUStats[curFrom].domainIdCnt == 0) break;

    int id = pCPUStats[curFrom].domainIds[pCPUStats[curFrom].domainIdCnt - 1];
    int index = findById(curVCPUInfo, vCPUCnt, id);
    fprintf(stdout, "workload size: %llu, domain id: %d, from pCPU: %d, to pCPU: %d \n",
      curVCPUInfo[index].CPUTimeDelta, curVCPUInfo[index].domainID, pCPUStats[curFrom].pCPU, pCPUStats[curTo].pCPU);

    pCPUStats[curTo].CPUTimeDelta += curVCPUInfo[index].CPUTimeDelta;
    pCPUStats[curTo].domainIdCnt++;
    pCPUStats[curTo].domainIds[pCPUStats[curTo].domainIdCnt - 1] = id;
    pCPUStats[curFrom].CPUTimeDelta -= curVCPUInfo[index].CPUTimeDelta;
    pCPUStats[curFrom].domainIdCnt--;

    fprintf(stdout, "done moving workflow of domain id: %d, from pCPU: %d, to pCPU: %d \n",
      pCPUStats[curTo].domainIds[pCPUStats[curTo].domainIdCnt - 1], pCPUStats[curFrom].pCPU, pCPUStats[curTo].pCPU);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Error: Invalid arugment\n");
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

  int domainCnt = virConnectNumOfDomains(conn);
  vCPUStatsPtr curVCPUInfo = malloc(sizeof(struct pCPUStats) * domainCnt);
  vCPUStatsPtr prevVCPUInfo = malloc(sizeof(struct pCPUStats) * domainCnt);
  while(domainCnt > 0) {
    // get all active running virtual machines
    int *activeDomains = malloc(sizeof(int) * domainCnt);
    virConnectListDomains(conn, activeDomains, domainCnt);

    fprintf(stdout, "Sampling pCPU stats...\n");
    pCPUStatsPtr pCPUStats = malloc(sizeof(struct pCPUStats) * 4);
    // get pCPU stats
    sampleDomainInfo(conn, domainCnt, activeDomains, pCPUStats, prevVCPUInfo, curVCPUInfo);
    // rebalance pCPU
    rebalance(pCPUStats, 4, curVCPUInfo, domainCnt);
    free(pCPUStats);
    sleep(interval);
  }

  virConnectClose(conn);
  if (curVCPUInfo != NULL) free(curVCPUInfo);
  if (prevVCPUInfo != NULL) free(prevVCPUInfo);
  return 0;
}
