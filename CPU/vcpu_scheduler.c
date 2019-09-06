#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

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

  fprintf(stdout, "%d guest domains", domainCnt);

  // launch VM and see how vCUP is associated with pCUP
  virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
  if (virDomainGetInfo(activeDomains[0], domainInfo) == -1) {
    fprintf(stderr, "Failed to get domain info");
    exit(1);
  }

  virVcpuInfoPtr vcupInfo = malloc(sizeof(virVcpuInfo) * domainInfo->nrVirtCpu);
  virDomainGetVcpus(activeDomains[0], vcupInfo, domainInfo->nrVirtCpu, NULL, 0);

  virConnectClose(conn);
  return 0;
}
