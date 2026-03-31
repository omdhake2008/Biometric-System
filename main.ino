#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <string.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

bool waitingStore = false;
bool waitingRemove = false;
byte scannedUID[4];

bool waitingEnrollFinger = false;
bool waitingRemoveFinger = false;
uint8_t fingerID = 1;

unsigned long invalidStart = 0;
bool showingInvalid = false;

class Fingerprint;
extern Fingerprint finger;

class BiometricSystem{
public:
  enum Mode{
    NORMAL,
    ACCESS,
    ADMIN_LOGIN,
    ADMIN,
    INVALID
  };
  enum AdminOptions{
  CHANGE_PASSWORD,
  RESET_PASSWORD,
  CHANGE_ADMIN_PASSWORD,
  STORE_RFID,
  REMOVE_RFID,
  ENROLL_FINGER,
  REMOVE_FINGER,
  NONE
};
  
protected:
  Mode mode=NORMAL;
  AdminOptions opt=NONE;
public:
  void resetAuth(){
    mode=NORMAL;
    opt = NONE; 
  }
  Mode getMode(){
    return mode;
  }
  AdminOptions getAdminOption(){
    return opt;
  }
  void grantAccess(){
    mode = ACCESS;
  }
  void reset(Fingerprint &finger);
};

class LCDDisplay{
  private:
    LiquidCrystal_I2C lcd;
  public:
    LCDDisplay() : lcd(0x27,16,2){}
    void begin(){
      lcd.init();
      lcd.backlight();
    }
    void showEnterPassword(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter Password");
    }
    void clearInput(){
      lcd.setCursor(0,1);
      lcd.print("                ");
    }
    void showScanCard(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Scan RFID Card");
    }

    void showCardStored(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Card Stored");
    }
    void showFingerStored(){
      lcd.clear();
      lcd.print("Finger Stored");
    }
    void showRemoved(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Removed");
      lcd.setCursor(0,1);
      lcd.print("Successfully");
    }
    void showScanFinger(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Place Finger");
    }

    void showRemoveFinger(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Scan Finger");
    }
    void showConfirmPassword(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter again");
      lcd.setCursor(0,1);
      lcd.print("to confirm");
    }
    void showRestart(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("System Reset");
      lcd.setCursor(0,1);
      lcd.print("Restart Device");
    }
    void showAccessGranted(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Granted");
    }
    void showWrongPassword(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Invalid");
    }
    void showAccessState(bool permanent){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Mode");

      lcd.setCursor(0,1);

      if(permanent)
        lcd.print("State: P");
      else
        lcd.print("State: 2");
    }
    void showAdminLogin(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter Admin PIN");
    }
    void showAdminMode(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Admin Mode");
    }
    void printStar(byte pos){
      lcd.setCursor(pos,1);
      lcd.print("*");
    }
    void showAdminOptions(){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter Operation");
    }
    void update(BiometricSystem::Mode mode){
      switch(mode){
        case BiometricSystem::NORMAL:
          showEnterPassword();
        break;
        case BiometricSystem::ACCESS:
          showAccessGranted();
        break;
        case BiometricSystem::ADMIN_LOGIN:
          showAdminLogin();
        break;
        case BiometricSystem::ADMIN:
          showAdminMode();
        break;
        case BiometricSystem::INVALID:
         showWrongPassword();
        break;
      }
    }
    void updateAdmin(BiometricSystem::AdminOptions opt){
      lcd.clear();

      switch(opt){

        case BiometricSystem::CHANGE_PASSWORD:
          lcd.setCursor(0,0);
          lcd.print("Enter new Pass");
        break;

        case BiometricSystem::RESET_PASSWORD:
          lcd.setCursor(0,0);
          lcd.print("Reset System");
        break;

        case BiometricSystem::CHANGE_ADMIN_PASSWORD:
          lcd.setCursor(0,0);
          lcd.print("Enter new Pass");
        break;

        case BiometricSystem::STORE_RFID:
          lcd.setCursor(0,0);
          lcd.print("Scan RFID Card");
        break;

        case BiometricSystem::REMOVE_RFID:
          lcd.setCursor(0,0);
          lcd.print("Scan RFID Card");
        break;
        case BiometricSystem::ENROLL_FINGER:
          lcd.setCursor(0,0);
          lcd.print("Place Finger");
        break;

        case BiometricSystem::REMOVE_FINGER:
          lcd.setCursor(0,0);
          lcd.print("Scan Finger");
        break;
        default:
        break;
      }
    }
};

