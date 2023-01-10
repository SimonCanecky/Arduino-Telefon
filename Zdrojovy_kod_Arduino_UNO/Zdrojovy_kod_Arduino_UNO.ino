#include <SoftwareSerial.h>
SoftwareSerial displaySerial(10, 11); //Rx, Tx

char displayCommand[30], simCommand[30];
String data, page, displayMsg, simMsg;

void setup() {                 //Inicializácia HardwareSerial, SoftwareSerial, spustenie GSM modulu SIM900
  Serial.begin(9600);          //GSM modul SIM900
  displaySerial.begin(9600);   //Nextion displej
  pinMode(9, OUTPUT);          //Pin pre softvérové zapínanie GSM modulu SIM900
  delay(2000);
  powerOn();
}

void loop() {
  while (displaySerial.available()) {   //Čítanie SW serial
    data.concat(char(displaySerial.read()));
  }
  delay(10);

  if (!displaySerial.available()) {

    if (data.length()) {
      page = data[data.length() - 4];                       //Čítanie SW serialu a získanie čísla strany
      displayMsg = data.substring(1, data.length() - 4);    //Čítanie SW serialu a získanie dát príkazu
      if (page == "0") {         //Čítanie SW serialu, ak číslo strany je 0, načítanie všetkých SMS správ z pamäte GSM modulu SIM900
        loadingSMS(displayMsg);
      }
      else if (page == "1") {    //Čítanie SW serialu, ak  číslo strany je 1, zavolanie na zadané číslo
        makeCall(displayMsg);
      }
      else if (page == "2") {    //Čítanie SW serialu, ak číslo strany je 2, ukončenie hovoru
        endCall(displayMsg);
      }
      else if (page == "3") {    //Čítanie SW serialu, ak číslo strany je 3, poslanie SMS
        sendSMS(displayMsg);
      }
      else if (page == "7") {    //Čítanie SW serialu, ak číslo strany je 7, vymazanie všetkých SMS z pamäte GSM modulu SIM900
        deleteSMS(displayMsg);
      }
      else if (page == "9") {    //Čítanie SW serialu, ak číslo strany je 9, prijatie hovoru
        pickUpCall(displayMsg);
      }
      else if (page == "11") {   //Čítanie SW serialu, ak číslo strany je 11, vymazanie zobrazenej SMS na displeji z pamäte GSM modulu SIM900
        deleteSMS(displayMsg);
      }
      data = "";
      page = "";
      displayMsg = "";
    }
  }

  if (Serial.available()) {     //Čítanie HW serial

    simMsg = Serial.readString();

    if (simMsg.indexOf("NO CARRIER") != -1) {        //Prejdenie na stranu 0, ak osoba, ktorá s nami volá zavesí
      sendDisplay("page page0");
      sendSIM("AT", "OK", 2000);
    }
    else if (simMsg.indexOf("BUSY") != -1) {         //Prejdenie na stranu 0, ak osoba, ktorej voláme odmietne prichádzajúci hovor
      sendDisplay("page page0");
      sendSIM("AT", "OK", 2000);
    }
    else if (simMsg.indexOf("NO ANSWER") != -1) {    //Prejdenie na stranu 0, ak osoba, ktorej voláme neodpovedá
      sendDisplay("page page0");
      sendSIM("AT", "OK", 2000);
    }
    else if (simMsg.indexOf("RING") != -1) {         //Prejdenie na stranu 9 pri prichádzajúcom hovore
      int firstPart = simMsg.indexOf("\"");
      int secondPart = simMsg.indexOf("\"", firstPart + 1);
      String phoneNumber = simMsg.substring(firstPart + 1, secondPart);
      sendDisplay("page page9");
      sendDisplay("page9.t1.txt=\"" + phoneNumber + "\"");
      delay(100);
      sendSIM("AT", "OK", 2000);
    }
    else if (simMsg.indexOf("+CMTI") != -1) {        //Funkcia na načítanie prijatých SMS správ
      readAllSMS();
      sendDisplay("vis p0,1");
    }
    simMsg = "";
  }
}

void powerOn() {    //Inicializácia GSM modulu SIM900, pripojenie k sieti operátora, načítanie prijatých SMS správ
  digitalWrite(9, HIGH);
  delay(1000);
  digitalWrite(9, LOW);
  delay(7000);
  sendSIM("AT", "OK", 2000);
  while ((sendSIM("AT+CREG?", "+CREG: 0,1", 500) || sendSIM("AT+CREG?", "+CREG: 0,5", 500)) == 0);
  sendDisplay("page0.t0.txt=\"Searching...\"");
  searchNetwork();
  readAllSMS();
  delay(1000);
}

