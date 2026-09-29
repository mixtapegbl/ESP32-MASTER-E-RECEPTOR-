// ============================================================================
// NOME DO PROJETO: Sistema Multiversatio
// PLACA: Master ESP32-S3 
// OBJETIVO: Ler sensores e enviar via ESP-NOW com recuperação robusta de LCD I2C
// ============================================================================

#include <WiFi.h>              // Inclui a biblioteca para controle das funções WiFi do ESP32
#include <esp_now.h>           // Inclui a biblioteca do protocolo de comunicação ESP-NOW
#include <esp_wifi.h>          // Inclui a biblioteca para configurações de hardware do rádio WiFi
#include <Wire.h>              // Inclui a biblioteca para comunicação no barramento I2C
#include <LiquidCrystal_I2C.h> // Inclui a biblioteca para controle do Display LCD via módulo I2C
#include <DHT.h>               // Inclui a biblioteca para leitura do sensor de temperatura e umidade

// --- MAPEAMENTO DE PINOS ADAPTADO PARA ESP32-S3 ---
const int pinoLedVermelho = 36; // Define o pino GPIO 36 para controle do LED de alerta Vermelho
const int pinoLedVerde    = 37; // Define o pino GPIO 37 para controle do LED de status Verde
const int pinoLedPingAzul = 38; // Define o pino GPIO 38 para controle do LED indicador de transmissão Azul
const int pinoBuzzer      = 18; // Define o pino GPIO 18 para controle do buzzer de aviso sonoro
const int pinoDHT         = 7;  // Define o pino GPIO 7 para o barramento de dados do sensor DHT11
const int pinoLDR1        = 4;  // Define o pino GPIO 4 (Entrada Analógica) para o primeiro sensor de luz LDR
const int pinoLDR2        = 5;  // Define o pino GPIO 5 (Entrada Analógica) para o segundo sensor de luz LDR
// Pinos I2C: SDA fixado no GPIO 8 e SCL fixado no GPIO 9

// --- CONFIGURAÇÃO DOS SENSORES E PERIFÉRICOS ---
#define DHTTYPE DHT11                  // Define o modelo físico do sensor climátco como DHT11
DHT dht(pinoDHT, DHTTYPE);             // Inicializa o objeto dht passando o pino de dados e o modelo
LiquidCrystal_I2C lcd(0x27, 16, 2);    // Inicializa o LCD I2C no endereço padrão 0x27, com 16 colunas e 2 linhas

// --- MAC REAL DO SEU RECEPTOR ESP32-S3 V ---
uint8_t macReceptorReal[] = {0xA4, 0xCB, 0x8F, 0xD4, 0xAB, 0x34}; // Endereço físico do receptor de destino

// --- ESTRUTURA DE DADOS (STRUCT) ---
typedef struct struct_mensagem {
  int ldr1;          // Variável para armazenar a leitura bruta do LDR 1
  int ldr2;          // Variável para armazenar a leitura bruta do LDR 2
  int mediaLdr;      // Variável para armazenar a média calculada dos dois LDRs
  float temperatura; // Variável de ponto flutuante para armazenar a temperatura medida
  float humidade;    // Variável de ponto flutuante para armazenar a umidade medida
} struct_mensagem;

struct_mensagem meusDados;        // Cria uma instância da estrutura chamada meusDados para armazenar os envios
esp_now_peer_info_t informacoesPeer; // Cria o objeto que armazenará os dados de pareamento do receptor (Peer)

// --- VARIÁVEIS DE CONTROLE DE TEMPO (MILLIS) ---
unsigned long ultimoEnvio         = 0;    // Guarda o tempo do último pacote enviado via rádio
const int intervaloEnvio          = 1500; // Define o intervalo de transmissão dos dados para 1.5 segundos
unsigned long tempoUltimoBipErro   = 0;    // Guarda o tempo do último bipe emitido pelo alarme de falha
unsigned long tempoUltimoPiscaErro = 0;    // Guarda o tempo da última alternância de estado do LED vermelho

