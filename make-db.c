#include "oui.h"
#include <ctype.h>
#include <db.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

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
    err(1, "open %s", fileName);
    return;
  }

  while ((lineLength = getline(&line, &lineCapacity, ouiFile)) != -1) {
    Oui oui;

    /* kill \r and \n */
    while ((lineLength >= 1) &&
           ((line[lineLength - 1] == '\r') ||
            (line[lineLength - 1] == '\n'))) {
      line[lineLength - 1] = '\0';
      --lineLength;
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
      char* organization = line + 22;

      key.data = &oui;
      key.size = sizeof(oui);

      value.data = organization;
      value.size = strlen(organization) + 1;

      ++totalRecords;

      if (db->put(db, &key, &value, 0) != 0) {
        warn("db->put");
      } else {
        ++recordsWritten;
      }
    }
  }

  if ((error = ferror(ouiFile)) != 0) {
    warn("ferror %s", fileName);
  }

  if ((error = fclose(ouiFile)) != 0) {
    warn("fclose %s", fileName);
  }

  free(line);
  line = NULL;

  printf("totalRecords = %zu recordsWritten = %zu\n", totalRecords, recordsWritten);
}

static void setMallocOptions() {
  extern char* malloc_options;
  malloc_options = "X";
}

int main(int argc, char** argv) {
  const char* dbFileName = "oui.db";
  DB* db;

  setMallocOptions();

  if (pledge("stdio flock cpath rpath wpath", NULL) == -1) {
    err(1, "pledge");
  }

  printf("dbFileName = %s\n", dbFileName);

  db = dbopen(dbFileName, O_CREAT|O_TRUNC|O_EXLOCK|O_RDWR, 0600, DB_BTREE, NULL);
  if (db == NULL) {
    err(1, "dbopen %s", dbFileName);
  }

  readOuiFile(db);

  if (db->close(db) != 0) {
    warn("db->close");
  }

  return 0;
}
