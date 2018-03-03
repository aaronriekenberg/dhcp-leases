#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <time.h>

struct DhcpdLease {
  char* ip;
  time_t startTime;
  time_t endTime;
  char* mac;
  char* hostname;
  RB_ENTRY(DhcpdLease) entry;
};

static void freeDhcpdLease(
  struct DhcpdLease* dhcpdLease) {
  if (dhcpdLease != NULL) {
    free(dhcpdLease->ip);
    free(dhcpdLease->mac);
    free(dhcpdLease->hostname);
    free(dhcpdLease);
  }
}

#if 0
static void printDhcpdLease(
  const struct DhcpdLease* dhcpdLease) {
  printf("dhcpdLease = %p\n", dhcpdLease);
  if (dhcpdLease != NULL) {
    if (dhcpdLease->ip == NULL) {
      printf("dhcpdLease->ip = NULL\n");
    } else {
      printf("dhcpdLease->ip = '%s'\n", dhcpdLease->ip);
    }

    printf("dhcpdLease->startTime = %lld\n", dhcpdLease->startTime);

    printf("dhcpdLease->endTime = %lld\n", dhcpdLease->endTime);

    if (dhcpdLease->mac == NULL) {
      printf("dhcpdLease->mac = NULL\n");
    } else {
      printf("dhcpdLease->mac = '%s'\n", dhcpdLease->mac);
    }

    if (dhcpdLease->hostname == NULL) {
      printf("dhcpdLease->hostname = NULL\n");
    } else {
      printf("dhcpdLease->hostname = '%s'\n", dhcpdLease->hostname);
    }

  }
}
#endif

static int compareDhcpdLease(
  const struct DhcpdLease* d1,
  const struct DhcpdLease* d2) {
  return strcmp(d1->ip, d2->ip);
}

RB_HEAD(DhcpdLeaseTree, DhcpdLease);

RB_GENERATE(DhcpdLeaseTree, DhcpdLease, entry, compareDhcpdLease)

typedef uint32_t Oui;

struct OuiAndOrganization {
  Oui oui;
  const char* organization;
  RB_ENTRY(OuiAndOrganization) entry;
};

static int compareOuiAndOrganization(
  const struct OuiAndOrganization* o1,
  const struct OuiAndOrganization* o2) {
  if (o1->oui < o2->oui) {
    return -1;
  } else if (o1->oui == o2->oui) {
    return 0;
  } else {
    return 1;
  }
}

RB_HEAD(OuiAndOrganizationTree, OuiAndOrganization);

RB_GENERATE(OuiAndOrganizationTree, OuiAndOrganization, entry, compareOuiAndOrganization)

static void* checkedCallocOne(
  const size_t size)
{
  void* retVal = calloc(1, size);
  if (retVal == NULL)
  {
    printf("calloc failed nmemb %u size %zu\n",
           1, size);
    abort();
  }
  return retVal;
}

static void* checkedMalloc(
  const size_t size)
{
  void* retVal = malloc(size);
  if (retVal == NULL)
  {
    printf("malloc failed size %zu\n", size);
    abort();
  }
  return retVal;
}

static const char* errnoToString(const int errnoToTranslate)
{
  const int previousErrno = errno;
  const char* errorString;

  errno = 0;
  errorString = strerror(errnoToTranslate);
  if (errno != 0)
  {
    printf("strerror error errnoToTranslate = %d errno = %d\n",
           errnoToTranslate, errno);
    abort();
  }

  errno = previousErrno;
  return errorString;
}

struct OuiAndOrganizationTree* readOuiFile() {
  const char* fileName = "/home/aaron/oui.txt";
  FILE* ouiFile;
  char* line = NULL;
  size_t lineCapacity = 0;
  ssize_t lineLength;
  struct OuiAndOrganizationTree* ouiAndOrganizationTree;
  int error;

  ouiAndOrganizationTree = checkedMalloc(sizeof(struct OuiAndOrganizationTree));
  RB_INIT(ouiAndOrganizationTree);

  printf("reading %s\n", fileName);
  ouiFile = fopen(fileName, "r");

