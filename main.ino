#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <EEPROM.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mp3Serial(11,10); // RX, TX
DFRobotDFPlayerMini mp3;

LiquidCrystal_I2C lcd(0x27,20,4);
DHT dht(2, DHT22);
RTC_DS1307 rtc;

// -------- PINOS --------
#define BTN_UP 3
#define BTN_DOWN 4
#define BTN_SELECT 5
#define BUZZER 8
#define LDR_PIN A0
#define LED_VERDE 6
#define LED_VERMELHO 7

// -------- BLUETOOTH --------
SoftwareSerial bluetooth(12,13);

// -------- ESTADOS --------
int tela = 0;
int opcao = 0;

// -------- CONFIG --------
bool usarFahrenheit;
int modoSistema;
bool sisIngles;
bool somOn;
bool modoSom;
bool modoLed;
int intervalo;
bool modoMonit;

// -------- SENSORES --------
float temperatura;
float umidade;
int luz;

// -------- LIMITES --------
float tempMin,tempMax;
float umidMin,umidMax;
float luzMax;

// -------- EEPROM  --------
#define MAX_REG 80
#define REG_SIZE 10
int baseEEPROM = 20;
int indexAtual = 0;
int ultimoMinutoSalvo = -1;

// -------- DEBOUNCE --------
unsigned long lastClick = 0;

// ---------------- SOM ----------------
void som(int faixa){
  mp3Serial.listen();
  static unsigned long ultimo = 0;

  if(millis() - ultimo > 400){
    mp3.play(faixa);
    ultimo = millis();
  }
}

// ---------------- BOOT ----------------
void bootGlitter(){
lcd.clear();
lcd.setCursor(6,1);
lcd.print("GLITTER");
lcd.setCursor(5,2);
lcd.print("COMPUTER");

tone(BUZZER,800); delay(100);
tone(BUZZER,1200); delay(100);
tone(BUZZER,1800); delay(150);
noTone(BUZZER);

delay(800);
lcd.clear();
}

// ---------------- SETUP ----------------
void setup(){

Serial.begin(9600);

mp3Serial.begin(9600);
mp3.begin(mp3Serial, false);
mp3.volume(25);

rtc.begin();
rtc.adjust(DateTime(F(_DATE),F(TIME_)));


lcd.init();
lcd.backlight();

dht.begin();
bluetooth.begin(9600);

pinMode(BTN_UP,INPUT_PULLUP);
pinMode(BTN_DOWN,INPUT_PULLUP);
pinMode(BTN_SELECT,INPUT_PULLUP);
pinMode(BUZZER,OUTPUT);
pinMode(LED_VERDE, OUTPUT);
pinMode(LED_VERMELHO, OUTPUT);

usarFahrenheit = EEPROM.read(0);
modoSistema = EEPROM.read(1);
sisIngles = EEPROM.read(2);
somOn = EEPROM.read(3);
indexAtual = EEPROM.read(5);
if(indexAtual >= MAX_REG) indexAtual = 0;
atualizarModo();
bootGlitter();
}

// ---------------- LOOP ----------------
void loop(){

lerSensores();
lerBluetooth();
verificarAlertas();
controleSalvamento();
attLeds();

if(tela==0) telaMonitor();
if(tela==1) telaMenu();
if(tela==2) telaSistema();
if(tela==3) telaModos();
if(tela==6) telaRegistros();
if (tela==7) telaDetalhes();
if (tela==4) telaBt();
}

