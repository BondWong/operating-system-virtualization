#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>

struct pCPUStats {
  double CPUTimeDelta; // time delta from last interval
  int * domainIds; // id of doamins that use this pCPU
  int domainIdCnt;
};

struct vCPUStats {
  unsigned long long cpuTime;
  double CPUTimeDelta; // time delta from last interval
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
    if (preStatsIdx != -1) fprintf(stdout, "startTime: %llu\n", preVCPUStats[preStatsIdx].cpuTime);
    double delta = (double) (vcpuInfo->cpuTime - pCPUTimStart);
    curVCPUStats[i].domainID = activeDomains[i];
    curVCPUStats[i].CPUTimeDelta = delta;
    curVCPUStats[i].cpuTime = vcpuInfo->cpuTime;
    pCPUStats[pCPU].CPUTimeDelta += delta;
    pCPUStats[pCPU].domainIdCnt++;
    pCPUStats[pCPU].domainIds[pCPUStats[pCPU].domainIdCnt - 1] = activeDomains[i];

    fprintf(stdout, "guest domain %d -- %s -- vCPU usage %f assigned to pCPU %d pCPU usage %f\n",
      activeDomains[i], virDomainGetName(domain), curVCPUStats[i].CPUTimeDelta, pCPU, pCPUStats[pCPU].CPUTimeDelta);
    free(domainInfo);
    free(vcpuInfo);
  }
  fprintf(stdout, "%llu\n", preVCPUStats[0].cpuTime);
  memcpy(preVCPUStats, curVCPUStats, sizeof(struct vCPUStats) * domainCnt);
  fprintf(stdout, "%llu\n", preVCPUStats[0].cpuTime);

  return 0;
}

int rebalance(pCPUStatsPtr pCPUStats, int pCPUCnt, vCPUStatsPtr curVCPUInfo, int vCPUCnt) {
  int preFrom = -1, preTo = -1, curFrom = -1, curTo = -1;
  int shouldStop = 0;
  while (shouldStop == 0) {
    unsigned long long curMax = 0;
    unsigned long long curMin = -1;
    int maxPCpuId = -1;
    int minPCpuId = -1;
    for (int i = 0; i < pCPUCnt; i++) {
      fprintf(stdout, "pCPU %d - pCPUTimeDelta %f \n", i, pCPUStats[i].CPUTimeDelta);
      if (pCPUStats[i].CPUTimeDelta > curMax) {
        curMax = pCPUStats[i].CPUTimeDelta;
        maxPCpuId = i;
      }
      if (curMin == -1 || pCPUStats[i].CPUTimeDelta < curMin) {
        curMin = pCPUStats[i].CPUTimeDelta;
        minPCpuId = i;
      }
    }

    preFrom = curFrom;
    preTo = curTo;
    curFrom = maxPCpuId;
    curTo = minPCpuId;
    if (curTo == preFrom && curFrom == preTo) shouldStop = 1;
    if (pCPUStats[curFrom].domainIdCnt == 0) {
      fprintf(stderr, "No domain id pinned to pCPU %d \n", curFrom);
      exit(1);
    };

    int id = pCPUStats[curFrom].domainIds[pCPUStats[curFrom].domainIdCnt - 1];
    fprintf(stdout, "looking for domain info... \n");
    int index = findById(curVCPUInfo, vCPUCnt, id);
    fprintf(stdout, "moving workload of size: %f, domain id: %d, from pCPU: %d, to pCPU: %d \n",
      curVCPUInfo[index].CPUTimeDelta, curVCPUInfo[index].domainID, curFrom, curTo);

    pCPUStats[curTo].CPUTimeDelta += curVCPUInfo[index].CPUTimeDelta;
    pCPUStats[curTo].domainIdCnt++;
    pCPUStats[curTo].domainIds[pCPUStats[curTo].domainIdCnt - 1] = id;
    pCPUStats[curFrom].CPUTimeDelta -= curVCPUInfo[index].CPUTimeDelta;
    pCPUStats[curFrom].domainIdCnt--;

    fprintf(stdout, "done moving workflow of domain id: %d, from pCPU: %d, to pCPU: %d \n",
      pCPUStats[curTo].domainIds[pCPUStats[curTo].domainIdCnt - 1], curFrom, curTo);
  }

  return 0;
}

int repin(virConnectPtr conn, pCPUStatsPtr curPCPUStats, int pCPUCnt) {
  for (int i = 0; i < pCPUCnt; i++) {
    unsigned char pCPU = 0x1 << i;
    fprintf(stdout, "domain cnt %d\n", curPCPUStats[i].domainIdCnt);
    for (int j = 0; j < curPCPUStats[i].domainIdCnt; i++) {
      fprintf(stdout, "Repining domain %d ... \n", curPCPUStats[i].domainIds[j]);
      virDomainPtr domain = virDomainLookupByID(conn, curPCPUStats[i].domainIds[j]);
      virDomainPinVcpu(domain, 0, &pCPU, 1);
    }
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
  for (int i = 0; i < domainCnt; i++) {
    curVCPUInfo[i].domainID = -1;
    curVCPUInfo[i].cpuTime = 0;
    curVCPUInfo[i].CPUTimeDelta = 0;
  }
  vCPUStatsPtr prevVCPUInfo = malloc(sizeof(struct pCPUStats) * domainCnt);
  memcpy(prevVCPUInfo, curVCPUInfo, sizeof(struct vCPUStats) * domainCnt);
  while(domainCnt > 0) {
    // get all active running virtual machines
    int *activeDomains = malloc(sizeof(int) * domainCnt);
    virConnectListDomains(conn, activeDomains, domainCnt);
    pCPUStatsPtr curPCPUStats = malloc(sizeof(struct pCPUStats) * 4);
    // pCPUStatsPtr prePCPUStats = malloc(sizeof(struct pCPUStats) * 4);
    for (int i = 0; i < 4; i++) {
      curPCPUStats[i].CPUTimeDelta = 0;
      curPCPUStats[i].domainIds = malloc(sizeof(int) * domainCnt);
      curPCPUStats[i].domainIdCnt = 0;
    }
    // memcpy(prePCPUStats, curPCPUStats, 4 * sizeof(struct pCPUStats));

    fprintf(stdout, "Sampling pCPU stats...\n");
    sampleDomainInfo(conn, domainCnt, activeDomains, curPCPUStats, prevVCPUInfo, curVCPUInfo);

    fprintf(stdout, "Running rebalance algorithm...\n");
    // rebalance(curPCPUStats, 4, curVCPUInfo, domainCnt);

    fprintf(stdout, "Repinning vCPUs...\n");
    // repin(conn, curPCPUStats, 4);

    if (prevVCPUInfo != NULL) free(prevVCPUInfo);
    // if (prePCPUStats != NULL) free(prePCPUStats);
    sleep(interval);
  }

  virConnectClose(conn);
  if (curVCPUInfo != NULL) free(curVCPUInfo);
  if (prevVCPUInfo != NULL) free(prevVCPUInfo);
  return 0;
}
