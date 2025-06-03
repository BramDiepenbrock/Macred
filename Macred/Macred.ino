/* Macred code: for the Sparkfun Pro Micro with VL6180x-TOF050C, SSD1306, BMP280 and mechanical switches
  by: Bram Diepenbrock

  The Pong game is based on: https://github.com/shveytank/Arduino_Pong_Game/tree/master from shveytank. I added a game counter, a condition when the game is over and a decreasing paddle size depending on the score.
  TODO: I want to implement a TOF sensor that turns on the screen (and LED's) when a hand is close. Currently the fysical placement is far from optimal.
*/

#include <Wire.h>
#include <SPI.h>
#include <TimerOne.h>
#include <Adafruit_GFX.h>
#include <Fonts/Picopixel.h>

// #include "Adafruit_VL6180X.h"
#include "BMP280.h"
#include <Keyboard.h>

// Library optimization
#define SSD1306_NO_SPLASH  // Saves ~1KB
#include <Adafruit_SSD1306.h>


// Global variables
bool displayUpdate0 = true;  // decides if the display needs an update on a region
//                1, 2,  3,  4,  5,  6, 7, 8, 9        KeyPins is an array. The function getKeyStatus adds + 1. For example: pin 16 is pressed, 4th place in the array, 5th case in the profile.
int keyPins[9] = { 4, 5, 10, 16, 14, 6, 7, 8, 9 };  // GPIO pins reserved for the keyboard
int keyStatus;                                      // Used to monitor which buttons are pressed
int profile;                                        // keyboard profile variable to switch keyboard binds
int RXLED = 17;                                     // The RX LED has a defined Arduino pin
int TXLED = 30;                                     // The TX LED has a defined Arduino pin
unsigned int timeoutRepeat = 400;                   // top of the repeat delay for the keyboard to start sending extra presses
unsigned int timeoutDisplay = 40000;                // for how long a message should be displayed on the screen
const int distReadInterval = 100;                   // read interval for the distance sensor. Without it reading blocks code execution
unsigned short distLastReadTime = 0;                // start value of last read time for the distance sensor since boot
const unsigned short int tempReadInterval = 30000;
unsigned short tempLastReadTime = 0;
bool gameMode = false;  // game mode that changes the void loop function
bool gameStart = true;  // setup for game mode. Goes to true when the game is started, then to false
// game globals
const unsigned long PADDLE_RATE = 33;
unsigned long BALL_RATE = 16;
uint8_t PADDLE_HEIGHT = 24;

uint8_t ball_x = 64, ball_y = 32;        // starting position for the ball
uint8_t ball_dir_x = 4, ball_dir_y = 4;  // starting direction for the ball
unsigned long ball_update;
unsigned long hit_counter = 0;  // Counter for how many succesfull passes there have been

unsigned long paddle_update;
const uint8_t CPU_X = 12;
uint8_t cpu_y = 16;

const uint8_t PLAYER_X = 115;
uint8_t player_y = 16;


// Devices
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire, 4);
// Adafruit_VL6180X vl = Adafruit_VL6180X();
BMP280 bmp280;

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);

  pinMode(RXLED, OUTPUT);  // Set RX LED as an output
  pinMode(TXLED, OUTPUT);  // Set TX LED as an output

  //Init display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Serial.println(F("SSD1306 begin failed"));
    for (;;)
      ;
  }

  //Init distance sensor
  // Serial.println("Adafruit VL6180x test!");
  // if (!vl.begin()) {
  //   // Serial.println("Failed to find sensor");
  //   while (1)
  //     ;
  // }
  // Serial.println("Distance sensor VL6180x found!");

  //Init BMP280 temperature and pressure sensor
  bmp280.begin();
  bmp280.setCtrlMeasSamplingPress(BMP280::eSampling_no);  // Disable pressure reading
  bmp280.setCtrlMeasMode(BMP280::eCtrlMeasModeForced);    // Start first temperature measurement

  // Welcome message
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setRotation(1);
  display.setFont(&Picopixel);
  // display.setFont(&FreeSerif9pt7b);
  // display.setFont(&FreeSerif9pt7b);   // Visually the best
  display.setCursor(29, 20);
  display.println(F("M"));
  display.setCursor(29, 40);
  display.println(F("A"));
  display.setCursor(29, 60);
  display.println(F("C"));
  display.setCursor(29, 80);
  display.println(F("R"));
  display.setCursor(29, 100);
  display.println(F("E"));
  display.setCursor(29, 120);
  display.println(F("D"));
  display.display();
  delay(5000);
  display.clearDisplay();


  //Init profile keyboard
  profile = 0;

  //Init keyboard
  for (int i = 0; i < 9; i++) {
    pinMode(keyPins[i], INPUT_PULLUP);  // Set all keypad pins as inputs
    // digitalWrite[i], HIGH);  // pull all keypad pins high
  }
}


