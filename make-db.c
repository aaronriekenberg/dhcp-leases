#include "oui.h"
#include <ctype.h>
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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

static void readOuiFile(DB* db) {
  const char* fileName = "oui.txt";
  FILE* ouiFile;
  char* line = NULL;
  size_t lineCapacity = 0;
  ssize_t lineLength;
  int error;
  size_t totalRecords = 0, recordsWritten = 0;

  printf("reading %s\n", fileName);
  ouiFile = fopen(fileName, "r");

  if (ouiFile == NULL) {
    printf("failed to open %s errno %d: %s", 
           fileName, errno, errnoToString(errno));
    return;
  }

  while ((lineLength = getline(&line, &lineCapacity, ouiFile)) != -1) {
    Oui oui;

    /* kill \r and \n */
    while ((lineLength >= 1) &&
           ((line[lineLength - 1] == '\r') ||
            (line[lineLength - 1] == '\n'))) {
      line[lineLength - 1] = '\0';
      lineLength -= 1;
    }

    if ((lineLength < 23) ||
        (!isxdigit(line[0])) ||
        (!isxdigit(line[1])) ||
        (!isxdigit(line[2])) ||
        (!isxdigit(line[3])) ||
        (!isxdigit(line[4])) ||
        (!isxdigit(line[5]))) {
      continue;
    }

    line[6] = '\0';
    if (sscanf(line, "%x", &oui) == 1) {
      DBT key, value;
      char* organization = &(line[22]);

      key.data = &oui;
      key.size = sizeof(oui);

      value.data = organization;
      value.size = strlen(organization) + 1;

      ++totalRecords;

      if (db->put(db, &key, &value, 0) != 0) {
        printf("db->put error errno %d: %s\n", errno, errnoToString(errno));
      } else {
        ++recordsWritten;
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

  printf("totalRecords = %zu recordsWritten = %zu\n", totalRecords, recordsWritten);
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  DB* db;

  printf("dbFileName = %s\n", dbFileName);

  db = dbopen(dbFileName, O_CREAT|O_TRUNC|O_EXLOCK|O_RDWR, 0600, DB_BTREE, NULL);
  if (db == NULL) {
    printf("dbopen error %s errno %d: %s\n", dbFileName, errno, errnoToString(errno));
    return 1;
  }

  readOuiFile(db);

  if (db->close(db) != 0) {
    printf("db->close error errno %d: %s\n", errno, errnoToString(errno));
  }

  return 0;
}