  if (ouiFile == NULL) {
    printf("failed to open %s errno %d: %s", 
           fileName, errno, errnoToString(errno));
    return ouiAndOrganizationTree;
  }

  while ((lineLength = getline(&line, &lineCapacity, ouiFile)) != -1) {
    char ouiString[7];
    Oui oui;

    /* kill newline */
    if (lineLength > 0) {
      line[lineLength - 1] = '\0';
    }

    if ((lineLength < 24) ||
        (line[0] == '\t') ||
        (line[2] == '-')) {
      continue;
    }

    memcpy(ouiString, line, 6);
    ouiString[6] = '\0';

    if (sscanf(ouiString, "%x", &oui) == 1) {
      struct OuiAndOrganization* ouiAndOrganization = checkedMalloc(sizeof(struct OuiAndOrganization));
      ouiAndOrganization->oui = oui;
      ouiAndOrganization->organization = strdup(&(line[22]));
      RB_INSERT(OuiAndOrganizationTree, ouiAndOrganizationTree, ouiAndOrganization);
    }
  }

  if ((error = ferror(ouiFile)) != 0) {
    printf("error reading oui file %s errno %d: %s", 
           fileName, error, errnoToString(error));
    return ouiAndOrganizationTree;
  }

  free(line);
  line = NULL;