// pointer array for switching keyboard profiles
void (*sendKeyPressProfile[])(int) = { sendKeyPressProfile0, sendKeyPressProfile1 };


// the loop function runs over and over again forever
void loop() {
  digitalWrite(RXLED, HIGH);
  digitalWrite(TXLED, HIGH);

  // Game mode loop
  if (gameMode == true) {
    // void setup for the game
    if (gameStart == true) {
      Serial.println("Init for pong");
      // Configure display
      display.clearDisplay();
      display.setRotation(0);
      displayCourt();
      ball_update = millis();
      paddle_update = ball_update;
      gameStart = false;

      // Reset variables to get ball in starting position
      ball_x = 64, ball_y = 32;
      ball_dir_x = 1, ball_dir_y = 1;
      cpu_y = 16;
      player_y = 16;
      hit_counter = 0;
      delay(1000);
    }
    bool update = false;
    unsigned long time = millis();

    static bool up_state = false;
    static bool down_state = false;

    // keyStatus = getKeyStatus();
    // TODO: the getKeyStatus function has an 50 ms delay. Avoid.
    up_state |= (digitalRead(9) == LOW);
    down_state |= (digitalRead(8) == LOW);

    if (time > ball_update) {
      uint8_t new_x = ball_x + ball_dir_x;
      uint8_t new_y = ball_y + ball_dir_y;
      Serial.print(ball_x);
      Serial.print(ball_dir_x);
      Serial.println(new_x);

      // Check if we hit the vertical walls
      if (new_x == 0 || new_x == 127) {
        Serial.println("Someone lost!");

        gameMode = false;
        gameStart = true;
        displayGameOver();
      }

      // Check if we hit the horizontal walls.
      if (new_y == 0 || new_y == 63) {
        ball_dir_y = -ball_dir_y;
        new_y += ball_dir_y + ball_dir_y;
      }

      // Check if we hit the CPU paddle
      if (new_x == CPU_X && new_y >= cpu_y && new_y <= cpu_y + PADDLE_HEIGHT) {
        ball_dir_x = -ball_dir_x;
        new_x += ball_dir_x + ball_dir_x;
      }

      // Check if we hit the player paddle
      if (new_x == PLAYER_X
          && new_y >= player_y
          && new_y <= player_y + PADDLE_HEIGHT) {
        ball_dir_x = -ball_dir_x;
        new_x += ball_dir_x + ball_dir_x;
        hit_counter++;  // increment the hitcounter
        // Decrease size paddle every 2 hits
        if (hit_counter % 2 == 0) {
          PADDLE_HEIGHT--;
        }
        // Increase speed every 4 hits
        if (hit_counter % 4 == 0) {
          BALL_RATE++;
        }
        Serial.print("Hit counter: ");
        Serial.println(hit_counter);
      }

      display.drawPixel(ball_x, ball_y, BLACK);
      display.drawPixel(new_x, new_y, WHITE);
      ball_x = new_x;
      ball_y = new_y;

      ball_update += (BALL_RATE - hit_counter);
      Serial.print("ball_update: "), Serial.println(ball_update);

      update = true;
    }

    if (time > paddle_update) {
      paddle_update += PADDLE_RATE;

      // CPU paddle
      display.drawFastVLine(CPU_X, cpu_y, PADDLE_HEIGHT + 1, BLACK);
      const uint8_t half_paddle = PADDLE_HEIGHT >> 1;
      if (cpu_y + half_paddle > ball_y) {
        cpu_y -= 1;
      }
      if (cpu_y + half_paddle < ball_y) {
        cpu_y += 1;
      }
      if (cpu_y < 1) cpu_y = 1;
      if (cpu_y + PADDLE_HEIGHT > 63) cpu_y = 63 - PADDLE_HEIGHT;
      display.drawFastVLine(CPU_X, cpu_y, PADDLE_HEIGHT, WHITE);

      // Player paddle
      display.drawFastVLine(PLAYER_X, player_y, PADDLE_HEIGHT + 1, BLACK);
      if (up_state) {
        player_y -= 1;
      }
      if (down_state) {
        player_y += 1;
      }
      up_state = down_state = false;
      if (player_y < 1) player_y = 1;
      if (player_y + PADDLE_HEIGHT > 63) player_y = 63 - PADDLE_HEIGHT;
      display.drawFastVLine(PLAYER_X, player_y, PADDLE_HEIGHT, WHITE);

      update = true;
    }

    if (update)
      display.display();
  }
  // Normal macro keyboard loop
  else {
    // Read the TOF distance sensor without blocking code
    // if (millis() - distLastReadTime >= distReadInterval) {
    //   distLastReadTime = millis();
    //   uint8_t range = vl.startRange();
    //   if (vl.readRangeStatus() == 0) {  // Check if reading is valid, 0 means valid reading. This is because the senor is placed wrong in the housing.
    //     Serial.print("readRangeResult: ");
    //     range = vl.readRangeResult();
    //     Serial.println(range);
    //     if (range <= 40) {
    //       // Object is detected closeby, display message
    //       displayMovement();
    //     }
    //   }
    // }
    if (millis() - tempLastReadTime >= tempReadInterval) {  // Check if 5 minutes have been passed
      tempLastReadTime = millis();
      bmp280.setCtrlMeasMode(BMP280::eCtrlMeasModeForced);
    }

    keyStatus = getKeyStatus();  // Read which button is pressed
    if (keyStatus != 0)          // If a button is pressed go into here
    {
      // Serial.print("GPIO button pressed: "); Serial.println(keyStatus);
      displayUpdate0 = true;
      sendKeyPressProfile[profile](keyStatus);                    // send the button over USB
      timeoutRepeat = 400;                                        // top of the repeat delay
      while ((getKeyStatus() == keyStatus) && (--timeoutRepeat))  // Decrement timeout and check if key is being held down
        delayMicroseconds(1);
      while (getKeyStatus() == keyStatus)  // while the same button is held down
      {
        sendKeyPressProfile[profile](keyStatus);  // continue to send the button over USB
        delay(50);                                // 50ms repeat rate
      }
      timeoutDisplay = 40000;  // top of display clearance delay in microsenconds
    }
    while ((getKeyStatus() == 0) && (--timeoutDisplay))  // Decrement display timout and check if no GPIO/key is held down
    {
      delayMicroseconds(1);  // ! microseconds to allow quick reponse in GPIO changes
    }
    if (getKeyStatus() == 0)  // after each while this will be triggered. Mechanism can be made that it does only once
    {
      display.clearDisplay();  // clear display while no GPIO is being pressed
      display.display();
    }
  }
}


