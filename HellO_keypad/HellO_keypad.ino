#include <Keypad.h>

const byte ROWS = 4; //four rows
const byte COLS = 4; // four columns
char keys[ROWS][COLS] = {
  {'1','2','3', 'A'},
  {'4','5','6', 'B'},
  {'7','8','9', 'C'},
  {'*','0','#', 'D'}
};
byte rowPins[ROWS] = {32, 33, 34, 35}; //connect to the row pinouts of the keypad 32, 33, 34, 35
byte colPins[COLS] = {28, 29, 30, 31}; //connect to the column pinouts of the keypad 28, 29, 30, 31

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

void setup(){
  Serial.begin(9600);
}
  
void loop(){
  char key = keypad.getKey();
  
  if (key){
    Serial.println(key);
  }
}