// --- FLAGS DE ESTADO DO SISTEMA ---
bool envioSucesso  = false; // Armazena o resultado da confirmação de entrega do pacote (ACK)
bool erroSensorDHT = false; // Flag que indica se o sensor DHT11 está desconectado ou quebrado

// --- ROTINA ANTI-CONGELAMENTO E ANTI-CARACTERES ESTRANHOS ROBUSTA PARA DISPLAY I2C ---
void verificarErepararDisplayI2C() {
  Wire.beginTransmission(0x27);         // Inicia uma comunicação teste com o endereço físico do LCD
  byte erroI2C = Wire.endTransmission(); // Encerra a transmissão e captura o código de retorno/erro
  
  // Se o display não responder (erro diferente de 0), ele travou, perdeu energia ou desconectou
  if (erroI2C != 0) {
    Serial.println("Alerta: Falha ou ruído no LCD I2C detectado! Reiniciando barramento...");
    
    Wire.end();        // Fecha de forma absoluta o barramento I2C que sofreu a pane elétrica
    delay(20);         // Janela de atraso para descarregar qualquer estática dos pinos físicos
    Wire.begin(8, 9);  // Inicializa novamente o barramento I2C mapeando explicitamente SDA (8) e SCL (9)
    delay(20);         // Aguarda a estabilização das linhas de clock e dados (Pull-ups)
    
    lcd.init();        // Força o controlador interno do display (HD44780/PCF8574) a resetar via hardware
    lcd.backlight();   // Liga novamente a luz de fundo que foi desligada no reset
    
    // --- BLINDAGEM CONTRA CARACTERES ESTRANHOS FANTASMAS ---
    lcd.clear();       // Limpa o buffer de caracteres gerados por picos de energia
    lcd.home();        // Força a reinicialização e ancoragem estável do cursor na posição (0,0)
    lcd.print("Display Reset!"); // Imprime aviso local de recuperação
    delay(300);        // Janela estável para leitura do operador antes do loop de telemetria
    lcd.clear();       // Deixa a tela perfeitamente limpa para receber as novas strings estáveis
  }
}

// --- CALLBACK DE ENVIO DO ESP-NOW ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  envioSucesso = (status == ESP_NOW_SEND_SUCCESS); // Define a flag como verdadeira se o receptor acusou recebimento
}

