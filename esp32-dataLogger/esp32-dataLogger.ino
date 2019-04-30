/*
   Connect the SD card to the following pins:

   SD Card | ESP32
      D2       12
      D3       13
      CMD      15
      VSS      GND
      VDD      3.3V
      CLK      14
      VSS      GND
      D0       2  (add 1K pull up after flashing)
      D1       4
*/

#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include "FS.h"
#include "SD_MMC.h"
#include <Wire.h>
#include "RTClib.h"

RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }


  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

char *zErrMsg = 0;
const char *data = "Callback function called";
static int callback(void *data, int argc, char **argv, char **azColName)
{
  int i;
  Serial.printf("%s: ", (const char *)data);
  for (i = 0; i < argc; i++)
  {
    Serial.printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
  }
  Serial.printf("\n");
  return 0;
}

int db_exec(sqlite3 *db, const char *sql)
{
  Serial.println(sql);
  long start = micros();
  int rc = sqlite3_exec(db, sql, callback, (void *)data, &zErrMsg);
  if (rc != SQLITE_OK)
  {
    Serial.printf("SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  else
  {
    Serial.printf("Operation done successfully\n");
  }
  Serial.print(F("Time taken:"));
  Serial.println(micros() - start);
  return rc;
}

int openDb(const char *filename, sqlite3 **db)
{
  int rc = sqlite3_open(filename, db);
  if (rc)
  {
    Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
    return rc;
  }
  else
  {
    Serial.printf("Opened database successfully\n");
  }
  return rc;
}
/************* END FUNCTION *************/

sqlite3 *db1;

String s_Date = " ";
String s_Time = " ";
String s_Name = " ";
int id = 0;
int value = 0;
uint32_t count = 0;
uint32_t pevTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  //  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  while (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed");
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }

  Serial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);


  //  listDir(SD_MMC, "/", 1);
  //  writeFile(SD_MMC, "/datalogger.txt", "esp32 power consumption data logger\n");
  //  appendFile(SD_MMC, "/datalogger.txt", "test1234\n");

  //  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  //  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));


  Serial.println("init sqlit3...");
  int rc;
  sqlite3_initialize();
  while (openDb("/sdcard/superman.db", &db1))
  {
    Serial.print("Failed...");
    delay(10);
    //    ESP.deepSleep(1e6);   // restart esp32
  }

  //    // CREATE TABLE superman.db
  //    rc = db_exec(db1, "CREATE TABLE datalogger (date TEXT, time TEXT,  IDname TEXT, number INTEGER, value INTEGER);");
  //    //  rc = db_exec(db1, "CREATE TABLE dataman (date TEXT, time TEXT, IDname content, order INTEGER, value INTEGER);");
  //    if (rc != SQLITE_OK)
  //    {
  //      Serial.println("CREATE TABLE Failed...");
  //    } else {
  //      Serial.println("CREATE TABLE Successfully...");
  //    }


  // INSERT INTO dataman superman.db
  //  char buffer[200];
  //  s_Date = "26/04/19";
  //  s_Time = "16.56";
  //  id = 0;
  //  value = random(0, 100);
  //  s_Name = "ID001";
  //
  //  sprintf(buffer, "INSERT INTO datalogger (date, time, IDname, number, value) VALUES ('%s', '%s', '%s', %d, %d);", s_Date.c_str(), s_Time.c_str(), s_Name.c_str(), id, value);
  //  rc = db_exec(db1, buffer);
  //  if (rc != SQLITE_OK)
  //  {
  //    Serial.println("INSER Failed...");
  //  }
  //  else
  //  {
  //    Serial.println("INSER Done...");
  //  }


} // end setup


void loop() {
  uint32_t curTime = millis();
  if (curTime - pevTime >= 3000) {
    pevTime = curTime;

    DateTime now = rtc.now();

    // INSERT INTO dataman superman.db
    int rc;
    char buffer[200];
    sprintf(buffer, "%d/%d/%d", now.day(), now.month(), now.year());
    s_Date = buffer;

    sprintf(buffer, "%02d:%02d", now.hour(), now.minute());
    s_Time = buffer;

    s_Name = "ID001";
    id += 1;
    value = random(0, 100);

    sprintf(buffer, "INSERT INTO datalogger (date, time, IDname, number, value) VALUES ('%s', '%s', '%s', %d, %d);", s_Date.c_str(), s_Time.c_str(), s_Name.c_str(), id, value);
    rc = db_exec(db1, buffer);
    if (rc != SQLITE_OK)
    {
      Serial.println("INSERT Failed...");
    }
    else
    {
      Serial.println("INSERT Done...");
    }
  }
}
