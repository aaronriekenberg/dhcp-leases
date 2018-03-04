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

struct OuiAndOrganization {
  Oui oui;
  char* organization;
  RB_ENTRY(OuiAndOrganization) entry;
};

static void freeOuiAndOrganization(struct OuiAndOrganization* ouiAndOrganization) {
  if (ouiAndOrganization != NULL) {
    free(ouiAndOrganization->organization);
    free(ouiAndOrganization);
  }
}

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
  const char* fileName = "oui.txt";
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
    Oui oui;

    /* kill cr and newline */
    if (lineLength >= 2) {
      line[lineLength - 1] = '\0';
      line[lineLength - 2] = '\0';
      lineLength -= 2;
    }

    if ((lineLength < 23) ||
        (line[0] == '\t') ||
        (line[2] == '-')) {
      continue;
    }

    line[6] = '\0';
    if (sscanf(line, "%x", &oui) == 1) {
      struct OuiAndOrganization* ouiAndOrganization;

      ouiAndOrganization = checkedMalloc(sizeof(struct OuiAndOrganization));
      ouiAndOrganization->oui = oui;
      ouiAndOrganization->organization = strdup(&(line[22]));

      if (RB_INSERT(OuiAndOrganizationTree, ouiAndOrganizationTree, ouiAndOrganization) != NULL) {
        freeOuiAndOrganization(ouiAndOrganization);
        ouiAndOrganization = NULL;
      }
    }
  }

  if ((error = ferror(ouiFile)) != 0) {
    printf("error reading oui file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  free(line);
  line = NULL;

  if ((error = fclose(ouiFile)) != 0) {
    printf("error closing oui file %s errno %d: %s", 
           fileName, error, errnoToString(error));
  }

  return ouiAndOrganizationTree;
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  struct OuiAndOrganizationTree* ouiAndOrganizationTree;
  struct OuiAndOrganization* ouiAndOrganization;
  DB* db;
  BTREEINFO btreeinfo;
  DBT key, value;
  size_t totalRecords = 0, recordsWritten = 0;

  ouiAndOrganizationTree = readOuiFile();

  printf("dbFileName = %s", dbFileName);

  memset(&btreeinfo, 0, sizeof(btreeinfo));
  db = dbopen(dbFileName, O_CREAT|O_TRUNC|O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
  if (db == NULL) {
    printf("dbopen error %s errno %d: %s\n", dbFileName, errno, errnoToString(errno));
    return 1;
  }

  RB_FOREACH(ouiAndOrganization, OuiAndOrganizationTree, ouiAndOrganizationTree) {
    key.data = &(ouiAndOrganization->oui);
    key.size = sizeof(ouiAndOrganization->oui);
    value.data = ouiAndOrganization->organization;
    value.size = strlen(ouiAndOrganization->organization) + 1;

    ++totalRecords;

    if (db->put(db, &key, &value, 0) != 0) {
      printf("db->put error errno %d: %s\n", errno, errnoToString(errno));
    } else {
      ++recordsWritten;
    }
  }

  if (db->close(db) != 0) {
      printf("db->close error errno %d: %s\n", errno, errnoToString(errno));
  }

  printf("totalRecords = %zu recordsWritten = %zu\n", totalRecords, recordsWritten);

  return 0;
}