/* sendKeyPress(int key): This function sends a single key over USB
   It requires an int, indicating the key pressed.
   This function will only send a key press if a single button
   is being pressed */
void sendKeyPressProfile0(int key) {

  // Refer to line 22 and 23 to see which pin I mapped to a case
  Serial.println(key);

  switch (key) {
    case 1:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.write('s');
      Keyboard.release(KEY_LEFT_CTRL);
      displayKeyPress("Save");
      break;
    case 2:
      // Keyboard.write('2');
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.write('y');
      Keyboard.release(KEY_LEFT_CTRL);
      displayKeyPress("Redo");
      break;
    case 3:
      Keyboard.write('m');
      displayKeyPress("m");
      break;
    case 4:
      // Keyboard.write('4');
      Keyboard.write(KEY_DELETE);
      displayKeyPress("Del");
      break;
    case 5:
      // Keyboard.write('5');
      Keyboard.write(KEY_KP_ENTER);
      displayKeyPress("Enter");
      break;
    case 6:
      // Keyboard.write('6');
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.write('z');
      Keyboard.release(KEY_LEFT_CTRL);
      displayKeyPress("Undo");
      break;
    case 7:
      Keyboard.write(KEY_HOME);
      displayKeyPress("Home");
      break;
    case 8:
      // Keyboard.write('8');
      Keyboard.write(KEY_TAB);
      displayKeyPress("Tab");
      break;
    case 9:
      // Keyboard.write('9');
      Keyboard.write(KEY_LEFT_CTRL);
      displayKeyPress("Ctrl");
      break;
  }
}


/* sendKeyPress(int key): This function sends a single key over USB
   It requires an int, indicating the key pressed.
   This function will only send a key press if a single button
   is being pressed */