// ---------------- SENSORES ----------------
void lerSensores(){

float t = dht.readTemperature();
float u = dht.readHumidity();

if(!isnan(t) && !isnan(u)){
temperatura = t;
umidade = u;
}

int v = analogRead(LDR_PIN);
luz = map(v,1023,0,0,100);
luz = constrain(luz,0,100);
}
// ---------------- LEDS ----------------
void attLeds(){
  if (!modoLed){
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, LOW);
    return;
  }
  if(alerta()){
    digitalWrite(LED_VERDE, LOW);
    if(millis()%600 <300) digitalWrite(LED_VERMELHO, HIGH);
    else digitalWrite(LED_VERMELHO, LOW);
  }
  else{
   digitalWrite(LED_VERDE, HIGH);
   digitalWrite(LED_VERMELHO, LOW);
  }
}
// ---------------- ALERTA ----------------
bool alerta(){

if (modoMonit==true) return false;
if(temperatura<tempMin || temperatura>tempMax) return true;
if(umidade<umidMin || umidade>umidMax) return true;
if(luz>luzMax) return true;

else return false;
}

void verificarAlertas(){
static unsigned long segs=0;

if(alerta()&& modoSom){
  if(millis()-segs>=10000){
    tone(BUZZER,1500);
    segs=millis();
  }
}else{
  noTone(BUZZER);
}
}

// ---------------- SALVAR ----------------
void controleSalvamento(){
DateTime now = rtc.now();
if(now.minute() != ultimoMinutoSalvo){
  if(alerta())salvarRegistro();
  else if(now.minute()%intervalo == 0) salvarRegistro();
ultimoMinutoSalvo = now.minute();
}
}


// ---------------- SALVAR REGISTRO ----------------
void salvarRegistro(){
DateTime now = rtc.now();
long tempo = now.unixtime();

int addr = baseEEPROM + (indexAtual * REG_SIZE);
int tempInt = temperatura * 100;
int umidInt = umidade * 100;
indexAtual++;

EEPROM.put(addr, tempo);
EEPROM.put(addr + 4, tempInt);
EEPROM.put(addr + 6, umidInt);
EEPROM.put(addr + 8, luz);
if(indexAtual>= MAX_REG)indexAtual= 0;
EEPROM.write(10,indexAtual);
indexAtual++;
if(indexAtual >= MAX_REG) indexAtual = 0;
}

// ---------------- BLUETOOTH ----------------
void enviarDados(){
  bluetooth.print("T: ");
  bluetooth.print(temperatura,1);
  bluetooth.print(usarFahrenheit?"F":"°C");
  bluetooth.print("  U: ");
  bluetooth.print(umidade,0);
  bluetooth.print("  L: ");
  bluetooth.print(luz);
  bluetooth.print("%");

  bluetooth.println();

}
void lerBluetooth(){

bluetooth.listen();

if(bluetooth.available()){

char c = bluetooth.read();

if(c=='D')enviarDados();
if(c=='0')
{
  modoSistema= 0;
  EEPROM.write(1,modoSistema);
  atualizarModo();
}
if(c=='1')
{
  modoSistema= 3;
  EEPROM.write(1,modoSistema);
  atualizarModo();
}
if(c=='2')
{
  modoSistema= 2;
  EEPROM.write(1,modoSistema);
  atualizarModo();
}
if(c=='3')
{
  modoSistema= 1;
  EEPROM.write(1,modoSistema);
  atualizarModo();
}
}
}

// ---------------- MONITOR ----------------
void telaMonitor(){

lcd.setCursor(0,0);
lcd.print(sisIngles?"Temperature:":"Temperatura:");
lcd.print(usarFahrenheit?temperatura*1.8+32:temperatura,1);

lcd.setCursor(0,1);
lcd.print(sisIngles?"Humidity:":"Umidade:");
lcd.print(umidade,0);

lcd.setCursor(0,2);
lcd.print(sisIngles?"Light:":"Luz:");
lcd.print(luz);

lcd.setCursor(0,3);
lcd.print(sisIngles?"SELECT = Menu":"SELECIONAR = Menu");

if(botaoSelect()){
tela=1;
opcao=0;
lcd.clear();
}
}