void setup() {
  Serial.begin(115200); // Inicializa a comunicação serial para monitoramento via PC a 115200 bps
  delay(1000);          // Aguarda um segundo para estabilização de tensões lógicas da placa

  // Garante a leitura correta de 0V a 3.3V nos pinos analógicos do S3
  analogSetAttenuation(ADC_11db); // Configura o atenuador do conversor ADC para ler a faixa total de tensão

  // Configuração das portas digitais como saídas
  pinMode(pinoLedVermelho, OUTPUT);  // Configura a porta do LED vermelho como saída digital
  pinMode(pinoLedVerde, OUTPUT);     // Configura a porta do LED verde como saída digital
  pinMode(pinoLedPingAzul, OUTPUT); // Configura a porta do LED azul (ping) como saída digital
  pinMode(pinoBuzzer, OUTPUT);       // Configura a porta do buzzer ativo como saída digital

  // Inicializa os atuadores desligados
  digitalWrite(pinoBuzzer, LOW);       // Garante que o alarme sonoro inicia desligado
  digitalWrite(pinoLedVermelho, LOW);  // Garante que o LED de erro inicia desligado
  digitalWrite(pinoLedVerde, LOW);     // Garante que o LED de link inicia desligado
  digitalWrite(pinoLedPingAzul, LOW); // Garante que o LED de atividade inicia desligado

  // Inicializa o barramento I2C forçando explicitamente SDA=8 e SCL=9 no S3
  Wire.begin(8, 9); 
  
  lcd.init();          // Inicializa o módulo LCD pela primeira vez
  lcd.backlight();     // Ativa a luz de fundo do painel
  lcd.clear();         // Limpa qualquer dado residual da tela
  lcd.print("Master S3 Online"); // Exibe a mensagem de inicialização bem-sucedida do transmissor

  dht.begin(); // Inicializa o sensor climático DHT11
  
  // Inicialização do rádio WiFi e ESP-NOW
  WiFi.mode(WIFI_STA); // Configura o rádio do chip obrigatoriamente no modo Station (Estação)
  
  if (esp_now_init() != ESP_OK) { // Inicializa o protocolo ESP-NOW e checa se ocorreu erro crítico
    ESP.restart(); // Se o rádio falhar na largada, força a placa a reiniciar por software
  }
  
  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent); // Vincula a função OnDataSent para monitorar envios
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);          // Tranca o rádio no Canal 1 para evitar ruídos
  
  // Configura os parâmetros do receptor (Peer)
  memset(&informacoesPeer, 0, sizeof(informacoesPeer));        // Limpa a região de memória do objeto peer
  memcpy(informacoesPeer.peer_addr, macReceptorReal, 6);       // Injeta o endereço MAC do receptor na configuração
  informacoesPeer.channel = 1;                                 // Define que a transmissão ocorrerá no canal 1
  informacoesPeer.encrypt = false;                             // Desativa criptografia para otimizar velocidade
  
  if (esp_now_add_peer(&informacoesPeer) != ESP_OK) { // Adiciona o receptor à lista de dispositivos pareados
    return; // Caso ocorra erro ao cadastrar o parceiro de rede, aborta a rotina
  }
}