/*============================================================================*/
LCDDisplay display;
/*============================================================================*/

class Password: public BiometricSystem{
  private:
    static const byte ROWS = 4;
    static const byte COLS = 3;
    char hexaKeys[ROWS][COLS] = {
      {'1','2','3'},
      {'4','5','6'},
      {'7','8','9'},
      {'*','0','#'}
    };
    byte rowPins[ROWS] = {A2, A1, A0, 3};
    byte colPins[COLS] = {4,7,6};

    Keypad keypad;

    byte index = 0;
    char entered[5];
    char storedPassword[5];
    char adminPassword[5];
    
    bool enteringNewPassword = false;
    bool confirmPassword = false;
    char newPassword[5];

  public:
    Password() : keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS){}
    void setPassword(String pass){
      char password[5];
      strcpy(password,pass.c_str());
      for(int i=0;i<4;i++){
        EEPROM.update(i, password[i]);
      }
      getPasswords();
    }
    char getKey(){
      return keypad.getKey();
    }
    void setAdminPassword(const char* pass){
      char password[5];
      strcpy(password,pass);
      for(int i=0;i<4;i++){
        EEPROM.update(i+4, password[i]);
      }
      getPasswords();
    }
    void getPasswords(){
      for(int i=0;i<4;i++){
        storedPassword[i] = EEPROM.read(i);
        adminPassword[i]  = EEPROM.read(i+4);
      }
      storedPassword[4] = '\0';
      adminPassword[4]  = '\0';
    }
    void begin(){
      keypad.setHoldTime(2000);
      getPasswords();

      if(EEPROM.read(0) == 255){
        setPassword("0000");
        setAdminPassword("9999");
      }
      
      entered[0] = '\0';
      display.showEnterPassword();
    }

    void update(){
      static bool starPressed = false;
      static unsigned long pressStart = 0;
      char key = keypad.getKey();
      if(mode == INVALID && millis() - invalidStart > 1500){
        mode = NORMAL;
      }

      if(key && key!='*'){
        starPressed = false;
      }

      if(!key){
        if(starPressed && millis()-pressStart > 2000){
          if(mode == NORMAL){
            Serial.println("Enter Admin PIN");
            mode = ADMIN_LOGIN;
          }
          else{
            Serial.println("Exit Admin Mode");
            mode = NORMAL;
            opt=NONE;
          }

          index = 0;
          entered[0] = '\0';
          display.clearInput();
          starPressed = false;
        } 
        return;
      }

      Serial.print("Key: ");
      Serial.println(key);

      if(key == '*'){
      starPressed = true;
      pressStart = millis();
      index = 0;
      entered[0] = '\0';
      display.clearInput();
      return;
      }

      if(mode == NORMAL || mode == ADMIN_LOGIN){
        if(key == '#'){
          entered[index] = '\0';
          checkPassword();

          index = 0;
          entered[0] = '\0';
          display.clearInput();
          return;
        }

        if(index < 4){
          entered[index++] = key;
          display.printStar(index-1);
        }
        return;
      }


      if(mode == ADMIN){
        if(!enteringNewPassword){
          if(key == '1'){
            opt = CHANGE_PASSWORD;
            enteringNewPassword = true;
            confirmPassword = false;

            index = 0;
            entered[0] = '\0';

            display.clearInput();
            display.updateAdmin(opt);
            return;
          }

          if(key == '2'){
            opt = RESET_PASSWORD;

            reset(finger);

            display.showRestart();

            while(true);
          }

          if(key == '3'){
            opt = CHANGE_ADMIN_PASSWORD;
            enteringNewPassword = true;
            confirmPassword = false;

            index = 0;
            entered[0] = '\0';

            display.clearInput();
            display.updateAdmin(opt);
            return;
          }
          if(key == '4'){
            opt = STORE_RFID;
            waitingStore = true;
            display.showScanCard();
            return;
          }

          if(key == '5'){
            opt = REMOVE_RFID;
            waitingRemove = true;
            display.showScanCard();
            return;
          }
          if(key == '6'){
            opt = ENROLL_FINGER;
            waitingEnrollFinger = true;
            display.showScanFinger();
            return;
          }

          if(key == '7'){
            opt = REMOVE_FINGER;
            waitingRemoveFinger = true;
            display.showRemoveFinger();
            return;
          }
          return;
        }

        if(key == '#'){
          entered[index] = '\0';
          if(!confirmPassword){
            strcpy(newPassword, entered);
            confirmPassword = true;

            Serial.println("Confirm Password");

            index = 0;
            entered[0] = '\0';

            display.showConfirmPassword();
            display.clearInput();
            return;
          }
          else{
            if(strcmp(newPassword, entered) == 0){
              if(opt == CHANGE_PASSWORD){
                setPassword(newPassword);
                Serial.println("User password updated");
              }
              if(opt == CHANGE_ADMIN_PASSWORD){
                setAdminPassword(newPassword);
                Serial.println("Admin password updated");
              }
              mode = NORMAL;
            }
            else{
              Serial.println("Passwords do not match");
              mode = INVALID;
              invalidStart = millis();
            }

            enteringNewPassword = false;
            confirmPassword = false;

            index = 0;
            entered[0] = '\0';
            display.clearInput();
            return;
          }
        }

        if(index < 4){
          entered[index++] = key;
          display.printStar(index-1);
        }
        return;

      }
    }