// ---------------- MENU ----------------
void telaMenu(){

const char* opcoes[4];

if (!sisIngles){
opcoes[0]= "Monitor";
opcoes[1]= "Registros";
opcoes[2]= "Sistema";
opcoes[3]= "Bluetooth";
}else{
opcoes[0]= "Monitor";
opcoes[1]= "Record";
opcoes[2]= "System";
opcoes[3]= "Bluetooth";
}

navegarMenu(4);

for(int i=0;i<4;i++){
lcd.setCursor(0,i);
lcd.print(opcao==i?"> ":" ");
lcd.print(opcoes[i]);
}

if(botaoSelect()){

if(opcao==0){tela=0; som(sisIngles?9:1);}
if(opcao==1){tela=6; som(sisIngles?25:2);}
if(opcao==2){tela=2; som(sisIngles?7:3);}
if(opcao==3){tela=4; som(sisIngles?10:5);}

lcd.clear();
}
}

// ---------------- SISTEMA ----------------
void telaSistema(){

const char* opcoes[5];

if (!sisIngles){
opcoes[0]= "Modos do sistema";
opcoes[1]= "Unidade temp";
opcoes[2]= "Idiomas";
opcoes[3]= "Audio";
opcoes[4]= "Voltar";
}else{
opcoes[0]= "System mode";
opcoes[1]= "Unit of temp";
opcoes[2]= "Language";
opcoes[3]= "Sound";
opcoes[4]= "Back";
}

navegarMenu(5);

int indiceSel=0; 
int linhaSel=0;
for(int i=0;i<4;i++){
int item = (indiceSel-linhaSel+i+5)%5;
lcd.setCursor(0,i);
lcd.print(i==linhaSel?"> ":" ");
lcd.print(opcoes[item]);

if(linhaSel<3){
  linhaSel++;
}
else{
  indiceSel = (indiceSel+1)%5;
}
if(linhaSel>0){
  linhaSel--;
}
else{
  indiceSel = (indiceSel-1+5)%5;
}

if(item==1) lcd.print(usarFahrenheit?" F":" C");
if(item==2) lcd.print(sisIngles?" English":" Portugues");
if (item ==3) lcd.print(somOn?"ON":"OFF");
}


if(botaoSelect()){

if(opcao==0){tela=3;opcao=0;
if (somOn)som(sisIngles?12:4);}
if(opcao==1){
if (somOn)som(sisIngles?14:6);
usarFahrenheit=!usarFahrenheit;
if (somOn){
  if (usarFahrenheit)som(31);
  else som(30);
}
EEPROM.write(0,usarFahrenheit);
}
if(opcao==2){
if (somOn)som(sisIngles?23:8);
sisIngles=!sisIngles;
if (somOn){
  if (sisIngles)som(28);
  else som(24);
}
EEPROM.write(2,sisIngles);
}
if(opcao==3){
somOn=!somOn;
EEPROM.write(3,somOn);
}
if(opcao == 4){tela=1;if (somOn)som(sisIngles?15:11);}
lcd.clear();
}
}

// ---------------- MODOS ----------------
void telaModos(){

const char* modos[4];

if(!sisIngles){
modos[0]="Desativado";
modos[1]="Noturno";
modos[2]="Estufa";
modos[3]="Ambiente";
}else{
modos[0]="Disabled";
modos[1]="Night mode";
modos[2]="Greenhouse";
modos[3]="Room mode";
}

navegarMenu(4);

for(int i=0;i<4;i++){
lcd.setCursor(0,i);
lcd.print(opcao==i?"> ":" ");
lcd.print(modos[i]);
}

if(botaoSelect()){
if(somOn){
  if (opcao == 0)som(sisIngles?16:26);
  else if (opcao == 1)som(sisIngles?17:13);
  else if (opcao == 2)som(sisIngles?18:20);
  else if (opcao == 3)som(sisIngles?21:22);
}
modoSistema=opcao;
EEPROM.write(1,modoSistema);
atualizarModo();

tela=2;
lcd.clear();
}
}

