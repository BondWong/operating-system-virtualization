#include <string.h>
#include "utils.h"

char *strrev(char *str) {
  char *p1, *p2;
  if (!str || ! *str) return str;
  for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2) {
    *p1 ^= *p2;
    *p2 ^= *p1;
    *p1 ^= *p2;
  }
  return str;
}

int arraycmp(int* arr1, int size1, int* arr2, int size2) {
  if (size1 <= 0 || size2 <= 0) return -1;
  if (size1 != size2) return -1;

  return memcpy(arr1, arr2, size1);
}
