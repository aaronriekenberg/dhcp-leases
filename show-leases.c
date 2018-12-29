#include "oui.h"
#include <arpa/inet.h>
#include <db.h>
#include <err.h>
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
  uint32_t numRecords;
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

static void copyDhcpdLease(
  const struct DhcpdLease* from,
  struct DhcpdLease* to) {
  to->ip = from->ip;
  to->numRecords = from->numRecords;
  to->startTime = from->startTime;
  to->endTime = from->endTime;

  free(to->mac);
  to->mac = NULL;
  if (from->mac != NULL) {
    to->mac = strdup(from->mac);
  }

  free(to->hostname);
  to->hostname = NULL;
  if (from->hostname != NULL) {
    to->hostname = strdup(from->hostname);
  }

  to->abandoned = from->abandoned;
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

static const size_t MAX_TOKENS = 4;

static struct DhcpdLeaseTree* readDhcpdLeasesFile() {
  const char* fileName = "/var/db/dhcpd.leases";
  struct DhcpdLeaseTree* dhcpdLeaseTree;
  struct DhcpdLease* currentDhcpdLease = NULL;
  FILE* dhcpdLeasesFile;
  char* line = NULL;
  size_t lineCapacity = 0;
  ssize_t lineLength;
  int error;

  dhcpdLeaseTree = malloc(sizeof(struct DhcpdLeaseTree));
  RBT_INIT(DhcpdLeaseTree, dhcpdLeaseTree);

  printf("reading %s\n", fileName);
  dhcpdLeasesFile = fopen(fileName, "r");

  if (dhcpdLeasesFile == NULL) {
    warn("open %s", fileName);
    return dhcpdLeaseTree;
  }

  while ((lineLength = getline(&line, &lineCapacity, dhcpdLeasesFile)) != -1) {
    size_t i;
    char** token;
    char* tokens[MAX_TOKENS + 1];
    size_t numTokens;

    /* kill \r and \n */
    while ((lineLength >= 1) &&
           ((line[lineLength - 1] == '\r') ||
            (line[lineLength - 1] == '\n'))) {
      line[lineLength - 1] = '\0';
      --lineLength;
    }

    for (token = tokens;
         (token < (tokens + MAX_TOKENS)) &&
         (((*token) = strsep(&line, " \t")) != NULL);) {
      if ((**token) != '\0') {
        ++token;
      }
    }
    (*token) = NULL;

    numTokens = 0;
    for (i = 0; i < (MAX_TOKENS + 1); ++i) {
      if (tokens[i] == NULL) {
        numTokens = i;
        break;
      }
    }

    if (currentDhcpdLease == NULL) {

      if ((numTokens >= 3) &&
          (strcmp(tokens[0], "lease") == 0) &&
          (strcmp(tokens[2], "{") == 0)) {
        currentDhcpdLease = calloc(1, sizeof(struct DhcpdLease));
        currentDhcpdLease->ip = inet_addr(tokens[1]);
        currentDhcpdLease->numRecords = 1;
      }

    } 

    else {

      if ((numTokens >= 1) &&
          (strcmp(tokens[0], "}") == 0)) {
        struct DhcpdLease* otherLeaseForIP =
          RBT_INSERT(DhcpdLeaseTree, dhcpdLeaseTree, currentDhcpdLease);
        if (otherLeaseForIP != NULL) {
          const uint32_t totalNumRecords =
            currentDhcpdLease->numRecords + otherLeaseForIP->numRecords;
          if (currentDhcpdLease->endTime >= otherLeaseForIP->endTime) {
            copyDhcpdLease(currentDhcpdLease, otherLeaseForIP);
          }
          otherLeaseForIP->numRecords = totalNumRecords;
          freeDhcpdLease(currentDhcpdLease);
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
        free(currentDhcpdLease->mac);
        currentDhcpdLease->mac = strdup(tokens[2]);
      }

      else if ((numTokens >= 2) &&
               (strcmp(tokens[0], "client-hostname") == 0)) {
        const size_t hostnameLength = strlen(tokens[1]);
        if (hostnameLength > 3) {
          tokens[1][hostnameLength - 1] = '\0';
          tokens[1][hostnameLength - 2] = '\0';
          free(currentDhcpdLease->hostname);
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
    warn("ferror %s", fileName);
  }

  if ((error = fclose(dhcpdLeasesFile)) != 0) {
    warn("fclose %s", fileName);
  }

  free(line);
  line = NULL;

  freeDhcpdLease(currentDhcpdLease);
  currentDhcpdLease = NULL;

  return dhcpdLeaseTree;
}

enum DhcpdLeaseState {
  DHCPD_LEASE_STATE_ABANDONED,
  DHCPD_LEASE_STATE_FUTURE,
  DHCPD_LEASE_STATE_CURRENT,
  DHCPD_LEASE_STATE_PAST,
  DHCPD_LEASE_STATE_NUM_STATES
};

static const char* dhcpdLeaseStateString[] = {
  "Abandoned",
  "Future",
  "Current",
  "Past"
};

static enum DhcpdLeaseState getDhcpdLeaseState(
  const struct DhcpdLease* dhcpdLease,
  const time_t now) {
  if (dhcpdLease->abandoned) {
    return DHCPD_LEASE_STATE_ABANDONED;
  } else if (now < (dhcpdLease->startTime)) {
    return DHCPD_LEASE_STATE_FUTURE;
  } else if (((dhcpdLease->startTime) <= now) &&
             (now <= (dhcpdLease->endTime))) {
    return DHCPD_LEASE_STATE_CURRENT;
  } else {
    return DHCPD_LEASE_STATE_PAST;
  }
}

static void printLeases(struct DhcpdLeaseTree* dhcpdLeaseTree, DB* db) {
  struct DhcpdLease* dhcpdLease;
  time_t now;
  size_t totalLeases;
  size_t dhcpdLeaseStateCount[DHCPD_LEASE_STATE_NUM_STATES];
  int i;

  now = time(NULL);

  printf("\n%-18s%-11s%-6s%-28s%-20s%-24s%s\n", "IP", "State", "Num", "End Time", "MAC", "Hostname", "Organization");
  for (i = 0; i < 140; ++i) {
    putchar('=');
  }
  putchar('\n');

  totalLeases = 0;
  memset(dhcpdLeaseStateCount, 0, sizeof(dhcpdLeaseStateCount));

  RBT_FOREACH(dhcpdLease, DhcpdLeaseTree, dhcpdLeaseTree) {
    struct in_addr ipAddressAddr;
    enum DhcpdLeaseState dhcpdLeaseState;
    struct tm* tm;
    char timeBuffer[80];
    const char* organization;

    ++totalLeases;

    ipAddressAddr.s_addr = dhcpdLease->ip;
    printf("%-18s", inet_ntoa(ipAddressAddr));

    dhcpdLeaseState = getDhcpdLeaseState(dhcpdLease, now);
    ++(dhcpdLeaseStateCount[dhcpdLeaseState]);
    printf("%-11s", dhcpdLeaseStateString[dhcpdLeaseState]);

    printf("%-6u", dhcpdLease->numRecords);

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

    organization = NULL;
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
    putchar('\n');
  }

  printf("\n%zu leases with unique IPs:\n", totalLeases);
  for (i = 0; i < DHCPD_LEASE_STATE_NUM_STATES; ++i) {
    printf("\t%zu %s\n", dhcpdLeaseStateCount[i], dhcpdLeaseStateString[i]);
  }
}

static void setMallocOptions() {
  extern char* malloc_options;
  malloc_options = "X";
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  DB* db;
  struct DhcpdLeaseTree* dhcpdLeaseTree;

  setMallocOptions();

  if (pledge("stdio flock rpath", NULL) == -1) {
    err(1, "pledge");
  }

  db = dbopen(dbFileName, O_SHLOCK|O_RDONLY, 0600, DB_BTREE, NULL);
  if (db == NULL) {
    err(1, "dbopen %s", dbFileName);
  }

  printf("dbFileName = %s\n", dbFileName);

  dhcpdLeaseTree = readDhcpdLeasesFile();

  printLeases(dhcpdLeaseTree, db);

  if (db->close(db) != 0) {
    warn("db->close");
  }

  return 0;
}