int8_t sendSIM(char* commandAT, char* requiredAnswer, unsigned long lenghtTime) {   //Funkcia odosielania AT príkazov a čítania odpovedí z GSM modulu SIM900
  uint8_t i = 0, answer = 0;
  char simAnswer[100] = { 0x00 };
  unsigned long lastTime;
  delay(100);
  while (Serial.available() > 0) Serial.read();
  Serial.println(commandAT);
  lastTime = millis();
  do {
    if (Serial.available() != 0) {
      simAnswer[i] = Serial.read();
      i++;
      if (strstr(simAnswer, requiredAnswer) != NULL) {
        answer = 1;
      }
    }
  }
  while ((answer == 0) && ((millis() - lastTime) < lenghtTime));
  return answer;
}

void searchNetwork() {    //Funkcia na vyhľadávanie dostupných mobilných sieti
  String networkData, phoneOperator;
  int startData = -1, data1, data2, cops;
  while (Serial.available())(Serial.read());
  Serial.println("AT+COPS?");
  while (!Serial.available());
  while (Serial.available()) {
    networkData = Serial.readString();
  }
  if (networkData.indexOf("+COPS:") != -1) {
    data1 = networkData.indexOf(",", startData + 1);
    data2 = networkData.indexOf(",", data1 + 2);
    cops = networkData.indexOf("\n", data2 + 1);
    phoneOperator = networkData.substring(data2 + 2, cops - 2);
    sendDisplay("page0.t0.txt=\"" + phoneOperator + "\"");
  }
  networkData = "";
  phoneOperator = "";
  data1 = 0;
  cops = 0;
}

void makeCall(String dataMakeCall) {      //Funkcia na spustenie telefónneho hovoru
  while ((sendSIM("AT+CREG?", "+CREG: 0,1", 500) || sendSIM("AT+CREG?", "+CREG: 0,5", 500)) == 0);     //0,1 = domáca sieť, 0,5 = roaming
  sendDisplay("page2.t0.txt=\"Connecting\"");
  dataMakeCall.toCharArray(displayCommand, data.length());
  sprintf(simCommand, "ATD%s;", displayCommand);
  sendSIM(simCommand, "OK", 10000);
  sendDisplay("page2.t0.txt=\"Calling\"");
  memset(displayCommand, '\0', 30);
  memset(simCommand, '\0', 30);
}

void endCall(String dataEndCall) {   //Funkcia na ukončenie hovoru
  Serial.println(dataEndCall);
  delay(7000);
}

void pickUpCall(String dataPickUpCall) {    //Funkcia na prijatie hovoru
  Serial.println(dataPickUpCall);
  sendDisplay("page2.t0.txt=\"Connected\"");
  delay(7000);
}

void readAllSMS() {    //Funkcia na čítanie SMS správ z pamäte GSM modulu SIM900, normálne je veľkosť pamäte 20 SMS, ale na displeji sa zobrazuje iba 8 SMS
  int CPMS = -1, CPMS1, CPMS2, msgCount = 0;
  int CMGR = -1, CMGR1, CMGR2, SMSindex;
  String readCPMS, CPMSCount;
  String readCMGR, CMGRinside, SMSinside;
  Serial.println("AT+CPMS?");
  while (!Serial.available());
  while (Serial.available()) {
    readCPMS = Serial.readString();
  }
  if (readCPMS.indexOf("+CPMS:") != -1) {
    CPMS1 = readCPMS.indexOf(",", CPMS + 1);
    CPMS2 = readCPMS.indexOf(",", CPMS1 + 1);
    CPMSCount = readCPMS.substring(CPMS1 + 1, CPMS2);
    msgCount = CPMSCount.toInt();
    sendDisplay("page11.t41.txt=\"" + CPMSCount + "\"");
    int msgNumber = 0;
    while (Serial.available())(Serial.read());
    if (msgCount != 0) {
      for (int j = 20; j > 0; j--) {
        Serial.println("AT+CMGR=" + String(j, DEC));
        while (!Serial.available());
        while (Serial.available()) {
          readCMGR = Serial.readString();
        }
        if ((readCMGR.indexOf("+CMGR:") != -1) && (msgNumber < (50))) {
          CMGR1 = readCMGR.indexOf("+CMGR:", CMGR + 1);
          CMGR2 = readCMGR.indexOf("\n", CMGR1 + 1);
          CMGRinside = readCMGR.substring(CMGR1, CMGR2 - 1);
          SMSindex = readCMGR.indexOf("\n", CMGR2 + 1);
          SMSinside = readCMGR.substring(CMGR2 + 1, SMSindex - 1);
          readSMS(CMGRinside, SMSinside, j, msgNumber);
          msgNumber = msgNumber + 5;
        }
        CMGR = -1;
        CMGR1 = 0;
        CMGR2 = 0;
        SMSindex = 0;
        CMGRinside = "";
        SMSinside = "";
        readCMGR = "";
      }
    }
  }
  readCPMS = "";
  CPMSCount = "";
  CPMS = -1;
  CPMS1 = 0;
  CPMS2 = 0;
  msgCount = 0;
}