private:
  void checkPassword(){
    switch(mode){
      case NORMAL:
        if(strcmp(entered,storedPassword)==0){
          Serial.println("Access Granted");
          mode=ACCESS;
        }
        else{
          Serial.println("Wrong Password");
          mode=INVALID;
          invalidStart=millis();
        }
      break;

      case ADMIN_LOGIN:
        if(strcmp(entered,adminPassword)==0){
          Serial.println("Admin Access Granted");
          mode = ADMIN;
        }
        else{
          Serial.println("Wrong Admin PIN");
          mode=INVALID;
          invalidStart=millis();
        }
      break;
      case ADMIN:
        Serial.println("Admin Mode Active");
      break;
    }
  }
};
//==============================================================================
class Fingerprint: public BiometricSystem{
  private:

    SoftwareSerial fingerSerial;
    Adafruit_Fingerprint finger;

  public:

    Fingerprint() :
      fingerSerial(A3,2),   // RX, TX
      finger(&fingerSerial)
    {}

    void begin(){

      finger.begin(57600);

      if(finger.verifyPassword()){
        Serial.println("Fingerprint sensor ready");
      }
      else{
        Serial.println("Fingerprint sensor not found");
      }
    }
    void clearAll(){
      for(int i = 1; i <= 127; i++){
        deleteFinger(i);
      }
    }
    int update(){

      if(finger.getImage() != FINGERPRINT_OK)
        return 0;

      if(finger.image2Tz() != FINGERPRINT_OK)
        return 0;

      if(finger.fingerFastSearch() != FINGERPRINT_OK)
        return -1;

      return 1;
    }
    bool enrollFinger(uint8_t id){

      unsigned long start = millis();
      int p = -1;

      display.showScanFinger();

      while(p != FINGERPRINT_OK){

        if(millis() - start > 8000){   // 8 second timeout
          waitingEnrollFinger = false;
          return false;
        }

        p = finger.getImage();
      }

      if(finger.image2Tz(1) != FINGERPRINT_OK)
        return false;

      display.showConfirmPassword();

      delay(1000);

      start = millis();
      p = -1;

      while(p != FINGERPRINT_OK){

        if(millis() - start > 8000){
          waitingEnrollFinger = false;
          return false;
        }

        p = finger.getImage();
      }

      if(finger.image2Tz(2) != FINGERPRINT_OK)
        return false;

      if(finger.createModel() != FINGERPRINT_OK)
        return false;

      if(finger.storeModel(id) != FINGERPRINT_OK)
        return false;

      return true;
    }
    bool deleteFinger(uint8_t id){

      if(finger.deleteModel(id) == FINGERPRINT_OK)
        return true;

      return false;
    }
    int findFinger(){

      unsigned long start = millis();

      while(millis() - start < 8000){

        if(finger.getImage() != FINGERPRINT_OK)
          continue;

        if(finger.image2Tz() != FINGERPRINT_OK)
          return -1;

        if(finger.fingerFastSearch() != FINGERPRINT_OK)
          return -1;

        return finger.fingerID;
      }

      waitingRemoveFinger = false;   // timeout
      return -1;
    }
  };

