#include "oui.h"
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/tree.h>
#include <sys/types.h>
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
    char** token;
    char* tokens[MAX_TOKENS];
    size_t numTokens = 0;

    /* kill \r and \n */
    while ((lineLength >= 1) &&
           ((line[lineLength - 1] == '\r') ||
            (line[lineLength - 1] == '\n'))) {
      line[lineLength - 1] = '\0';
      lineLength -= 1;
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
               (strcmp(tokens[0], "client-hostname") == 0)) {
        const size_t hostnameLength = strlen(tokens[1]);
        if (hostnameLength > 3) {
          tokens[1][hostnameLength - 1] = '\0';
          tokens[1][hostnameLength - 2] = '\0';
          currentDhcpdLease->hostname = strdup(&(tokens[1][1]));
        }
      }

    }
  }

  if ((error = ferror(dhcpdLeasesFile)) != 0) {
    printf("error reading dhcpd leases file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  free(line);
  line = NULL;

  if ((error = fclose(dhcpdLeasesFile)) != 0) {
    printf("error closing dhcpd leases file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  if (currentDhcpdLease != NULL) {
    freeDhcpdLease(currentDhcpdLease);
    currentDhcpdLease = NULL;
  }

  return dhcpdLeaseTree;
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* dhcpdLease;
  size_t numLeases = 0;
  DB* db;
  BTREEINFO btreeinfo;

  dhcpdLeaseTree = readDhcpdLeasesFile();

  memset(&btreeinfo, 0, sizeof(btreeinfo));
  db = dbopen(dbFileName, O_SHLOCK|O_RDONLY, 0600, DB_BTREE, &btreeinfo);
  if (db == NULL) {
    printf("dbopen error %s errno %d: %s\n", dbFileName, errno, errnoToString(errno));
    return 1;
  }

  printf("dbFileName = %s\n", dbFileName);

  printf("\n%-18s%-28s%-20s%-24s%s\n", "IP", "End Time", "MAC", "Hostname", "Organization");
  printf("====================================================================================================================\n");

  RB_FOREACH(dhcpdLease, DhcpdLeaseTree, dhcpdLeaseTree) {
    char buffer[80];
    struct tm* tm;
    const char* organization = NULL;

    ++numLeases;

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
        DBT key, value;
        Oui oui = (byte1 << 16) | (byte2 << 8) | byte3;
        key.data = &oui;
        key.size = sizeof(oui);
        memset(&value, 0, sizeof(value));
        if (db->get(db, &key, &value, 0) == 0) {
          organization = value.data;
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

  printf("\n%zu IPs in use\n", numLeases);

  if (db->close(db) != 0) {
    printf("db->close error errno %d: %s\n", errno, errnoToString(errno));
  }

  return 0;
}