void loop() {
  unsigned long tempoAtual = millis(); // Captura o tempo atual de execução do microcontrolador

  // --- LEITURA DO SENSOR DE TEMPERATURA E UMIDADE ---
  float h = dht.readHumidity();    // Executa a leitura da umidade atualizada
  float t = dht.readTemperature(); // Executa a leitura da temperatura atualizada

  if (isnan(h) || isnan(t)) { // Verifica se a leitura retornou um valor inválido (Not a Number)
    erroSensorDHT = true;          // Ativa a flag indicando avaria física no sensor de clima
    meusDados.temperatura = -99.0; // Injeta o valor padrão -99.0 indicando erro para o receptor
    meusDados.humidade    = -99.0; // Injeta o valor padrão -99.0 indicando erro para o receptor
  } else {
    erroSensorDHT = false;       // Desativa a flag indicando que as leituras estão operando normalmente
    meusDados.temperatura = t;   // Armazena a temperatura real lida dentro da estrutura de mensagem
    meusDados.humidade    = h;   // Armazena a umidade real lida dentro da estrutura de mensagem
  }

    // --- LEITURA DOS SENSORES DE LUZ (LDR) ---
  meusDados.ldr1     = analogRead(pinoLDR1);                  // Lê o conversor analógico do LDR 1 (0 a 4095)
  meusDados.ldr2     = analogRead(pinoLDR2);                  // Lê o conversor analógico do LDR 2 (0 a 4095)
  meusDados.mediaLdr = (meusDados.ldr1 + meusDados.ldr2) / 2; // Calcula a média matemática dos dois pontos de leitura

  // --- BLOCO DE ENVIO E ATUALIZAÇÃO DO DISPLAY LCD LOCAL ---
  if (tempoAtual - ultimoEnvio > intervaloEnvio) { // Executa o bloco se o intervalo de 1.5s foi atingido
    ultimoEnvio = tempoAtual;                      // Atualiza a marca temporal do último ciclo de envio
    
    // Testa, limpa e força o reset elétrico do LCD se houver congelamento por ruído
    verificarErepararDisplayI2C(); 
    
    lcd.setCursor(0, 0); // Posiciona o cursor na coluna 0 da linha 1
    lcd.printf("L1:%-4d L2:%-4d", meusDados.ldr1, meusDados.ldr2); // Exibe as leituras brutas de luz formatadas
    
    lcd.setCursor(0, 1); // Posiciona o cursor na coluna 0 da linha 2
    if (erroSensorDHT) {
      lcd.print("ERRO NO DHT11   "); // Caso a flag de erro esteja alta, avisa o operador no display local
    } else {
      if (envioSucesso) {
        lcd.printf("T:%-4.1fC H:%-3.0f%% ", meusDados.temperatura, meusDados.humidade); // Exibe clima com status OK
      } else {
        lcd.printf("T:%-4.1fC H:%-3.0f%%*", meusDados.temperatura, meusDados.humidade); // Adiciona asterisco se houver queda de sinal
      }
    }

    esp_now_send(macReceptorReal, (uint8_t *) &meusDados, sizeof(meusDados)); // Despacha a struct via rádio para o receptor
  }

  // --- MÁQUINA DE ALERTAS (SEM BLOQUEIOS) ---
  if (erroSensorDHT) { // Se houver erro físico no sensor local DHT11
    digitalWrite(pinoLedVerde, LOW);    // Apaga imediatamente o LED indicador de estabilidade
    digitalWrite(pinoLedPingAzul, LOW); // Apaga o LED indicador de atividade de transmissão
    
    if (tempoAtual - tempoUltimoPiscaErro > 250) { // Cria o oscilador de pisca-pisca do LED vermelho a cada 250ms
      tempoUltimoPiscaErro = tempoAtual;            // Sincroniza o tempo do oscilador visual
      digitalWrite(pinoLedVermelho, !digitalRead(pinoLedVermelho)); // Inverte o estado do LED vermelho
    }
    if (tempoAtual - tempoUltimoBipErro > 2000) { // Dispara um bipe duplo de alerta curto a cada 2 segundos
      tempoUltimoBipErro = tempoAtual;             // Sincroniza o tempo do oscilador sonoro
      tone(pinoBuzzer, 1800, 150);                 // Emite um bipe agudo com duração estável de 150ms
    }
  } 
  else if (!envioSucesso) { // Se o rádio não receber confirmação de entrega do Receptor (Link Caído)
    digitalWrite(pinoLedVerde, LOW);     // Desliga o LED indicador de comunicação activa
    digitalWrite(pinoLedVermelho, HIGH); // Força o LED vermelho a ficar aceso de forma fixa (Erro de Sinal)
    digitalWrite(pinoLedPingAzul, LOW);  // Desliga o LED azul de transmissão ativo
    
    if (tempoAtual - tempoUltimoBipErro > 1000) { // Emite um aviso sonoro contínuo e longo de 1 em 1 segundo
      tone(pinoBuzzer, 1500, 300);                // Emite som de falha na rede de 1500Hz com duração de 300ms
      tempoUltimoBipErro = tempoAtual;            // Sincroniza o tempo do alarme de rede
    }
  } 
  else { // Se o sistema estiver com os sensores operando e a rede sem fio conectada com sucesso
    digitalWrite(pinoLedVerde, HIGH);    // Acende o LED verde de forma contínua indicando sucesso absoluto
    digitalWrite(pinoLedVermelho, LOW);  // Garante que o LED vermelho de erros permanece apagado
    
    tempoUltimoBipErro = 0; // Zera a marca temporal garantindo disparo sonoro imediato se o sinal cair repentinamente
    
    static unsigned long ultimoPiscaPing = 0; // Cria a variável estática local para controlar o LED indicador azul
    if (tempoAtual - ultimoPiscaPing > 100) { // Cria o oscilador rápido de atividade de rede (100ms)
      ultimoPiscaPing = tempoAtual;            // Sincroniza a marca do ping
      digitalWrite(pinoLedPingAzul, !digitalRead(pinoLedPingAzul)); // Alterna o LED azul gerando efeito pulsar rápido
    }
  }
  
  delay(1); // Micro-atraso estável de 1 milissegundo para alimentar o Watchdog do RTOS do ESP32-S3
}
