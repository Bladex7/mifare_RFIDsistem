#include <EEPROM.h>     // read and write UID dari/ke EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

#define COMMON_ANODE

#ifdef COMMON_ANODE 
#define Bazzer_ON LOW
#define Bazzer_OFF HIGH
#else
#define Bazzer_ON HIGH
#define Bazzer_OFF LOW
#endif
#define bazzer 5 // sebagai buzzer
#define kontak 4     // Set kontak Pin relay
//#define central_lock 6
#define wipeB 3     // tombol WipeMode
// kecocokan card yang dibaca dg yg disimpan jika cocok true
boolean match = false; //aturan awal false
boolean programMode = false;  //mode pemograman
boolean replaceMaster = false;

uint8_t successRead;    // Variable integer untuk disimpan ketika berhasil

byte storedCard[4];   // menyimpan ID ke EEPROM
byte readCard[4];   // menyimpan ID read/write dari modul RFID
byte masterCard[4];   // menyimpan ID master ke EEPROM


#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN); 

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //konfigurasi pin Arduino
  pinMode(bazzer, OUTPUT);
  pinMode(wipeB, INPUT_PULLUP);
  pinMode(kontak, OUTPUT);
  digitalWrite(kontak, HIGH);    
  digitalWrite(bazzer, Bazzer_OFF);

  //Protocol Configuration
  Serial.begin(9600);  // serial komunikasi
  SPI.begin();           // MFRC522 spi protocol
  mfrc522.PCD_Init(); 


  Serial.println(F("Selamat Datang di v0.1")); 
  ShowReaderDetails();

  //hapus code jika tombol wipeB ditekan 
  if (digitalRead(wipeB) == LOW) {  
    digitalWrite(bazzer, Bazzer_ON); 
    Serial.println(F("tombol Wipe ditekan"));
    Serial.println(F("Waktumu 15s Untuk Cancel"));
    Serial.println(F("Semua Record Akan dihapus"));
    delay(15000);                        
    if (digitalRead(wipeB) == LOW) {    
      Serial.println(F("Mulai Menghapus EEPROM"));
      for (uint8_t x = 0; x < EEPROM.length(); x = x + 1) {   
        if (EEPROM.read(x) == 0) {              
          
        }
        else {
          EEPROM.write(x, 0);  
        }
      }
      Serial.println(F("EEPROM Berhasil dihapus"));
      digitalWrite(bazzer, Bazzer_OFF); 
      delay(200);
      digitalWrite(bazzer, Bazzer_ON);
      delay(200);
      digitalWrite(bazzer, Bazzer_OFF);
      delay(200);
      digitalWrite(bazzer, Bazzer_ON);
      delay(200);
      digitalWrite(bazzer, Bazzer_OFF);
    }
    else {
      Serial.println(F("Dibatalkan")); 
      digitalWrite(bazzer, Bazzer_OFF);
    }
  }
  // Check kartu master ditentukan, jika tidak tambahkan kartu master
  // record EEPROM tulis selain 143"kartumaster/tidak" ke alamat EEPROM 1
  if (EEPROM.read(1) != 143) {
    Serial.println(F("Kartu Master Belum ditentukan"));
    Serial.println(F("Pindai Untuk ditetapkan Sebagai Kartu Master"));
    do {
      successRead = getID(); 
      digitalWrite(bazzer, Bazzer_ON);  
      delay(200);
      digitalWrite(bazzer, Bazzer_OFF);
      delay(200);
    }
    while (!successRead);                  
    for ( uint8_t j = 0; j < 4; j++ ) {   //loop 4 kali
      EEPROM.write( 2 + j, readCard[j] ); //start no 2
    }
    EEPROM.write(1, 143);
    Serial.println(F("Kartu Master ditentukan"));
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( uint8_t i = 0; i < 4; i++ ) {          // baca ID mastercard dari EEPROM
    masterCard[i] = EEPROM.read(2 + i);   
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Semua Siap"));
  Serial.println(F("Menunggu Pemindaian"));
  cycleBazzer();    
}


///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
  do {
    successRead = getID(); 
    // saat device digunakan jika tombol ditekan selama 10s. penghapusan master card
    if (digitalRead(wipeB) == LOW) { 
      digitalWrite(bazzer, Bazzer_OFF); 
      Serial.println(F("Tombol Wipe ditekan"));
      Serial.println(F("Kartu Master Akan dihapus dalam 10s"));
      delay(10000); 
      if (digitalRead(wipeB) == LOW) {
        EEPROM.write(1, 0);                  // Reset Magic Number.
        Serial.println(F("Restart Untuk Mengatur Ulang Kartu Master"));
        while (1);
      }
    }
    if (programMode) {
      cycleBazzer();              // menunggu untuk baca kartu baru
    }
    else {
      normalModeOn();   
    }
  }
  while (!successRead); 
  if (programMode) {
    if ( isMaster(readCard) ) { //keluar dari mode program jika master kan di pindai lagi
      Serial.println(F("Master Card dipindai"));
      Serial.println(F("Keluar dari Mode Program"));
      Serial.println(F("-----------------------------"));
      programMode = false;
      return;
    }
    else {
      if ( findID(readCard) ) { 
        Serial.println(F("Menghapus Kartu..."));
        deleteID(readCard);
        Serial.println("-----------------------------");
        Serial.println(F("Pindai Kartu untuk ADD atau Remove ke EEPROM"));
      }
      else {                
        Serial.println(F("Menambahkan Kartu..."));
        writeID(readCard);
        Serial.println(F("-----------------------------"));
        Serial.println(F("Pindai Kartu untuk ADD atau Remove ke EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {  //jika ID card cocok dengan ID master-masuk ke mode program
      programMode = true;
      Serial.println(F("Hello Master - Masuk Mode Program"));
      uint8_t count = EEPROM.read(0);   // Read Byte pertama dar EEPROM 
      Serial.print(F("Ada"));     // menyimpan nomor ID di EEPROM
      Serial.print(count);
      Serial.print(F("catat pada EEPROM"));
      Serial.println("");
      Serial.println(F("Pindai Kartu untuk ADD atau Remove ke EEPROM"));
      Serial.println(F("Pindai Master Card lagi Untuk Keluar di Mode Program"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) { // jika tidak, liat apakah kartu ada di EEPROM
        Serial.println(F("kartu di deteksi"));
        granted(5000);         // relay on
      }
      else { 
        Serial.println(F("kartu tidak di deteksi"));
        denied();
      }
    }
  }
} 

/////////////////////////////////////////  Akses diterima    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  digitalWrite(bazzer, Bazzer_OFF); 
  delay(100);
  digitalWrite(bazzer, Bazzer_ON);  
  delay(200);
  digitalWrite(bazzer, Bazzer_OFF);   
  delay(100);
  digitalWrite(bazzer, Bazzer_ON);  
  delay(200);
  if(digitalRead(kontak)==HIGH) 
  { digitalWrite(kontak,LOW)
  //digitalWrite(central_lock,LOW); //hidupkan kontak
  delay(2000); 
  Serial.print(F("Kontak Hidup"));
  }
  else
  { digitalWrite(kontak,HIGH); //matikan kontak
  Serial.print(F("Kontak Mati"));
  }
  
}

///////////////////////////////////////// akses ditolak  ///////////////////////////////////
void denied() {
  digitalWrite(bazzer, Bazzer_OFF); 
  delay(1000);
  Serial.print(F("Akses di Tolak"));
}


///////////////////////////////////////// Get  UID ///////////////////////////////////
uint8_t getID() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) { 
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   
    return 0;
  }
  // PICC Mifare memiliki UID 4 / 7 byte 
  Serial.println(F("Pindai PICC's UID:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  //MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown)"));
  Serial.println("");
  // 0x00 or 0xFF, komunikasi gagal
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));

    digitalWrite(bazzer, Bazzer_OFF);   
    while (true); 
  }
}

