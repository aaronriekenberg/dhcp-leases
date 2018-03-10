#include "oui.h"
#include <arpa/inet.h>
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

struct DhcpdLease {
  in_addr_t ip;
  time_t startTime;
  time_t endTime;
  char* mac;
  char* hostname;
  bool abandoned;
  RBT_ENTRY(DhcpdLease) entry;
};

static void freeDhcpdLease(
  struct DhcpdLease* dhcpdLease) {
  if (dhcpdLease != NULL) {
    free(dhcpdLease->mac);
    free(dhcpdLease->hostname);
    free(dhcpdLease);
  }
}

static inline int compareDhcpdLease(
  const struct DhcpdLease* d1,
  const struct DhcpdLease* d2) {
  if (d1->ip < d2->ip) {
    return -1;
  } else if (d1->ip == d2->ip) {
    return 0;
  } else {
    return 1;
  }
}

RBT_HEAD(DhcpdLeaseTree, DhcpdLease);

RBT_PROTOTYPE(DhcpdLeaseTree, DhcpdLease, entry, compareDhcpdLease);
RBT_GENERATE(DhcpdLeaseTree, DhcpdLease, entry, compareDhcpdLease);

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

static const size_t MAX_TOKENS = 4;

struct DhcpdLeaseTree* readDhcpdLeasesFile() {
  const char* fileName = "/var/db/dhcpd.leases";
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* currentDhcpdLease = NULL;
  FILE* dhcpdLeasesFile;
  char* line = NULL;
  size_t lineCapacity = 0;
  ssize_t lineLength;
  int error;

  dhcpdLeaseTree = checkedMalloc(sizeof(struct DhcpdLeaseTree));
  RBT_INIT(DhcpdLeaseTree, dhcpdLeaseTree);

  printf("reading %s\n", fileName);
  dhcpdLeasesFile = fopen(fileName, "r");

  if (dhcpdLeasesFile == NULL) {
    printf("failed to open %s errno %d: %s", 
           fileName, errno, strerror(errno));
    return dhcpdLeaseTree;
  }

  while ((lineLength = getline(&line, &lineCapacity, dhcpdLeasesFile)) != -1) {
    size_t i;
    char** token;
    char* tokens[MAX_TOKENS + 1];
    size_t numTokens = 0;

    /* kill \r and \n */
    while ((lineLength >= 1) &&
           ((line[lineLength - 1] == '\r') ||
            (line[lineLength - 1] == '\n'))) {
      line[lineLength - 1] = '\0';
      lineLength -= 1;
    }

    for (token = tokens; token < &(tokens[MAX_TOKENS]) &&
                         ((*token) = strsep(&line, " \t")) != NULL;) {
      if ((**token) != '\0') {
        token++;
      }
    }
    (*token) = NULL;

    for (i = 0; i < (MAX_TOKENS + 1); ++i) {
      if (tokens[i] == NULL) {
        numTokens = i;
        break;
      }
    }

    if (currentDhcpdLease == NULL) {

      if ((numTokens >= 2) &&
          (strcmp(tokens[0], "lease") == 0) &&
          (strcmp(tokens[2], "{") == 0)) {
        currentDhcpdLease = checkedCallocOne(sizeof(struct DhcpdLease));
        currentDhcpdLease->ip = inet_addr(tokens[1]);
      }

    } 

    else {

      if ((numTokens >= 1) &&
          (strcmp(tokens[0], "}") == 0)) {
        struct DhcpdLease* tmpDhcpdLease =
          RBT_INSERT(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
        if (tmpDhcpdLease != NULL) {
          if (currentDhcpdLease->endTime >= tmpDhcpdLease->endTime) {
            RBT_REMOVE(DhcpdLeaseTree, dhcpdLeaseTree, tmpDhcpdLease);
            freeDhcpdLease(tmpDhcpdLease);
            tmpDhcpdLease = NULL;
            RBT_INSERT(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
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

      else if ((numTokens >= 1) &&
               (strcmp(tokens[0], "abandoned;") == 0)) {
        currentDhcpdLease->abandoned = true;
      }

    }
  }

  if ((error = ferror(dhcpdLeasesFile)) != 0) {
    printf("error reading dhcpd leases file %s errno %d: %s", 
           fileName, error, strerror(error));
  }

  free(line);
  line = NULL;

  if ((error = fclose(dhcpdLeasesFile)) != 0) {
    printf("error closing dhcpd leases file %s errno %d: %s", 
           fileName, error, strerror(error));
  }

  if (currentDhcpdLease != NULL) {
    freeDhcpdLease(currentDhcpdLease);
    currentDhcpdLease = NULL;
  }

  return dhcpdLeaseTree;
}

static const char* getDhcpdLeaseState(
  const struct DhcpdLease* dhcpdLease,
  const time_t* now) {
  if (dhcpdLease->abandoned) {
    return "Abandoned";
  } else if ((*now) < (dhcpdLease->startTime)) {
    return "Future";
  } else if (((*now) >= (dhcpdLease->startTime)) &&
             ((dhcpdLease->endTime) >= (*now))) {
    return "Current";
  } else {
    return "Past";
  }
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  DB* db;
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* dhcpdLease;
  time_t now;
  size_t numLeases = 0;

  if (pledge("stdio flock rpath", NULL) == -1) {
    printf("pledge error %d: %s\n", errno, strerror(errno));
    return 1;
  }

  db = dbopen(dbFileName, O_SHLOCK|O_RDONLY, 0600, DB_BTREE, NULL);
  if (db == NULL) {
    printf("dbopen error %s errno %d: %s\n", dbFileName, errno, strerror(errno));
    return 1;
  }

  printf("dbFileName = %s\n", dbFileName);

  dhcpdLeaseTree = readDhcpdLeasesFile();

  now = time(NULL);

  printf("\n%-18s%-11s%-28s%-20s%-24s%s\n", "IP", "State", "End Time", "MAC", "Hostname", "Organization");
  printf("===============================================================================================================================\n");

  RBT_FOREACH(dhcpdLease, DhcpdLeaseTree, dhcpdLeaseTree) {
    struct in_addr ipAddressAddr;
    struct tm* tm;
    char timeBuffer[80];
    const char* organization = NULL;

    ++numLeases;

    ipAddressAddr.s_addr = dhcpdLease->ip;
    printf("%-18s", inet_ntoa(ipAddressAddr));

    printf("%-11s", getDhcpdLeaseState(dhcpdLease, &now));

    if ((dhcpdLease->endTime != 0) &&
        ((tm = localtime(&(dhcpdLease->endTime))) != NULL) &&
        (strftime(timeBuffer, 80, "%Y/%m/%d %H:%M:%S %z", tm) != 0)) {
      printf("%-28s", timeBuffer);
    } else {
      printf("%-28s", "NA");
    }

    if (dhcpdLease->mac != NULL) {
      printf("%-20s", dhcpdLease->mac);
    } else {
      printf("%-20s", "NA");
    }

    if (dhcpdLease->hostname != NULL) {
      printf("%-24.23s", dhcpdLease->hostname);
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
    printf("db->close error errno %d: %s\n", errno, strerror(errno));
  }

  return 0;
}
