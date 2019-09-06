#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  // get connection to hypervisor
  virConnectPtr conn;
  conn = virConnectOpen("qemu:///system");
  if (conn == NULL) {
    fprintf(stderr, "Failed to open connection to hypervisor\n");
    exit(1);
  }

  // get all active running virtual machines
  int domainCnt = virConnectNumOfDomains(conn);
  int *activeDomains = malloc(sizeof(int) * domainCnt);
  domainCnt = virConnectListDomains(conn, activeDomains, domainCnt);
  if (domainCnt == -1) {
    fprintf(stderr, "Failed to get domains");
    exit(1);
  }
  for (int i = 0; i < domainCnt; i++) {
    fprintf(stdout, "guest domain: %d\n", activeDomains[i]);
  }

  // launch VM and see how vCUP is associated with pCUP
  virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
  virDomainPtr domainPtr = virDomainLookupByID(conn, activeDomains[0]);
  if (virDomainGetInfo(domainPtr, domainInfo) == -1) {
    fprintf(stderr, "Failed to get domain info");
    exit(1);
  }

  virVcpuInfoPtr vcupInfo = malloc(sizeof(virVcpuInfo) * domainInfo->nrVirtCpu);
  virDomainGetVcpus(domainPtr, vcupInfo, domainInfo->nrVirtCpu, NULL, 0);

  for (int j = 0; j < 5; j++) {
    for (int i = 0; i < domainInfo->nrVirtCpu; i++) {
      fprintf(stderr, "%s\n", vcupInfo[i]->number);
      fprintf(stderr, "%s\n", vcupInfo[i]->cpu);
      fprintf(stderr, "%llu\n", vcupInfo[i]->cpuTime);
    }
    sleep(5);
  }

  virConnectClose(conn);
  return 0;
}
