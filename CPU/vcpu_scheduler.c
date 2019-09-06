#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[]) {
  // get connection to hypervisor
  virConnectPtr conn;
  conn = virConnectOpen("qemu://system");
  if (conn == NULL) {
    fprintf(stderr, "Failed to open connection to hypervisor\n");
    exit(1);
  }

  virConnectClose(virConnectPtr);

  // get all active running virtual machines
  int domainCnt = virConnectNumOfDomains(conn);
  int *activeDomains = malloc(sizeof(int) * domainCnt);
  int res = virConnectListDomains(conn, domainCnt, activeDomains);

  // launch VM and see how vCUP is associated with pCUP
  virDomainInfoPtr domainInfo = malloc(sizeof(virDomainInfo));
  if (virDomainGetInfo(activeDomains[0], domainInfo) != 0) {
    fprintf(stderr, "Failed to get domain info");
    exit(1);
  }

  virVcpuInfoPtr vcupInfo =malloc(sizeof(virVcpuInfo) * domainInfo.vcpus_count);
  virDomainGetVcpus(activeDomains[0], vcupInfo, domainInfo.vcpus_count, NULL, 0);
  return 0;
}