void sendKeyPressProfile1(int key) {

  // Refer to line 22 and 23 to see which pin I mapped to a case
  // Serial.println(key);

  switch (key) {
    case 1:
      Keyboard.write('3');
      displayKeyPress("3");
      break;
    case 2:
      Keyboard.write('2');
      displayKeyPress("2");
      break;
    case 3:
      Keyboard.write('6');
      displayKeyPress("6");
      break;
    case 4:
      Keyboard.write('5');
      displayKeyPress("5");
      break;
    case 5:
      Keyboard.write('4');
      displayKeyPress("4");
      break;
    case 6:
      Keyboard.write('1');
      displayKeyPress("1");
      break;
    case 7:
      Keyboard.write('9');
      displayKeyPress("9");
      break;
    case 8:
      Keyboard.write('8');
      displayKeyPress("8");
      break;
    case 9:
      Keyboard.write('7');
      displayKeyPress("7");
      break;
  }
}


/* getKeyStatus(): This function returns an int that represents
    status of all the keys on the Macred. It also switches the profile int.
*/
int getKeyStatus() {
  // Refer to line 22 and 23 to see which pin I mapped to a case

  // if statement to switch profiles or start a game
  if (!digitalRead(keyPins[4]) && !digitalRead(keyPins[8])) {   // case 5 and 9
    profile ^= 1;  // expression that toggles the value profile between 0 and 1
    // profile = (profile + 1) % 3; // modulo for 3 profiles
    // Serial.print("Switched to profile ");
    // Serial.println(profile);
    displayProfileSwitch(profile);  // display to which profile is being switched
  }
  // if statement to switch to tetris or some other game that fits in the memory
  if (!digitalRead(keyPins[3]) && !digitalRead(keyPins[7])) {   // case 4 and 8
    Serial.println("Tetris time!");
    // Start or stop Tetris
    gameMode = !gameMode;
  }
  // if statement to display the bmp280 sensor values, temperature and pressure
  if (!digitalRead(keyPins[2]) && !digitalRead(keyPins[6])) {
    Serial.println("Display temperature and pressure");
    displayTempPres();
    delay(1000);
  }

  int keyStatus = 0;                   // this will be what's returned
  for (int col = 0; col < 9; col++) {  // for loop to check all the keys on the Macred
    if (!digitalRead(keyPins[col])) {
      delay(1);  // debounce key: wait for 50 ms to ensure the press is stable. Can be adjusted if necessary
      if (!digitalRead(keyPins[col])) {
        keyStatus = col + 1;
        // Serial.print("This pin is pressed: ");
        // Serial.println(keyPins[col]);
      }
    }
  }

  return keyStatus;
}


/* displayTempPres(): display the current temperature and pressure
*/
void displayTempPres() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Temp:");
  display.setCursor(0, 20);
  display.println(bmp280.getTemperature());
  display.setCursor(0, 40);
  display.println("Pres:");
  display.setCursor(0, 60);
  display.println(bmp280.getPressure());
  display.display();
}

/* displayKeyPress(): display which function is activated when a key is pressed. 
This is expressed in a string. Maybe add symbols at some point
*/
void displayKeyPress(String key) {
  if (displayUpdate0) {
    Serial.println(key);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setFont();
    display.setCursor(0, 0);
    display.println("Key");
    display.setCursor(0, 10);
    display.println("Press");
    display.setCursor(0, 40);
    display.println(key);
    display.setFont();
    display.display();

    displayUpdate0 = false;
  }
}


/* displayProfileSwitch(): display which profile is being switched to. Maybe add icons/symbols
*/
void displayProfileSwitch(int profile) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setFont(&Picopixel);
  display.println("Profile");
  display.setCursor(50, 35);
  display.println(profile);
  display.display();

  delay(1000);  // wait 1 second between switching profiles to prevent rapid switching
}


/* displayKeyOverview():
  TODO: I want to create a display with all the keybinds in the current profile.
*/
void displayKeyOverview(int profile) {
  display.display();
}

/* displayCourt(): for the pong game. draws a rectangle that serves as the court
*/
void displayCourt() {
  display.drawRect(0, 0, 128, 64, WHITE);
}

/* displayGameOver(): game over screen
*/
void displayGameOver() {
  display.clearDisplay();
  display.setRotation(1);
  display.setCursor(5, 20);
  display.println("Game");
  display.setCursor(5, 30);
  display.println("Over");
  display.setCursor(15, 55);
  display.println(hit_counter);
  display.display();
  delay(2000);
}