class Rfid: public BiometricSystem{
  private:
    static const byte SS_PIN = 10;
    static const int RFID_START = 8;
    static const byte UID_SIZE = 4;
    static const byte MAX_CARDS = 10;
    MFRC522 mfrc522;

    
  public:
    Rfid() : mfrc522(SS_PIN, MFRC522::UNUSED_PIN){};

    void begin(){
      SPI.begin();
      mfrc522.PCD_Init();
      Serial.println("RFID Ready");
    }
    int update(){
      if(!mfrc522.PICC_IsNewCardPresent())
        return 0;   // no card

      if(!mfrc522.PICC_ReadCardSerial())
        return 0;

      byte *uid = mfrc522.uid.uidByte;

      bool granted = cardExists(uid);;

      mfrc522.PICC_HaltA();

      if(granted)
        return 1;   // valid card

      return -1;    // invalid card
    }

    
    bool scanUID(byte *uid){

      if(!mfrc522.PICC_IsNewCardPresent())
        return false;

      if(!mfrc522.PICC_ReadCardSerial())
        return false;

      for(byte i=0;i<UID_SIZE;i++)
        uid[i] = mfrc522.uid.uidByte[i];

      mfrc522.PICC_HaltA();
      return true;
    }
    int findEmptySlot(){
      for(int i = 0; i < MAX_CARDS; i++){
        int addr = RFID_START + i * UID_SIZE;
        bool empty = true;
        for(int j = 0; j < UID_SIZE; j++){
          if(EEPROM.read(addr + j) != 255){
            empty = false;
            break;
          }
        }
        if(empty) return i;
      }
      return -1;   // no free slot
    }
    bool registerCard(byte *uid){
      if(cardExists(uid)){
      Serial.println("Card already registered");
      return false;
      }
      int slot = findEmptySlot();
      if(slot == -1){
        Serial.println("RFID memory full");
        return false;
      }
      int addr = RFID_START + slot * UID_SIZE;
      for(int i = 0; i < UID_SIZE; i++){
        EEPROM.update(addr + i, uid[i]);
      }
      Serial.println("Card registered");
      return true;
    }
    bool cardExists(byte *uid){
      for(int i=0;i<MAX_CARDS;i++){
        int addr = RFID_START + i * UID_SIZE;
        bool match = true;
        for(int j=0;j<UID_SIZE;j++){
          if(uid[j] != EEPROM.read(addr + j)){
            match = false;
            break;
          }
        }
        if(match) return true;
      }
      return false;
    }
    bool removeCard(byte *uid){
      for(int i=0;i<MAX_CARDS;i++){

        int addr = RFID_START + i * UID_SIZE;
        bool match = true;

        for(int j=0;j<UID_SIZE;j++){
          if(uid[j] != EEPROM.read(addr + j)){
            match = false;
            break;
          }
        }

        if(match){
          for(int j=0;j<UID_SIZE;j++){
            EEPROM.update(addr + j, 255);
          }
          return true;
        }
      }

      return false;
    }
};
//==============================================================================
void BiometricSystem::reset(Fingerprint &finger){

  // clear passwords
  for(int i = 0; i < 8; i++){
    EEPROM.update(i,255);
  }

  // clear RFID cards
  for(int i = 8; i < 48; i++){
    EEPROM.update(i,255);
  }

  // clear fingerprint templates
  finger.clearAll();

  mode = NORMAL;
  opt = NONE;
}
Password password;
Rfid rfid;
Fingerprint finger;


