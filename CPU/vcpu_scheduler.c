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
    // get domain cpu info
    virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
    virDomainPtr domainPtr = virDomainLookupByID(conn, activeDomains[i]);
    if (virDomainGetInfo(domainPtr, domainInfo) == -1) {
      fprintf(stderr, "Failed to get domain info");
      exit(1);
    }

    fprintf(stdout, "guest domain: %d, %s\n", activeDomains[i], virDomainGetName(domainPtr));

    virVcpuInfoPtr vcupInfo = malloc(sizeof(virVcpuInfo) * domainInfo->nrVirtCpu);
    virDomainGetVcpus(domainPtr, vcupInfo, domainInfo->nrVirtCpu, NULL, 0);

    fprintf(stdout, "vCPU cnt: %d\n", domainInfo->nrVirtCpu);
    for (int j = 0; j < 5; j++) {
      for (int i = 0; i < domainInfo->nrVirtCpu; i++) {
        fprintf(stderr, "%d\n", vcupInfo[i].number);
        fprintf(stderr, "%d\n", vcupInfo[i].cpu);
        fprintf(stderr, "%llu\n", vcupInfo[i].cpuTime);
      }
      sleep(5);
    }
  }

  virConnectClose(conn);
  return 0;
}