// ---------------- REGISTROS ----------------
void telaRegistros(){

navegarMenu(MAX_REG);

for (int i=0; i<3; i++){

int idx = (indexAtual - (opcao+i) - 1 + MAX_REG) % MAX_REG;
int addr = baseEEPROM + (idx * REG_SIZE);

unsigned long tempo;
EEPROM.get(addr, tempo);

lcd.setCursor(0,i);

if(tempo == 0xFFFFFFFF || tempo == 0){
lcd.print("Sem registro   ");
continue;
}

DateTime t = DateTime(tempo);

lcd.print(i==0?"> ":" ");
lcd.print(t.day());
lcd.print("/");
lcd.print(t.month());
lcd.print(" -- ");
lcd.print(t.hour());
lcd.print(":");
lcd.print(t.minute());
lcd.print("   ");
}

lcd.setCursor(0, 3);
lcd.print(sisIngles?"Select = see more":"OK= ver mais");

if(botaoSelect()){
tela=7;
lcd.clear();
}
}

// ---------------- DETALHES ----------------
void telaDetalhes(){

int idx = (indexAtual - opcao - 1 + MAX_REG) % MAX_REG;
int addr = baseEEPROM + (idx * REG_SIZE);

unsigned long tempo;
int tempInt, umidInt, luzInt;

EEPROM.get(addr, tempo);
EEPROM.get(addr + 4, tempInt);
EEPROM.get(addr + 6, umidInt);
EEPROM.get(addr + 8, luzInt);

DateTime t = DateTime(tempo);

lcd.setCursor(0,0);
lcd.print(t.day());
lcd.print("/");
lcd.print(t.month());
lcd.print(" -- ");
lcd.print(t.hour());
lcd.print(":");
lcd.print(t.minute());

lcd.setCursor(0,1);
lcd.print(sisIngles?"Temperature:":"Temperatura:");
lcd.print(usarFahrenheit? (tempInt/100.0)*1.8+32 : tempInt/100.0);

lcd.setCursor(0,2);
lcd.print(sisIngles?"Humidity:":"Umidade:");
lcd.print(umidInt/100.0);

lcd.setCursor(0,3);
lcd.print(sisIngles?"Light:":"Luz:");
lcd.print(luzInt);

if(botaoSelect()){
if (somOn)som(sisIngles?15:11);
tela=1;
lcd.clear();
}
}

// ---------------- BLUETOOTH CONECTA ----------------

void telaBt(){
  lcd.setCursor(0,0);
  lcd.print("GLITTER CONECTA app");
  lcd.setCursor(0,1);
  lcd.print("Nome: HC-05");
  lcd.setCursor(0,2);
  lcd.print("Senha: 1234");
  lcd.setCursor(0,3);
  lcd.print("VOLTAR> botao azul");

  if(botaoSelect()){
  if (somOn)som(sisIngles?15:11);
  tela=1;
  lcd.clear();
}
}
// ---------------- MODOS ----------------
void atualizarModo(){

switch(modoSistema){

case 0:
modoMonit = true;
modoLed= true;
modoSom = false;
intervalo= 2;
break;

case 1:
tempMin=15; tempMax=25;
umidMin=35; umidMax=60;
luzMax=10;
modoMonit = false;
modoLed= true;
modoSom = false;
intervalo= 10;
somOn=false;
break;

case 2:
tempMin=22; tempMax=30;
umidMin=50; umidMax=80;
luzMax=90;
modoMonit = false;
modoLed= true;
modoSom = true;
intervalo= 5;
break;

case 3:
tempMin=16; tempMax=26;
umidMin=40; umidMax=70;
luzMax=60;
modoMonit = false;
modoLed= true;
modoSom = false;
somOn= true;
intervalo= 2;
break;
}
}

// ---------------- INPUT ----------------
bool clique(){
if(millis()-lastClick<200) return false;
lastClick=millis();
return true;
}

bool botaoSelect(){
return digitalRead(BTN_SELECT)==LOW && clique();
}

void navegarMenu(int maxOp){

if(digitalRead(BTN_UP)==LOW && clique()){
opcao--;
if(opcao<0) opcao=maxOp-1;
}

if(digitalRead(BTN_DOWN)==LOW && clique()){
opcao++;
if(opcao>=maxOp) opcao=0;
}
}