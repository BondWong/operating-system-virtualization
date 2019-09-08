#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>

const int PCPU_CNT = 4;

struct pCPUStats {
  unsigned long long CPUTimeDelta; // time delta from last interval
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

int arraycmp(int* arr1, int size1, int* arr2, int size2) {
  if (size1 <= 0 || size2 <= 0) return -1;
  if (size1 != size2) return -1;

  for (int i = 0; i < size1; i++) {
    fprintf(stdout, "%d ", arr1[i]);
  }
  fprintf(stdout, "\n");
  for (int i = 0; i < size1; i++) {
    fprintf(stdout, "%d ", arr2[i]);
  }
  fprintf(stdout, "\n");

  return memcmp(arr1, arr2, size1);
}

int comparator(const void* p1, const void* p2) {
  vCPUStatsPtr p = (vCPUStatsPtr)p1;
  vCPUStatsPtr q = (vCPUStatsPtr)p2;

  return p->CPUTimeDelta - q->CPUTimeDelta;
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
    curVCPUStats[i].cpuTime = vcpuInfo->cpuTime;
    pCPUStats[pCPU].CPUTimeDelta += delta;
    pCPUStats[pCPU].domainIdCnt++;
    pCPUStats[pCPU].domainIds[pCPUStats[pCPU].domainIdCnt - 1] = activeDomains[i];

    fprintf(stdout, "guest domain %d -- %s -- vCPU usage %llu assigned to pCPU %d pCPU usage %llu\n",
      activeDomains[i], virDomainGetName(domain), curVCPUStats[i].CPUTimeDelta, pCPU, pCPUStats[pCPU].CPUTimeDelta);
    free(domainInfo);
    free(vcpuInfo);
  }
  memcpy(preVCPUStats, curVCPUStats, sizeof(struct vCPUStats) * domainCnt);

  return 0;
}

int rebalance(pCPUStatsPtr pCPUStats, int pCPUCnt, vCPUStatsPtr curVCPUInfo, int vCPUCnt) {
  int from = -1, to = -1;
  unsigned long long curMax = 0;
  unsigned long long curMin = -1;
  for (int i = 0; i < pCPUCnt; i++) {
    fprintf(stdout, "pCPU %d - pCPUTimeDelta %llu \n", i, pCPUStats[i].CPUTimeDelta);
    if (pCPUStats[i].CPUTimeDelta > curMax) {
      curMax = pCPUStats[i].CPUTimeDelta;
      from = i;
    }
    if (curMin == -1 || pCPUStats[i].CPUTimeDelta < curMin) {
      curMin = pCPUStats[i].CPUTimeDelta;
      to = i;
    }
  }

  if (pCPUStats[from].domainIdCnt == 0) {
    fprintf(stderr, "No domain id pinned to pCPU %d \n", from);
    exit(1);
  };

  int id = pCPUStats[from].domainIds[pCPUStats[from].domainIdCnt - 1];
  fprintf(stdout, "looking for domain info... \n");
  int index = findById(curVCPUInfo, vCPUCnt, id);
  fprintf(stdout, "moving workload of size: %llu, domain id: %d, from pCPU: %d, to pCPU: %d \n",
    curVCPUInfo[index].CPUTimeDelta, curVCPUInfo[index].domainID, from, to);

  pCPUStats[to].CPUTimeDelta += curVCPUInfo[index].CPUTimeDelta;
  pCPUStats[to].domainIdCnt++;
  pCPUStats[to].domainIds[pCPUStats[to].domainIdCnt - 1] = id;
  pCPUStats[from].CPUTimeDelta -= curVCPUInfo[index].CPUTimeDelta;
  pCPUStats[from].domainIdCnt--;

  fprintf(stdout, "done moving workflow of domain id: %d, from pCPU: %d, to pCPU: %d \n",
    pCPUStats[to].domainIds[pCPUStats[to].domainIdCnt - 1], from, to);

  return 0;
}

int rebalanceBySorting(pCPUStatsPtr pCPUStats, int pCPUCnt, vCPUStatsPtr curVCPUInfo, int vCPUCnt) {
  qsort((void *)curVCPUInfo, vCPUCnt, sizeof(struct vCPUStats), comparator);

  for (int i = 0; i < vCPUCnt; i++) fprintf(stdout, "%llu ", curVCPUInfo[i].CPUTimeDelta);
  fprintf(stdout, "\n");

  int k = 0;
  for (int i = 0, j = vCPUCnt - 1; i <= j; i++, j--) {
    pCPUStats[k].CPUTimeDelta += curVCPUInfo[i].CPUTimeDelta;
    pCPUStats[k].domainIdCnt++;
    pCPUStats[k].domainIds[pCPUStats[k].domainIdCnt - 1] = curVCPUInfo[i].domainID;
    if (i != j) {
      pCPUStats[k].CPUTimeDelta += curVCPUInfo[j].CPUTimeDelta;
      pCPUStats[k].domainIdCnt++;
      pCPUStats[k].domainIds[pCPUStats[k].domainIdCnt - 1] = curVCPUInfo[j].domainID;
    }
    k++;
    k %= PCPU_CNT;
  }

  return 0;
}