void setup(){
  Serial.begin(9600);
  display.begin();
  password.begin();
  rfid.begin();
  finger.begin();
}

void loop(){

  static BiometricSystem::Mode lastMode = BiometricSystem::INVALID;
  static BiometricSystem::AdminOptions lastOpt = BiometricSystem::NONE;

  password.update();

  /* cancel enroll/remove with * */
  if(waitingEnrollFinger || waitingRemoveFinger){
    char k = password.getKey();
    if(k == '*'){
      waitingEnrollFinger = false;
      waitingRemoveFinger = false;

      password.resetAuth();
      display.showEnterPassword();
      return;
    }
  }

  /* ================= RFID STORE ================= */

  if(waitingStore){
    if(rfid.scanUID(scannedUID)){
      rfid.registerCard(scannedUID);

      display.showCardStored();
      delay(1500);

      waitingStore = false;
      password.resetAuth();
      lastOpt = BiometricSystem::NONE;

      display.showEnterPassword();
    }
  }

  /* ================= RFID REMOVE ================= */

  if(waitingRemove){
    if(rfid.scanUID(scannedUID)){
      rfid.removeCard(scannedUID);

      display.showRemoved();
      delay(1500);

      waitingRemove = false;
      password.resetAuth();
      lastOpt = BiometricSystem::NONE;

      display.showEnterPassword();
    }
  }

  /* ================= FINGERPRINT ENROLL ================= */

  if(waitingEnrollFinger){

    if(finger.enrollFinger(fingerID)){
      display.showFingerStored();
      fingerID++;
    }
    else{
      display.showWrongPassword();
    }

    delay(1500);

    waitingEnrollFinger = false;
    password.resetAuth();
    lastOpt = BiometricSystem::NONE;

    display.showEnterPassword();
  }

  /* ================= FINGERPRINT REMOVE ================= */

  if(waitingRemoveFinger){

    int id = finger.findFinger();

    if(id >= 0){

      if(finger.deleteFinger(id)){
        display.showRemoved();
      }
      else{
        display.showWrongPassword();
      }

      delay(1500);
    }
    else{
      // timeout or no finger
      display.showWrongPassword();
      delay(1500);
    }

    waitingRemoveFinger = false;
    password.resetAuth();
    lastOpt = BiometricSystem::NONE;

    display.showEnterPassword();
  }

  /* ================= RFID + FINGER ACCESS ================= */

  /* ================= RFID + FINGER ACCESS ================= */

/* ================= RFID + FINGER ACCESS ================= */

if(password.getMode() == BiometricSystem::NORMAL && !showingInvalid){

  int r = rfid.update();

  if(r == 1){
  password.grantAccess();
  // display.showAccessGranted();   // show immediately

  }

  else if(r == -1){
  password.resetAuth();
  invalidStart = millis();
  showingInvalid = true;
  display.showWrongPassword();
}

  int f = finger.update();

  if(f == 1){
  password.grantAccess();
  // display.showAccessGranted();   // show immediately


}

  else if(f == -1){
  password.resetAuth();
  invalidStart = millis();
  showingInvalid = true;
  display.showWrongPassword();
}
}
  /* ================= MODE CHANGE ================= */

  BiometricSystem::Mode currentMode = password.getMode();
  BiometricSystem::AdminOptions opt = password.getAdminOption();

  if(currentMode != lastMode){

    if(currentMode == BiometricSystem::ADMIN){
      display.showAdminOptions();
    }
    else{
      display.update(currentMode);
    }

    lastMode = currentMode;
  }

  /* ================= ADMIN OPTION ================= */

  if(currentMode == BiometricSystem::ADMIN &&
     opt != lastOpt &&
     !waitingStore &&
     !waitingRemove &&
     !waitingEnrollFinger &&
     !waitingRemoveFinger){
    display.updateAdmin(opt);
    lastOpt = opt;
  }

  if(showingInvalid && millis() - invalidStart > 1500){
    display.showEnterPassword();
    showingInvalid = false;

  }
}