  if ((error = fclose(ouiFile)) != 0) {
    printf("error closing oui file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  return ouiAndOrganizationTree;
}

static const size_t MAX_TOKENS = 5;

struct DhcpdLeaseTree* readDhcpdLeasesFile() {
  const char* fileName = "/var/db/dhcpd.leases";
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* currentDhcpdLease = NULL;
  struct DhcpdLease* tmpDhcpdLease;
  FILE* dhcpdLeasesFile;
  char* line = NULL;
  size_t lineCapacity = 0;
  ssize_t lineLength;
  char** token;
  char* tokens[MAX_TOKENS];
  int error;

  dhcpdLeaseTree = checkedMalloc(sizeof(struct DhcpdLeaseTree));
  RB_INIT(dhcpdLeaseTree);

  printf("reading %s\n", fileName);
  dhcpdLeasesFile = fopen(fileName, "r");

  if (dhcpdLeasesFile == NULL) {
    printf("failed to open %s errno %d: %s", 
           fileName, errno, errnoToString(errno));
    return dhcpdLeaseTree;
  }

  while ((lineLength = getline(&line, &lineCapacity, dhcpdLeasesFile)) != -1) {
    size_t i;
    size_t numTokens = 0;

    /* kill newline */
    if (lineLength > 0) {
      line[lineLength - 1] = '\0';
    }

    for (token = tokens; token < &(tokens[MAX_TOKENS - 1]) && 
                         ((*token) = strsep(&line, " \t")) != NULL;) {
      if ((**token) != '\0') {
        token++;
      }
    }
    (*token) = NULL;

    for (i = 0; i < MAX_TOKENS; ++i) {
      if (tokens[i] == NULL) {
        numTokens = i;
        break;
      }
    }

    if (currentDhcpdLease == NULL) {

      if ((numTokens >= 2) &&
          (strcmp(tokens[0], "lease") == 0) &&
          (strcmp(tokens[2], "{") == 0)) {
        char* ip = strdup(tokens[1]);
        currentDhcpdLease = checkedCallocOne(sizeof(struct DhcpdLease));
        currentDhcpdLease->ip = ip;
      }

    } 

    else {

      if ((numTokens >= 1) &&
          (strcmp(tokens[0], "}") == 0)) {
        tmpDhcpdLease = RB_FIND(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
        if (tmpDhcpdLease == NULL) {
          RB_INSERT(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
        } else {
          if (currentDhcpdLease->endTime >= tmpDhcpdLease->endTime) {
            RB_REMOVE(DhcpdLeaseTree, dhcpdLeaseTree, tmpDhcpdLease);
            freeDhcpdLease(tmpDhcpdLease);
            tmpDhcpdLease = NULL;
            RB_INSERT(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
          } else {
            freeDhcpdLease(currentDhcpdLease);
          }
        }
        currentDhcpdLease = NULL;
      }

      else if ((numTokens >= 4) &&
               (strcmp(tokens[0], "starts") == 0)) {
        char timeBuffer[80];
        struct tm tm;
        if (snprintf(timeBuffer, 80, "%s %s", tokens[2], tokens[3]) < 80) {
          if (strptime(timeBuffer, "%Y/%m/%d %H:%M:%S", &tm) != NULL) {
            currentDhcpdLease->startTime = timegm(&tm);
          }
        }
      }

      else if ((numTokens >= 4) &&
               (strcmp(tokens[0], "ends") == 0)) {
        char timeBuffer[80];
        struct tm tm;
        if (snprintf(timeBuffer, 80, "%s %s", tokens[2], tokens[3]) < 80) {
          if (strptime(timeBuffer, "%Y/%m/%d %H:%M:%S", &tm) != NULL) {
            currentDhcpdLease->endTime = timegm(&tm);
          }
        }
      }

      else if ((numTokens >= 3) &&
               (strcmp(tokens[0], "hardware") == 0) &&
               (strcmp(tokens[1], "ethernet") == 0) &&
               (strlen(tokens[2]) == 18)) {
        tokens[2][17] = '\0';
        currentDhcpdLease->mac = strdup(tokens[2]);
      }

      else if ((numTokens >= 2) &&
               (strcmp(tokens[0], "client-hostname") == 0) &&
               (strlen(tokens[1]) > 3)) {
        const size_t hostnameLength = strlen(tokens[1]);
        tokens[1][hostnameLength - 1] = '\0';
        tokens[1][hostnameLength - 2] = '\0';
        currentDhcpdLease->hostname = strdup(&(tokens[1][1]));
      }

    }
  }

  if ((error = ferror(dhcpdLeasesFile)) != 0) {
    printf("error reading dhcpd leases file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  if (currentDhcpdLease != NULL) {
    freeDhcpdLease(currentDhcpdLease);
    currentDhcpdLease = NULL;
  }

  free(line);
  line = NULL;

  if ((error = fclose(dhcpdLeasesFile)) != 0) {
    printf("error closing dhcpd leases file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  return dhcpdLeaseTree;
}

int main(int argc, char** argv) {
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* dhcpdLease;
  struct OuiAndOrganizationTree* ouiAndOrganizationTree;

  dhcpdLeaseTree = readDhcpdLeasesFile();
  ouiAndOrganizationTree = readOuiFile();

  printf("%-18s%-28s%-20s%-24s%s\n", "IP", "End Time", "MAC", "Hostname", "Organization");
  printf("====================================================================================================================\n");

  RB_FOREACH(dhcpdLease, DhcpdLeaseTree, dhcpdLeaseTree) {
    char buffer[80];
    struct tm* tm;
    const char* organization = NULL;

    printf("%-18s", dhcpdLease->ip);    

    if ((dhcpdLease->endTime != 0) &&
        ((tm = localtime(&(dhcpdLease->endTime))) != NULL) &&
        (strftime(buffer, 80, "%Y/%m/%d %H:%M:%S %z", tm) != 0)) {
      printf("%-28s", buffer);
    } else {
      printf("%-28s", "NA");
    }

    if (dhcpdLease->mac != NULL) {
      printf("%-20s", dhcpdLease->mac);
    } else {
      printf("%-20s", "NA");
    }

    if (dhcpdLease->hostname != NULL) {
      printf("%-24s", dhcpdLease->hostname);
    } else {
      printf("%-24s", "NA");
    }

    if (dhcpdLease->mac != NULL) {
      uint8_t byte1, byte2, byte3;
      if (sscanf(dhcpdLease->mac, "%hhx:%hhx:%hhx", &byte1, &byte2, &byte3) == 3) {
        struct OuiAndOrganization entry;
        struct OuiAndOrganization* pEntry;
        entry.oui = (byte1 << 16) | (byte2 << 8) | byte3;
        pEntry = RB_FIND(OuiAndOrganizationTree, ouiAndOrganizationTree, &entry);
        if (pEntry != NULL) {
          organization = pEntry->organization;
        }
      }
    }
    if (organization != NULL) {
      printf("%s", organization);
    } else {
      printf("%s", "NA");
    }

    printf("\n");
  }

  return 0;
}