///////////////////////////////////////// Cycle bazzer (Program Mode) ///////////////////////////////////
void cycleBazzer() {

  digitalWrite(bazzer, Bazzer_OFF);   
  delay(100);
  digitalWrite(bazzer, Bazzer_ON);  
  delay(100);
delay(200);
}

//////////////////////////////////////// Normal Mode Bazzer  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(bazzer, Bazzer_ON);  // bazzer on dan kartu siap dibaca
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // posisi awal
  for ( uint8_t i = 0; i < 4; i++ ) {     // looping 4 x untuk dapat 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // nilai yang dibaca dari EEPROM ke array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // chek apakah kartu ada atau tidak
    uint8_t num = EEPROM.read(0);     // posisi o menyimpan nomor kartu ID
    uint8_t start = ( num * 4 ) + 6;  // cari dlot berikutnya dimulai
    num++;                // naikan hitungan satu persatu
    EEPROM.write( 0, num );     // tulis hitungan baru
    for ( uint8_t j = 0; j < 4; j++ ) { 
      EEPROM.write( start + j, a[j] );  // write nilai array ke EEPROM di posisi yang tepat
    }
    successWrite();
    Serial.println(F("Berhasil Menambahkan ID Ke EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Gagal, Ada Yang Salah Dengan ID atau EEPROM Yang Buruk"));
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // sebelum menghapus dari EEPROM, Chek kartu ada atau tidak
    failedWrite();      // jika tidak
    Serial.println(F("Gagal, Ada Yang Salah Dengan ID atau EEPROM Yang Buruk"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // angka didapatkan, posisi 0 menyimpan nomor ID
    uint8_t slot;       // cari slot
    uint8_t start;      // = ( num * 4 ) + 6; // cari dimana slot selanjutnya
    uint8_t looping;   
    uint8_t j;
    uint8_t count = EEPROM.read(0); // read byte pertama EEPROM yang mentimpan jumlah kartu
    slot = findIDSLOT( a );   // cari nomor slot yang akan dihapus
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;    //kurangi hitung satu persatu
    EEPROM.write( 0, num );   // tulis hitungan baru
    for ( j = 0; j < looping; j++ ) {     
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   //geser nilai array ke 4 tempat sebelumnya di EEROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {       
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Berhasil Menghapus ID Dari EEPROM"));
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != 0 )      // pastikan ada sesuatu didalam array terlebih dahulu
    match = true;       
  for ( uint8_t k = 0; k < 4; k++ ) {  
    if ( a[k] != b[k] )     // a != b kemudian set match = false, satu salah, semua salah
      match = false;
  }
  if ( match ) {      
    return true;      
  }
  else  {
    return false;       
  }
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read Byte pertama dari EEPROM
  for ( uint8_t i = 1; i <= count; i++ ) {    // loop sekali untuk setiap entri EEPROM
    readID(i);                // Read ID dari EEPRO kemudian disimpan di storedCard[4]
    if ( checkTwo( find, storedCard ) ) {  
      // jika sama dengan ID kartu find[]
      return i;      
      break;         
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);    
  for ( uint8_t i = 1; i <= count; i++ ) {    
    readID(i);          
    if ( checkTwo( find, storedCard ) ) { 
      return true;
      break; 
    }
    else {  
    }
  }
  return false;
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
//buzzer bunyi  jika write berhasil
void successWrite() {
  digitalWrite(bazzer, Bazzer_OFF);  
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
//buzzer bunyi  jika write gagal
void failedWrite() {
  digitalWrite(bazzer, Bazzer_OFF);   
  delay(200);
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
//buzzer bunyi  jika berhasil menghapus
void successDelete() {
  digitalWrite(bazzer, Bazzer_OFF);   
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
//chek apakah ID dalah kartu program master
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}
