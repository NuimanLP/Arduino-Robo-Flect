/*
 * Test DFPlayer Mini by typing commands in Serial Monitor (Arduino UNO)
 * Wiring:
 *   DFPlayer: TX -> D10 (UNO RX),  RX -> D11 (UNO TX, via ~1k resistor), VCC=5V, GND=GND
 * microSD structure:
 *   /02/001.mp3, /02/002.mp3, /02/003.mp3   (welcome queue)
 *   /01/001.mp3 ... per user track
 *   /01/099.mp3                             (unknown)
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <string.h>
#include <ctype.h>

// ---------- DFPlayer on SoftwareSerial ----------
#define DF_RX_PIN 10  // UNO receives from DFPlayer TX
#define DF_TX_PIN 11  // UNO sends to DFPlayer RX (put ~1k series resistor)

SoftwareSerial dfSerial(DF_RX_PIN, DF_TX_PIN); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// ---------- Users (name -> track) ----------
struct User {
  const char* name;   // e.g., "User 1"
  int folderNumber;   // keep for future (we use folder 1 when speaking user)
  int trackNumber;    // e.g., 1 -> /01/001.mp3
};
const User users[] = {
  {"User 1", 1, 1},
  {"User 2", 1, 2},
  {"User 3", 1, 3},
  {"User 4", 1, 4},
  {"User 5", 1, 5},
  {"User 6", 1, 6},
  {"User 7", 1, 7},
  {"User 8", 1, 8},
};
const int USER_COUNT = sizeof(users) / sizeof(users[0]);

// ---------- Simple queue (for welcome sequence) ----------
struct AudioFile { int folder; int track; };
AudioFile audioQueue[6];
int queueSize = 0;
int currentQueueIndex = 0;

enum QueueMode { QUEUE_NONE, QUEUE_WELCOME, QUEUE_USER };
QueueMode queueMode = QUEUE_NONE;

bool isPlaying = false;

// ---------- Serial line buffer ----------
char lineBuf[40];
byte lineLen = 0;

// ---------- Helpers ----------
void clearQueue() {
  queueSize = 0;
  currentQueueIndex = 0;
  queueMode = QUEUE_NONE;
}

void playNextInQueue() {
  if (currentQueueIndex < queueSize) {
    myDFPlayer.playFolder(audioQueue[currentQueueIndex].folder,
                          audioQueue[currentQueueIndex].track);
    isPlaying = true;
    currentQueueIndex++;
  } else {
    isPlaying = false;
    clearQueue();
    Serial.println(F("[INFO] Queue finished."));
  }
}

// normalize string: remove spaces, to-lower (in-place)
void normalize(char* s) {
  byte w = 0;
  for (byte r = 0; s[r]; r++) {
    char c = s[r];
    if (!isspace((unsigned char)c)) {
      lineBuf[w++] = tolower((unsigned char)c);
    }
  }
  lineBuf[w] = '\0';
}

// return index of user by normalized name "user1", "user2", ...
int userIndexFromNormalized(const char* norm) {
  // Accept forms: "user1" or "user01" etc.
  if (strncmp(norm, "user", 4) == 0) {
    int num = atoi(norm + 4);
    if (num >= 1 && num <= USER_COUNT) return num - 1;
  }
  // Also try exact normalized match of the stored names (remove spaces, lower)
  for (int i = 0; i < USER_COUNT; i++) {
    char tmp[16];
    strncpy(tmp, users[i].name, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    // turn "User 1" -> "user1"
    for (byte j = 0; tmp[j]; j++) {
      tmp[j] = tolower((unsigned char)tmp[j]);
    }
    // strip spaces
    byte w = 0;
    for (byte j = 0; tmp[j]; j++) if (!isspace((unsigned char)tmp[j])) tmp[w++] = tmp[j];
    tmp[w] = '\0';
    if (strcmp(tmp, norm) == 0) return i;
  }
  return -1;
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F("  list                -> show users"));
  Serial.println(F("  user1 / user 1     -> speak User 1 (also user2..user8)"));
  Serial.println(F("  vol N              -> set volume 0..30"));
  Serial.println(F("  play F T           -> play folder F, track T (e.g., play 1 5)"));
  Serial.println(F("  help               -> show this help"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  dfSerial.begin(9600);
  Serial.println(F("Initializing DFPlayer ... (May take 3-5 seconds)"));
  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the wiring!"));
    Serial.println(F("2.Please make sure you have a SD card and it's FAT32 formatted!"));
    while (true) { delay(10); }
  }
  Serial.println(F("DFPlayer Mini is online."));
  myDFPlayer.volume(25); // 0..30

  // Welcome queue: /02/001, /02/002, /02/003
  audioQueue[0] = {2, 1};
  audioQueue[1] = {2, 2};
  audioQueue[2] = {2, 3};
  queueSize = 3;
  currentQueueIndex = 0;
  queueMode = QUEUE_WELCOME;
  isPlaying = true;
  playNextInQueue();

  printHelp();
  Serial.println(F("\nType a command (e.g., user1) and press Enter."));
}

void loop() {
  // DFPlayer event
  if (myDFPlayer.available()) {
    uint8_t t = myDFPlayer.readType();
    if (t == DFPlayerPlayFinished) {
      Serial.println(F("[DF] Finished. Next..."));
      if (queueMode != QUEUE_NONE && queueSize > 0) {
        playNextInQueue();
      } else {
        isPlaying = false;
      }
    }
  }

  // Read one line from Serial (ending with \n or \r)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) {
        // process command
        // make a copy to parse tokens
        char cmd[40];
        strncpy(cmd, lineBuf, sizeof(cmd)-1);
        cmd[sizeof(cmd)-1] = '\0';

        // normalized version without spaces, lowercase
        normalize(cmd); // result overwritten into lineBuf
        // lineBuf now contains normalized string (e.g., "user1")

        if (strcmp(lineBuf, "list") == 0) {
          Serial.println(F("Users:"));
          for (int i = 0; i < USER_COUNT; i++) {
            Serial.print(F("  "));
            Serial.print(i+1);
            Serial.print(F(": "));
            Serial.print(users[i].name);
            Serial.print(F("  -> /01/"));
            if (users[i].trackNumber < 10) Serial.print('0');
            if (users[i].trackNumber < 100) Serial.print('0');
            Serial.println(users[i].trackNumber);
          }
        }
        else if (strncmp(lineBuf, "vol", 3) == 0) {
          // original (non-normalized) line for number extraction
          int v = -1;
          // simple parse: look for integer in raw lineBuf? we normalized -> lost spaces.
          // Use the saved raw input in 'lineBuf' got normalized already; so use 'cmd' before normalize next time if needed.
          // Easiest: re-scan from original string in lineBuf? We overwrote. So read from 'lineLen' not possible.
          // Simpler approach: parse from Serial is more complex. We'll use a separate function soon.
          // For now, assume form "volN" e.g., "vol20"
          v = atoi(lineBuf + 3);
          if (v >= 0 && v <= 30) {
            myDFPlayer.volume(v);
            Serial.print(F("[DF] Volume set to ")); Serial.println(v);
          } else {
            Serial.println(F("[ERR] Use: vol0..vol30 (e.g., vol20)"));
          }
        }
        else if (strncmp(lineBuf, "play", 4) == 0) {
          // form: "playF T" -> normalized removed spaces -> e.g., "play15" (folder=1, track=5)
          // To keep it simple, accept "playF_T" like "play1_5" or "play1-5" too
          int F = -1, T = -1;
          // try patterns
          // 1) "play<folder><track>" with two digits each if needed (ambiguous) -> skip
          // 2) allow "play<folder>-<track>" or "play<folder>_<track>"
          char* p = lineBuf + 4;
          char* sep = strchr(p, '-');
          if (!sep) sep = strchr(p, '_');
          if (sep) {
            *sep = '\0';
            F = atoi(p);
            T = atoi(sep + 1);
          } else {
            // fallback: if string like "play15" meaning F=1 T=5
            if (strlen(p) == 2) { F = p[0]-'0'; T = p[1]-'0'; }
          }
          if (F > 0 && T > 0) {
            clearQueue();
            myDFPlayer.playFolder(F, T);
            isPlaying = true;
            Serial.print(F("[DF] playFolder(")); Serial.print(F); Serial.print(','); Serial.print(T); Serial.println(')');
          } else {
            Serial.println(F("[ERR] Use: playF-T or playF_T (e.g., play1-5)"));
          }
        }
        else if (strcmp(lineBuf, "help") == 0) {
          printHelp();
        }
        else {
          // try userN
          int idx = userIndexFromNormalized(lineBuf);
          if (idx >= 0) {
            clearQueue();
            // play user's track in folder 1
            myDFPlayer.playFolder(1, users[idx].trackNumber);
            isPlaying = true;
            Serial.print(F("[DF] Speaking ")); Serial.print(users[idx].name);
            Serial.print(F(" -> /01/"));
            Serial.println(users[idx].trackNumber);
          } else {
            // unknown
            clearQueue();
            myDFPlayer.playFolder(1, 99);
            isPlaying = true;
            Serial.println(F("[DF] Unknown command -> play /01/099.mp3"));
          }
        }

        // reset buffer
        lineLen = 0;
      }
    } else {
      if (lineLen < sizeof(lineBuf) - 1) {
        lineBuf[lineLen++] = c;
      }
    }
  }
}