int repin(virConnectPtr conn, pCPUStatsPtr curPCPUStats, pCPUStatsPtr prevPCPUStats, int pCPUCnt) {
  for (int i = 0; i < pCPUCnt; i++) {
    unsigned char pCPU = 0x1 << i;
    if(arraycmp(curPCPUStats[i].domainIds, curPCPUStats[i].domainIdCnt,
      prevPCPUStats[i].domainIds, prevPCPUStats[i].domainIdCnt) == 0) {
        fprintf(stdout, "No need to repining, skipping ... \n");
        continue;
      }
    for (int j = 0; j < curPCPUStats[i].domainIdCnt; j++) {
      fprintf(stdout, "Repining domain %d to pCPU %d with cpuamp %d ... \n", curPCPUStats[i].domainIds[j], i, pCPU);
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
  virConnectPtr conn;
  conn = virConnectOpen("qemu:///system");
  if (conn == NULL) {
    fprintf(stderr, "Failed to open connection to hypervisor\n");
    exit(1);
  }

  int domainCnt = virConnectNumOfDomains(conn);
  vCPUStatsPtr curVCPUStats = malloc(sizeof(struct pCPUStats) * domainCnt);
  for (int i = 0; i < domainCnt; i++) {
    curVCPUStats[i].domainID = -1;
    curVCPUStats[i].cpuTime = 0;
    curVCPUStats[i].CPUTimeDelta = 0;
  }
  vCPUStatsPtr prevVCPUStats = malloc(sizeof(struct pCPUStats) * domainCnt);
  memcpy(prevVCPUStats, curVCPUStats, sizeof(struct vCPUStats) * domainCnt);

  pCPUStatsPtr curPCPUStats = malloc(sizeof(struct pCPUStats) * PCPU_CNT);
  pCPUStatsPtr prePCPUStats = malloc(sizeof(struct pCPUStats) * PCPU_CNT);
  for (int i = 0; i < PCPU_CNT; i++) {
    curPCPUStats[i].CPUTimeDelta = 0;
    curPCPUStats[i].domainIds = malloc(sizeof(int) * domainCnt);
    curPCPUStats[i].domainIdCnt = 0;
  }
  memcpy(prePCPUStats, curPCPUStats, PCPU_CNT * sizeof(struct pCPUStats));

  while(domainCnt > 0) {
    int *activeDomains = malloc(sizeof(int) * domainCnt);
    virConnectListDomains(conn, activeDomains, domainCnt);

    fprintf(stdout, "Sampling pCPU stats...\n");
    sampleDomainInfo(conn, domainCnt, activeDomains, curPCPUStats, prevVCPUStats, curVCPUStats);

    unsigned long long averageDelta = 0;
    for (int i = 0; i < PCPU_CNT; i++) averageDelta += curPCPUStats[i].CPUTimeDelta;
    averageDelta /= 4;
    int rebalanceNeeded = 0;
    for (int i = 0; i < PCPU_CNT; i++) {
      if (abs(curPCPUStats[i].CPUTimeDelta - averageDelta) > 0.1 * averageDelta) {
        rebalanceNeeded = 1;
        break;
      }
    }

    if (rebalanceNeeded == 1) {
      fprintf(stdout, "Running rebalance algorithm...\n");
      // rebalance(curPCPUStats, PCPU_CNT, curVCPUStats, domainCnt);
      rebalanceBySorting(curPCPUStats, PCPU_CNT, curVCPUStats, domainCnt);

      fprintf(stdout, "Repinning vCPUs...\n");
      repin(conn, curPCPUStats, prePCPUStats, PCPU_CNT);
    } else {
      fprintf(stdout, "Already balance, no action ... \n");
    }

    memcpy(prePCPUStats, curPCPUStats, PCPU_CNT * sizeof(struct pCPUStats));
    for (int i = 0; i < PCPU_CNT; i++) {
      curPCPUStats[i].CPUTimeDelta = 0;
      curPCPUStats[i].domainIds = malloc(sizeof(int) * domainCnt);
      curPCPUStats[i].domainIdCnt = 0;
    }
    sleep(interval);
  }

  virConnectClose(conn);
  free(curVCPUStats);
  free(prevVCPUStats);
  if (curPCPUStats != NULL) free(curPCPUStats);
  if (prePCPUStats != NULL) free(prePCPUStats);
  return 0;
}