void sendSMS(String sendSMSContent) {       //Funkcia na odoslanie správy
  int firstSection = sendSMSContent.indexOf(byte(189));
  int secondSection = sendSMSContent.indexOf(byte(189), firstSection + 1);
  String smsContent = sendSMSContent.substring(0, firstSection);
  String phoneNumber = sendSMSContent.substring(firstSection + 1, secondSection);
  if (sendSIM("AT+CREG?", "+CREG: 0,1", 500) || sendSIM("AT+CREG?", "+CREG: 0,5", 500)) {
    if (sendSIM("AT+CMGF=1", "OK", 2500)) {
      phoneNumber.toCharArray(displayCommand, displayMsg.length());
      sprintf(simCommand, "AT+CMGS=\"%s\"", displayCommand);
      if (sendSIM(simCommand, ">", 2000)) {
        Serial.print(smsContent);
        Serial.write(0x1A);
        if (sendSIM("", "OK", 20000)) {
          Serial.println("message sent");
        }
        else {
          Serial.println("error sending");
        }
      }
      else {
        Serial.println("not found >");
      }
    }
  }
  smsContent = "";
  phoneNumber = "";
  memset(displayCommand, '\0', 30);
  memset(simCommand, '\0', 30);
}

void readSMS(String readinCMGR, String readinSMS, int readSMS , int numberSMS) {   //Funkcia na analýzu načítaných SMS správ z GSM modulu SIM900 a ich zobrazenie na displeji
  int startPartSMS = -1, firstNum, firstPartSMS, secondPartSMS, thirdPartSMS;
  String smsNum, smsCondition, telephoneNumber, smsDateTime, actualSMS;
  String startCommand = "page11.t", middleCommand = ".txt=\"";

  firstNum = readinCMGR.indexOf(":", startPartSMS + 1);
  firstPartSMS = readinCMGR.indexOf(",", startPartSMS + 1);
  secondPartSMS = readinCMGR.indexOf(",", firstPartSMS + 1);
  thirdPartSMS = readinCMGR.indexOf(",", secondPartSMS + 1);
  smsNum = String(readSMS);
  smsCondition = readinCMGR.substring(firstNum + 7, firstPartSMS - 1);
  telephoneNumber = readinCMGR.substring(firstPartSMS + 2, secondPartSMS - 1);
  smsDateTime = readinCMGR.substring(thirdPartSMS + 2, readinCMGR.length() - 1);
  actualSMS = readinSMS.substring(0, readinSMS.length());
  sendDisplay(startCommand + numberSMS + middleCommand + smsCondition + "\"");
  numberSMS++;
  sendDisplay(startCommand + numberSMS + middleCommand + telephoneNumber + "\"");
  numberSMS++;
  sendDisplay(startCommand + numberSMS + middleCommand + smsNum + "\"");
  numberSMS++;
  sendDisplay(startCommand + numberSMS + middleCommand + smsDateTime + "\"");
  numberSMS++;
  sendDisplay(startCommand + numberSMS + middleCommand + actualSMS + "\"");

  smsNum = "";
  smsCondition = "";
  telephoneNumber = "";
  smsDateTime = "";
  actualSMS = "";
  firstNum = 0;
  firstPartSMS = 0;
  secondPartSMS = 0;
  thirdPartSMS = 0;
  numberSMS = 0;
}

void loadingSMS(String dataSMS) {        //Funkcia na načítanie všetkých SMS z pamäte GSM modulu SIM900
  Serial.println(dataSMS);
}

void deleteSMS(String SMS) {             //Funkcia na vymazanie SMS z pamäte GSM modulu SIM900
  Serial.println(SMS);
}

void sendDisplay(String command) {          //Funkcia na odosielanie príkazov na displej
  for (int i = 0; i < command.length(); i++) {
    displaySerial.write(command[i]);
  }
  displaySerial.write(0xff);
  displaySerial.write(0xff);
  displaySerial.write(0xff);
}
