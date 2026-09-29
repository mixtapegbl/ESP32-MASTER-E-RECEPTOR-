// ============================================================================
// NOME DO PROJETO: Sistema Multiversatio
// PLACA: Receptor ESP32-S3 V (Endereço MAC: A4:CB:8F:D4:AB:34)
// OBJETIVO: Receber dados do Master E, gerenciar LEDs NeoPixel e exibir o gráfico
// ============================================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <LiquidCrystal.h>
#include <Adafruit_NeoPixel.h>

// --- SEU MAPEAMENTO DE PINOS REAL PRESERVADO ---
const int pinoBotaoModo   = 15; 
const int pinoBotaoLiga   = 8;  
const int pinoBotaoSobe   = 16; 
const int pinoBotaoDesce  = 17; 

const int pinoBuzzer      = 47; 
const int pinoBacklight   = 41; 
const int pinoLedPingAzul = 9;  

const int pinoWs2812Temp  = 11; 
const int pinoWs2812Hum   = 12; 
const int pinoWs2812Luz   = 13; 

// --- CONFIGURAÇÃO DE OBJETOS ---
Adafruit_NeoPixel pixelTemp(1, pinoWs2812Temp, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelHum(1, pinoWs2812Hum, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixelLuz(1, pinoWs2812Luz, NEO_GRB + NEO_KHZ800);

LiquidCrystal lcd(1, 2, 4, 5, 6, 7);

// --- ESTRUTURA DE DADOS (STRUCT) ---
typedef struct struct_mensagem {
  int ldr1;
  int ldr2;
  int mediaLdr;
  float temperatura;
  float humidade;
} struct_mensagem;

struct_mensagem dadosRecebidos;

// MAC do Master esperado (ESP32-S3 E)
const uint8_t macMasterEsperado[] = {0xA4, 0xCB, 0x8F, 0xD3, 0x4F, 0x00}; 

unsigned long ultimaMensagemRecebida = 0;
const unsigned long tempoLimiteMensagem = 7000; 
unsigned long ultimoBipErro = 0;                 

bool falhaComunicacao = false; 
bool painelLigado     = true;  
int modoExibicao      = 0;     
int escalaGrafico     = 50;    
int ultimoRssi        = -100;  

// Matriz do Gráfico
const byte barra[8][8] = {
  {0, 0, 0, 0, 0, 0, 0, 31},  
  {0, 0, 0, 0, 0, 0, 31, 31}, 
  {0, 0, 0, 0, 0, 31, 31, 31},
  {0, 0, 0, 0, 31, 31, 31, 31},
  {0, 0, 0, 31, 31, 31, 31, 31},
  {0, 0, 31, 31, 31, 31, 31, 31},
  {0, 31, 31, 31, 31, 31, 31, 31},
  {31, 31, 31, 31, 31, 31, 31, 31}
};

int historicoDados[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// --- FUNÇÕES DE SUPORTE ---

void recriarCaracteresGrafico() {
  for (int i = 0; i < 8; i++) {
    lcd.createChar(i, (uint8_t*)barra[i]);
    delay(10); 
  }
}

uint32_t calcularCorSuave(Adafruit_NeoPixel &pixel, int percentual) {
  percentual = constrain(percentual, 0, 100);
  int r = 0, g = 0, b = 0;
  if (percentual < 50) {
    r = map(percentual, 0, 50, 0, 255);
    g = 255;
  } else {
    r = 255;
    g = map(percentual, 50, 100, 255, 0);
  }
  return pixel.Color(r, g, b);
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (memcmp(recv_info->src_addr, macMasterEsperado, 6) == 0) {
    memcpy(&dadosRecebidos, data, sizeof(dadosRecebidos));
    ultimaMensagemRecebida = millis();
    falhaComunicacao = false;
    
    ultimoRssi = recv_info->rx_ctrl->rssi;
    int brilhoPing = map(ultimoRssi, -95, -30, 0, 255);
    brilhoPing = constrain(brilhoPing, 0, 255);
    analogWrite(pinoLedPingAzul, brilhoPing);
  }
}

// --- FRAMES DA ANIMAÇÃO DA GARRAFA (Alinhados antes do loop) ---

void frame1() { 
  lcd.clear();
  byte image01[] = { B00000, B11111, B11111, B01110, B00100, B00100, B00100, B01110 };
  lcd.createChar(0, image01); 
  lcd.setCursor(7, 0); lcd.write(byte(0)); 
}

void frame2() {
  lcd.clear();
  byte image01[] = { B00000, B00000, B11000, B00100, B01111, B11100, B11000, B00000 };
  byte image02[] = { B00000, B00000, B00000, B01000, B11000, B01000, B00000, B00000 };
  byte image03[] = { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00001 };
  byte image04[] = { B00011, B00010, B00000, B00010, B00000, B00000, B00000, B00000 };
  lcd.createChar(0, image01); lcd.createChar(1, image02);
  lcd.createChar(2, image03); lcd.createChar(3, image04);
  lcd.setCursor(7, 0); lcd.write(byte(0));
  lcd.setCursor(8, 0); lcd.write(byte(1));
  lcd.setCursor(6, 0); lcd.write(byte(2));
  lcd.setCursor(6, 1); lcd.write(byte(3));
}

void frame3() {
  lcd.clear();
  byte image01[] = { B00000, B00000, B11000, B00100, B00011, B00100, B11000, B00000 };
  byte image02[] = { B00000, B00000, B00000, B01000, B11000, B01000, B00000, B00000 };
  byte image03[] = { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00001 };
  byte image04[] = { B00001, B00011, B00011, B00110, B00010, B00000, B00010, B00000 };
  lcd.createChar(0, image01); lcd.createChar(1, image02);
  lcd.createChar(2, image03); lcd.createChar(3, image04);
  lcd.setCursor(7, 0); lcd.write(byte(0));
  lcd.setCursor(8, 0); lcd.write(byte(1));
  lcd.setCursor(6, 0); lcd.write(byte(2));
  lcd.setCursor(6, 1); lcd.write(byte(3));
}

void frame4() {
  lcd.clear();
  byte image01[] = { B00000, B00001, B00000, B00001, B00010, B00001, B00011, B00110 };
  byte image2[]  = { B00010, B00000, B00000, B00000, B00000, B00000, B00000, B00000 };
  lcd.createChar(0, image01); lcd.createChar(1, image2);
  lcd.setCursor(6, 0); lcd.write(byte(0));
  lcd.setCursor(6, 1); lcd.write(byte(1));
}

void frame5() {
  lcd.clear();
  byte image01[] = { B00010, B00100, B00100, B01100, B11000, B01001, B11011, B01110 };
  byte image02[] = { B00000, B00000, B00000, B10100, B11000, B10000, B10000, B00000 };
  lcd.createChar(0, image01); lcd.createChar(1, image02);
  lcd.setCursor(6, 0); lcd.write(byte(0));
  lcd.setCursor(7, 0); lcd.write(byte(1));
}

void frame6() {
  lcd.clear();
  byte image01[] = { B00010, B00100, B00100, B01100, B11000, B01001, B11011, B01110 };
  byte image02[] = { B00000, B00000, B00000, B10110, B11001, B10000, B10000, B10000 };
  byte image03[] = { B00000, B00000, B00000, B01110, B10001, B01111, B11001, B01111 };
  byte image4[]  = { B00001, B10001, B01110, B00000, B00000, B00000, B00000, B00000 };
  lcd.createChar(0, image01); lcd.createChar(1, image02);
  lcd.createChar(2, image03); lcd.createChar(3, image4);
  lcd.setCursor(6, 0); lcd.write(byte(0));
  lcd.setCursor(7, 0); lcd.write(byte(1));
  lcd.setCursor(8, 0); lcd.write(byte(2));
  lcd.setCursor(6, 1); lcd.write(byte(3));
}

void frame7() {
  lcd.clear();
  byte image01[] = { B00000, B00000, B00000, B01110, B11001, B01101, B11111, B01110 };
  byte image02[] = { B00000, B00000, B00000, B10110, B11001, B10000, B10000, B10000 };
  byte image03[] = { B00000, B00000, B00000, B01110, B10001, B01111, B11001, B01111 };
  byte image04[] = { B00001, B10001, B01110, B00000, B00000, B00000, B00000, B00000 };
  byte image05[] = { B00000, B00000, B00000, B10110, B11001, B10001, B11110, B10000 };
  byte image06[] = { B10000, B10000, B10000, B00000, B00000, B00000, B00000, B00000 };
  byte image07[] = { B00000, B00000, B00000, B01110, B10001, B11111, B10000, B01111 };
  lcd.createChar(0, image01); lcd.createChar(1, image02);
  lcd.createChar(2, image03); lcd.createChar(3, image04);
  lcd.createChar(4, image05); lcd.createChar(5, image06);
  lcd.createChar(6, image07);
  lcd.setCursor(6, 0); lcd.write(byte(0));
  lcd.setCursor(7, 0); lcd.write(byte(1));
  lcd.setCursor(8, 0); lcd.write(byte(2));
  lcd.setCursor(6, 1); lcd.write(byte(3));
  lcd.setCursor(9, 0); lcd.write(byte(4));
  lcd.setCursor(9, 1); lcd.write(byte(5));
  lcd.setCursor(10, 0); lcd.write(byte(6));
}

void executarAnimacaoAbertura() {
  frame1(); delay(600);
  frame2(); delay(400);
  frame3(); delay(400);
  frame4(); delay(500);
  frame5(); delay(300);
  frame6(); delay(300);
  frame7(); delay(1000);

  lcd.clear();
  lcd.setCursor(0, 0);    
  lcd.print("Bem-vindo(a) a"); 
  lcd.setCursor(0, 1); 
  lcd.print("Adega Agnello!"); 
  delay(1200);
  
  for (int x = 0; x < 16; x++) { 
    lcd.scrollDisplayRight(); 
    delay(100);
  }
  lcd.clear();
  
  recriarCaracteresGrafico(); 
}

// --- SETUP INICIAL ---
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Receptor esp32 s3 V Inicializando...");
  
  pinMode(pinoBotaoLiga, INPUT_PULLUP);
  pinMode(pinoBotaoModo, INPUT_PULLUP);
  pinMode(pinoBotaoSobe, INPUT_PULLUP);
  pinMode(pinoBotaoDesce, INPUT_PULLUP);
  
  pinMode(pinoBuzzer, OUTPUT);
  pinMode(pinoBacklight, OUTPUT);
  pinMode(pinoLedPingAzul, OUTPUT);
  
  digitalWrite(pinoBuzzer, LOW);
  digitalWrite(pinoBacklight, HIGH); 

  pixelTemp.begin(); pixelTemp.setBrightness(60);
  pixelHum.begin();  pixelHum.setBrightness(60);
  pixelLuz.begin();  pixelLuz.setBrightness(60);

  lcd.begin(16, 2);
  delay(50); 

  recriarCaracteresGrafico();
  
  for (int i = 0; i < 16; i++) {
    historicoDados[i] = 0;
  }

  lcd.clear();
  lcd.print("S3 Conectando...");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao inicializar ESP-NOW.");
    ESP.restart();
  }
  
  esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
  ultimaMensagemRecebida = millis();
}

// --- LOOP PRINCIPAL (Garantido sem erros de escopo) ---
void loop() {
  unsigned long tempoAtual = millis();
  
  if (tempoAtual - ultimaMensagemRecebida > tempoLimiteMensagem) {
    falhaComunicacao = true;
    
    pixelTemp.setPixelColor(0, pixelTemp.Color(255, 0, 0));
    pixelHum.setPixelColor(0, pixelHum.Color(255, 0, 0));
    pixelLuz.setPixelColor(0, pixelLuz.Color(255, 0, 0));
    pixelTemp.show(); pixelHum.show(); pixelLuz.show();

    if (tempoAtual - ultimoBipErro > 2000) {
      ultimoBipErro = tempoAtual;
      tone(pinoBuzzer, 1200, 300); 
      lcd.clear();
      lcd.print("SEM SINAL MASTER");
    }
  }

  if (digitalRead(pinoBotaoModo) == LOW) { 
    modoExibicao = (modoExibicao == 0) ? 1 : 0;
    lcd.clear(); 
    delay(250);  
  }

  if (digitalRead(pinoBotaoLiga) == LOW) {
    painelLigado = !painelLigado;
    
    if (painelLigado) {
      digitalWrite(pinoBacklight, HIGH);
      executarAnimacaoAbertura(); 
    } else {
      digitalWrite(pinoBacklight, LOW);
      lcd.clear();
    }
    delay(250); 
  }

  if (painelLigado) {
    if (digitalRead(pinoBotaoSobe) == LOW) { 
      escalaGrafico += 5; 
      delay(150); 
    }
    // Botão de Descer: Diminui o valor de referência da escala do gráfico
    if (digitalRead(pinoBotaoDesce) == LOW) { 
      escalaGrafico -= 5; 
      if (escalaGrafico < 5) {
        escalaGrafico = 5; // Protege para a escala nunca ser menor ou igual a zero
      }
      delay(150); 
    }
  }

  // --- PROCESSING VISUAL (Apenas opera se houver link ativo com o Master) ---
  if (!falhaComunicacao) {
    // Converte a média analógica dos LDRs para uma escala percentual (0 a 100%)
    int ldrPorcentagem = map(dadosRecebidos.mediaLdr, 0, 4095, 0, 100);
    ldrPorcentagem = constrain(ldrPorcentagem, 0, 100);

    // Ajusta individualmente as grandezas de cada sensor para as funções de cor
    int pctTemp = map((int)dadosRecebidos.temperatura, 15, 40, 0, 100); 
    int pctHum  = map((int)dadosRecebidos.humidade, 30, 85, 0, 100);    
    int pctLuz  = ldrPorcentagem;                                      

    // Calcula e injeta o gradiente suave nos anéis de LED NeoPixel
    pixelTemp.setPixelColor(0, calcularCorSuave(pixelTemp, pctTemp));
    pixelHum.setPixelColor(0, calcularCorSuave(pixelHum, pctHum));
    pixelLuz.setPixelColor(0, calcularCorSuave(pixelLuz, pctLuz));
    
    // Dispara a atualização física de hardware dos pixels
    pixelTemp.show(); 
    pixelHum.show(); 
    pixelLuz.show();

    // --- ATUALIZAÇÃO DO DISPLAY LCD ---
    if (painelLigado) {
      if (modoExibicao == 0) {
        // MODO 0: Visão textual limpa das leituras atuais
        lcd.setCursor(0, 0);
        lcd.printf("T:%-4.1fC H:%-3.0f%%", dadosRecebidos.temperatura, dadosRecebidos.humidade);
        lcd.setCursor(0, 1);
        lcd.printf("luz adegaria: %-3d%% ", ldrPorcentagem);
      } 
      else {
        // MODO 1: Gráfico horizontal animado deslizante
        static unsigned long ultimoUpdateGrafico = 0;
        
        if (tempoAtual - ultimoUpdateGrafico > 500) { // Atualiza a cada 0.5s
          ultimoUpdateGrafico = tempoAtual;
          
          // Desloca o vetor de memórias uma casa para a esquerda (Efeito esteira)
          for (int i = 0; i < 15; i++) { 
            historicoDados[i] = historicoDados[i + 1]; 
          }
          
          // Desenha a nova leitura estritamente no índice 15 (Canto direito do LCD)
          int novaBarra = map(ldrPorcentagem, 0, 100, 0, 7); 
          historicoDados[15] = constrain(novaBarra, 0, 7); 
        }
        
        // Linha 1 do modo gráfico: Monitor de texto auxiliar
        lcd.setCursor(0, 0);
        lcd.print("LDR:");
        lcd.print(ldrPorcentagem);
        lcd.print("%  Esc:");
        lcd.print(escalaGrafico);
        lcd.print("   "); // Limpa resquícios de dígitos fantasmas antigos
        
        // Linha 2 do modo gráfico: Renderiza as 16 colunas customizadas
        lcd.setCursor(0, 1);
        for (int i = 0; i < 16; i++) { 
          lcd.write((byte)historicoDados[i]); 
        }
      }
    }
  }
  delay(30); // Mantém o RTOS e Watchdog do ESP32-S3 operando de forma estável
}
