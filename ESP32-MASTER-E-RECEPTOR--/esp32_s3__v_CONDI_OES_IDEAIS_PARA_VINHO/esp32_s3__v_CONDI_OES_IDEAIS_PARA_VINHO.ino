// =========================================================================
// PROJETO FIAP: SISTEMA MULTIVERSATIO - MONITORAMENTO AUTOMATIZADO PARA A VINHERIA AGNELLO
// CÓDIGO DO RECEPTOR ESP32-S3 V COM ANIMAÇÃO GRÁFICA, HISTÓRICO E DUPLO BUZZER
// GRUPO: CTRL + 5
// INTEGRANTES DO GRUPO: Eduardo, Flávia, Gabriel, Lirity e Nicolle
// =========================================================================
#include <WiFi.h> // Inclui a biblioteca responsável por gerenciar as funções de Wi-Fi nativas do chip ESP32
#include <esp_now.h> // Inclui a biblioteca do protocolo ESP-NOW para realizar comunicação sem fio rápida entre placas
#include <esp_wifi.h> // Inclui recursos adicionais para controle de baixo nível do hardware de rádio Wi-Fi
#include <LiquidCrystal.h> // Inclui a biblioteca de controle para displays de texto LCD de 16 colunas e 2 linhas
#include <Adafruit_NeoPixel.h> // Inclui a biblioteca para comando e envio de cores para LEDs endereçáveis WS2812B

// --- MAPEAMENTO DE PINOS PRESERVADO E ADIÇÃO DO BUZZER 2 ---
const int pinoBotaoModo  = 15; // Define que o pino físico 15 monitora o botão que altera as telas do visor
const int pinoBotaoLiga  = 8;  // Define que o pino físico 8 monitora o botão que liga ou desliga o visor
const int pinoBotaoSobe  = 16; // Define que o pino físico 16 monitora o botão de aumentar a escala do gráfico
const int pinoBotaoDesce = 17; // Define que o pino físico 17 monitora o botão de diminuir a escala do gráfico

const int pinoBuzzer1     = 47; // BUZZER 1: Pino 47 dedicado ao retorno sonoro dos cliques de botões
const int pinoBuzzer2     = 46; // BUZZER 2: Novo Pino 46 dedicado a Alertas de Parâmetros e Desconexão (5V)
const int pinoBacklight   = 41; // Define que o pino digital 41 envia energia para acender a luz de fundo do LCD
const int pinoLedPingAzul = 9;  // Define que o pino digital 9 controla o LED que indica recepção de sinal de rádio

const int pinoWs2812Temp = 11; // Define que o pino digital 11 controla o LED NeoPixel indicador de Temperatura
const int pinoWs2812Hum  = 12; // Define que o pino digital 12 controla o LED NeoPixel indicador de Umidade
const int pinoWs2812Luz  = 13; // Define que o pino digital 13 controla o LED NeoPixel indicador de Luminosidade

// Inicializa o controle do LED de temperatura contendo 1 pixel no pino 11 usando o padrão de cores GRB
Adafruit_NeoPixel pixelTemp(1, pinoWs2812Temp, NEO_GRB + NEO_KHZ800);
// Inicializa o controle do LED de umidade contendo 1 pixel no pino 12 usando o padrão de cores GRB
Adafruit_NeoPixel pixelHum(1, pinoWs2812Hum, NEO_GRB + NEO_KHZ800);
// Inicializa o controle do LED de luminosidade contendo 1 pixel no pino 13 usando o padrão de cores GRB
Adafruit_NeoPixel pixelLuz(1, pinoWs2812Luz, NEO_GRB + NEO_KHZ800);

// Configura a pinagem física do visor LCD utilizando os pinos digitais 1, 2, 4, 5, 6 e 7 da placa receptor
LiquidCrystal lcd(1, 2, 4, 5, 6, 7);

// Cria um modelo estruturado de empacotamento para organizar os diferentes tipos de dados recebidos pelo ar
typedef struct struct_mensagem {
  int ldr1; // Reserva um espaço numérico inteiro para armazenar a leitura do sensor de luz esquerdo
  int ldr2; // Reserva um espaço numérico inteiro para armazenar a leitura do sensor de luz direito
  int mediaLdr; // Reserva um espaço numérico inteiro para guardar a média calculada da iluminação ambiental
  float temperatura; // Reserva um espaço para número decimal que guardará a temperatura lida na adega
  float humidade; // Reserva um espaço para número decimal que guardará a umidade lida na adega
} struct_mensagem;

struct_mensagem dadosRecebidos; // Cria a variável global oficial que armazenará o pacote de dados vindo do transmissor

unsigned long ultimaMensagemRecebida = 0; // Armazena a contagem de tempo de quando o último sinal válido chegou à placa
const unsigned long tempoLimiteMensagem = 7000; // Estipula o tempo limite de 7 segundos antes de decretar perda de sinal
unsigned long ultimoTempoAtividade = 0; // Guarda a última marcação de tempo em que o usuário interagiu com os botões
const unsigned long tempoLimitePainel = 30000; // Estipula 30 segundos de inatividade para proteção do painel de controle

bool falhaComunicacao = false; // Estado lógico que aponta se a comunicação caiu (falso significa que está estável)
bool painelLigado = true; // Estado lógico que controla se a tela e luz de fundo do visor devem permanecer ativas
int modoExibicao = 0; // Controla qual tela está ativa no visor (0 para textos normais, 1 para o gráfico deslizante)
int escalaGrafico = 50; // Determina o teto matemático inicial de proporção vertical das barras de histórico do visor
int ultimoRssi = -100; // Armazena a potência do sinal eletromagnético da rede, iniciando com valor baixo padrão

// --- MAC DO TRANSMISSOR AUTORIZADO MASTER ---
const uint8_t macMasterEsperado[] = {0xA4, 0xCB, 0x8F, 0xD3, 0x4F, 0x00};

// Matriz do Gráfico (Alturas de 0 a 7)
const byte barra[8][8] = {
  {0, 0, 0, 0, 0, 0, 0, 31}, {0, 0, 0, 0, 0, 0, 31, 31}, 
  {0, 0, 0, 0, 0, 31, 31, 31}, {0, 0, 0, 0, 31, 31, 31, 31},
  {0, 0, 0, 31, 31, 31, 31, 31}, {0, 0, 31, 31, 31, 31, 31, 31}, 
  {0, 31, 31, 31, 31, 31, 31, 31}, {31, 31, 31, 31, 31, 31, 31, 31}
};

int historicoDados[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// --- FUNÇÃO PARA CARREGAR OS CARACTERES DO GRÁFICO ---
void recriarCaracteresGrafico() {
  for (int i = 0; i < 8; i++) {
    lcd.createChar(i, (uint8_t*)barra[i]);
    delay(10);
  }
}

// --- FUNÇÃO AUXILIAR DE SOM NATIVA PARA ESP32-S3 (LEDC POR HARDWARE) ---
void emitirSom(int pino, int frequencia, int duracaoMs) {
  ledcWriteTone(pino, frequencia); // Liga o gerador de onda na frequência exata por hardware no pino escolhido
  delay(duracaoMs);                // Mantém o som tocando pelo tempo estipulado
  ledcWriteTone(pino, 0);          // Desliga o sinal do pino silenciando o circuito
}

// --- FRAMES DA ANIMAÇÃO DA GARRAFA (ESTREITOS PARA EVITAR QUEBRAS) ---
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
  lcd.setCursor(9, 0); 
  lcd.write(byte(4)); // Desenha a parte 4 do frame na coluna 9 da linha superior
  lcd.setCursor(9, 1); 
  lcd.write(byte(5)); // Desenha a parte 5 do frame na coluna 9 da linha inferior
  lcd.setCursor(10, 0); 
  lcd.write(byte(6)); // Desenha a parte 6 do frame na coluna 10 da linha superior
}

// --- FUNÇÃO PARA EXECUTAR A SEQUÊNCIA DE ABERTURA ---
void executarAnimacaoAbertura() {
  frame1(); delay(800); // Dispara o frame 1 e segura a imagem estática por 800 milissegundos
  frame2(); delay(600); // Dispara o frame 2 e segura a imagem estática por 600 milissegundos
  frame3(); delay(500); // Dispara o frame 3 e segura a imagem estática por 500 milissegundos
  frame4(); delay(700); // Dispara o frame 4 e segura a imagem estática por 700 milissegundos
  frame5(); delay(400); // Dispara o frame 5 e segura a imagem estática por 400 milissegundos
  frame6(); delay(400); // Dispara o frame 6 e segura a imagem estática por 400 milissegundos
  frame7(); delay(1350); // Dispara o frame 7 de encerramento do movimento por 1,35 segundos

  lcd.clear(); // Limpa completamente o visor para preparar as mensagens textuais oficiais de boas-vindas
  lcd.setCursor(0, 0); // Desloca o início do cursor de digitação para a primeira coluna da linha superior
  lcd.print("Bem-vindo(a) a"); // Imprime o texto inicial de recepção e apresentação do sistema
  lcd.setCursor(0, 1); // Desloca o início do cursor de digitação para a primeira coluna da linha inferior
  lcd.print("Adega Agnello!"); // Identifica textualmente o estabelecimento de vinhos monitorado
  delay(1500); // Trava a tela com o letreiro completo visível por 1,5 segundos para permitir leitura completa
  
  for(int x = 0; x < 16; x++) { // Inicia um laço de repetição exato que rodará os comandos internos por 16 ciclos
    lcd.scrollDisplayRight(); // Desloca fisicamente todas as mensagens do visor um bloco para o lado direito
    delay(150); // Aguarda 150 milissegundos em cada ciclo de movimento para criar um efeito visual fluido de saída
  }
  lcd.clear(); // Apaga do controlador qualquer pixel fantasma que tenha sobrado após a rolagem lateral
  
  // Recria os caracteres das barrinhas do histórico para o loop rodar limpo
  recriarCaracteresGrafico(); // Executa a recriação das barras de medição originais para o gráfico iniciar limpo
}

// --- CÁLCULO DE COR COM BASE NAS CONDIÇÕES ESTREITAS DA VINHARIA AGNELLO ---
// Desenvolvido para que os LEDs reflitam exatamente os limites reais de vinhos finos (Temperatura ideal: 12°C a 16°C / Umidade ideal: 60% a 80%)
uint32_t calcularCorSuave(Adafruit_NeoPixel &pixel, int percentual) {
  percentual = constrain(percentual, 0, 100); // Limita o valor percentual recebido para ficar estritamente na faixa de segurança de 0 a 100%
  int r = 0, g = 0, b = 0; // Inicia variáveis de armazenamento de brilho local para Vermelho, Verde e Azul zeradas
  if (percentual < 50) { // Se o nível atual estiver dentro da faixa de desvio aceitável inicial
    r = map(percentual, 0, 50, 0, 255); // Eleva suavemente a luz vermelha à medida que o ambiente se desloca em direção ao centro
    g = 255; // Segura o brilho da cor verde em sua potência máxima operacional
  } else { // Se o desvio medido romper as condições adequadas exigidas pelo controle de qualidade rigoroso da Agnello
    r = 255; // Crava a potência da coloração vermelha no limite máximo para disparar o aviso de risco
    g = map(percentual, 50, 100, 255, 0); // Apaga gradativamente a coloração verde gerando uma transição que termina em vermelho vivo
  }
  return pixel.Color(r, g, b); // Retorna a combinação binária exata de cor gerada para gravação no LED WS2812B correspondente
}

// --- CALLBACK EXECUTADO INSTANTANEAMENTE QUANDO DADOS CHEGAM VIA ESP-NOW ---
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (memcmp(recv_info->src_addr, macMasterEsperado, 6) == 0) { // Valida se o transmissor que enviou a mensagem possui o endereço MAC cadastrado
    memcpy(&dadosRecebidos, data, sizeof(dadosRecebidos)); // Copia diretamente a corrente de bytes recebidos do espaço para a nossa variável estruturada
    ultimaMensagemRecebida = millis(); // Salva a marcação atual do relógio interno registrando a chegada bem-sucedida do pacote
    falhaComunicacao = false; // Desativa o indicador de erros sinalizando que o sinal de rádio está ativo e functional
    
    ultimoRssi = recv_info->rx_ctrl->rssi; // Intercepta a intensidade de ganho de sinal rádio calculada em decibéis
    int brilhoPing = map(ultimoRssi, -95, -30, 0, 255); // Traduz a variação de potência de rádio em níveis aceitáveis de luz PWM para o LED
    brilhoPing = constrain(brilhoPing, 0, 255); // Bloqueia o brilho gerado para que ele permaneça obrigatoriamente dentro de 0 a 255
    analogWrite(pinoLedPingAzul, brilhoPing); // Altera o brilho físico do LED azul receptor com base na proximidade do transmissor
  }
}

void setup() {
  Serial.begin(115200); // Inicia o canal de comunicação serial enviando dados ao computador na velocidade de 115200 bits por segundo
  delay(1000); // Pausa o processador por 1 segundo para garantir que a energia da placa se estabilize por completo
  Serial.println("Receptor esp32 s3 V Inicializando..."); // Escreve um aviso no monitor do PC para indicar que a placa ligou
  
  pinMode(pinoBotaoLiga, INPUT_PULLUP); // Ativa o botão liga/desliga com o resistor interno conectado ao polo positivo
  pinMode(pinoBotaoModo, INPUT_PULLUP); // Ativa o botão de mudar de tela com o resistor interno conectado ao polo positivo
  pinMode(pinoBotaoSobe, INPUT_PULLUP); // Ativa o botão de aumentar a escala com o resistor interno conectado ao polo positivo
  pinMode(pinoBotaoDesce, INPUT_PULLUP); // Ativa o botão de diminuir a escala com o resistor interno conectado ao polo positivo
  
  // --- CONFIGURAÇÃO DOS DOIS BUZZERS POR HARDWARE (LEDC NATIVO) ---
  ledcAttach(pinoBuzzer1, 2000, 8); // Ativa o Buzzer 1 (Cliques do teclado) no pino 47 com frequência de 2000Hz e 8 bits de definição
  ledcAttach(pinoBuzzer2, 2000, 8); // Ativa o Buzzer 2 (Alertas inteligentes) no novo pino 46 com frequência de 2000Hz e 8 bits de definição
  ledcWrite(pinoBuzzer1, 0); // Garante que o Buzzer 1 de cliques inicie totalmente silencioso (volume zero)
  ledcWrite(pinoBuzzer2, 0); // Garante que o Buzzer 2 de alertas comece totalmente silencioso (volume zero)
  
  pinMode(pinoBacklight, OUTPUT); // Configura o pino de controle de luz de fundo do display LCD como uma saída de energia
  pinMode(pinoLedPingAzul, OUTPUT); // Configura o pino do LED indicador de sinal por proximidade como uma saída de energia
  
  digitalWrite(pinoBacklight, HIGH); // Envia energia para acender imediatamente os refletores traseiros do visor LCD

  pixelTemp.begin(); // Inicializa os barramentos de controle e as linhas de comunicação do LED NeoPixel de Temperatura
  pixelTemp.setBrightness(60); // Define a potência máxima de luz do LED de temperatura no nível confortável de 60
  pixelHum.begin(); // Inicializa os barramentos de controle e as linhas de comunicação do LED NeoPixel de Umidade
  pixelHum.setBrightness(60); // Define a potência máxima de luz do LED de umidade no nível confortável de 60
  pixelLuz.begin(); // Inicializa os barramentos de controle e as linhas de comunicação do LED NeoPixel de Luminosidade
  pixelLuz.setBrightness(60); // Define a potência máxima de luz do LED de luminosidade no nível confortável de 60

  lcd.begin(16, 2); // Ativa o visor configurando as linhas de comunicação paralelas para o formato de 16 colunas por 2 linhas
  delay(100); // Pausa o circuito por 100 milissegundos para dar tempo de o display paralelo processar as dimensões da tela

  // Carrega as barrinhas padrão e limpa o histórico
  recriarCaracteresGrafico(); // Executa a rotina que grava os 8 desenhos customizados de barras verticais na memória do visor LCD
  for (int i = 0; i < 16; i++) { // Abre um ciclo automático de repetição para percorrer as posições do gráfico 16 vezes
    historicoDados[i] = 0; // Grava o valor zero em cada uma das colunas para limpar quaisquer registros ou barras antigas
  }

  lcd.clear(); // Limpa totalmente a tela do display apagando letras da inicialização anterior
  lcd.print("S3 Conectando..."); // Imprime o texto inicial de busca de sinal na linha superior do display paralelo
  delay(500); // Segura o letreiro visível por meio segundo para dar tempo do usuário ler o status do rádio

  WiFi.mode(WIFI_STA); // Coloca a placa de rede rádio interna do chip ESP32 no modo de estação de trabalho
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Trava o receptor rigidamente no canal 1 para não sofrer interferências de outras redes

  if (esp_now_init() != ESP_OK) { // Inicia o protocolo de rádio ESP-NOW e checa se houve alguma falha crítica de hardware
    Serial.println("Erro ao inicializar ESP-NOW."); // Registra a ocorrência de erro crítico de conectividade na janela serial do PC
    delay(1000); // Segura o processamento por 1 segundo para garantir que o log de erro foi escrito por completo
    ESP.restart(); // Força um reset físico automático no chip para destravar e reiniciar a busca de sinal
  }
  
  esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv); // Conecta o receptor à função de recepção automática para capturar pacotes do ar
  
  ultimaMensagemRecebida = millis(); // Carimba o relógio de contagem salvando o instante inicial para evitar falsos alarmes de queda
  ultimoTempoAtividade = millis(); // Registra o tempo inicial indicando que o painel de controle iniciou ativo
}

void loop() {
  unsigned long tempoAtual = millis(); // Captura o tempo decorrido em milissegundos desde que a placa receptor foi ligada
  
  // --- SISTEMA DE TIMEOUT DA COMUNICAÇÃO (ACIONA O BUZZER 2 EXCLUSIVO DE RETORNO) ---
  if (tempoAtual - ultimaMensagemRecebida > tempoLimiteMensagem) { // Compara se o tempo sem mensagens do Master ultrapassou 7 segundos
    falhaComunicacao = true; // Ativa a variável de estado informando que a comunicação com a adega caiu
    
    pixelTemp.setPixelColor(0, pixelTemp.Color(255, 0, 0)); // Altera a cor do LED de temperatura para vermelho puro em caso de falha de sinal
    pixelHum.setPixelColor(0, pixelHum.Color(255, 0, 0)); // Altera a cor do LED de umidade para vermelho puro em caso de falha de sinal
    pixelLuz.setPixelColor(0, pixelLuz.Color(255, 0, 0)); // Altera a cor do LED de luz para vermelho puro em caso de falha de sinal
    pixelTemp.show(); pixelHum.show(); pixelLuz.show(); // Descarrega o comando físico para os 3 LEDs acenderem em alerta vermelho

    emitirSom(pinoBuzzer2, 1200, 1000); // Dispara o som contínuo de emergência de 1200Hz por 1 segundo no Buzzer 2 isolado de 5V
    lcd.clear(); // Limpa instantaneamente as escritas do visor para trocar de tela
    lcd.print("RECONECTANDO..."); // Escreve o texto de aviso emergencial na primeira linha do display paralelo
    delay(1500); // Segura o processamento por 1,5 segundos para que o usuário perceba o alarme de conexão
    ESP.restart(); // Executa um reset forçado por software na placa para tentar restaurar a conexão de rede
  }

  // --- LEITURA DOS BOTÕES COM REESTRUTURAÇÃO DO SOM EM SEGUNDO PLANO (BUZZER 1) ---
  if (digitalRead(pinoBotaoModo) == LOW) { // Lê o pino físico do botão de modo e verifica se ele foi pressionado ao chão
    emitirSom(pinoBuzzer1, 2000, 50); // Emite um clique rápido e agudo de 2000Hz por 50ms no Buzzer 1 do teclado
    modoExibicao = (modoExibicao == 0) ? 1 : 0; // Inverte o modo de tela ativo (se for texto muda para gráfico, e vice-versa)
    lcd.clear(); // Limpa totalmente o visor para não misturar textos do modo antigo com o modo novo
    delay(250);  // Bloqueia temporariamente por 250 milissegundos para evitar leituras falsas por ricochete mecânico do botão
  }

  if (digitalRead(pinoBotaoLiga) == LOW) { // Checa se o usuário pressionou o botão físico de controle de energia do display
    emitirSom(pinoBuzzer1, 1800, 80); // Emite um clique de confirmação médio de 1800Hz por 80ms no Buzzer 1 ao alternar tela
    painelLigado = !painelLigado; // Altera o estado binário da tela (liga se estava desligado ou desliga se estava ativo)
    
    if (painelLigado) { // Se o comando atual foi para ativar o monitor do painel
      digitalWrite(pinoBacklight, HIGH); // Liga fisicamente a alimentação elétrica dos LEDs de luz de fundo do LCD
      executarAnimacaoAbertura(); // Refaz obrigatoriamente a animação da garrafa quadro a quadro ao ligar a tela
    } else { // Caso o comando atual tenha sido para desativar o monitor do painel
      digitalWrite(pinoBacklight, LOW); // Corta a alimentação elétrica dos LEDs de fundo apagando o visor
      lcd.clear(); // Limpa os caracteres internos da tela para reiniciar limpo no próximo acionamento
    }
    delay(250); // Aguarda um quarto de segundo servindo de filtro mecânico contra ruídos de clique
  }

  if (painelLigado) { // Verifica se a tela está ativa para autorizar as configurações de ajuste do gráfico
    if (digitalRead(pinoBotaoSobe) == LOW) { // Lê o botão de navegação para cima e confere se ele está pressionado
      emitirSom(pinoBuzzer1, 2200, 40); // Emite um som rápido e bem agudo de 2200Hz para incremento de escala no Buzzer 1
      escalaGrafico += 5; // Aumenta o valor de divisão do limite vertical do gráfico em 5 unidades
      delay(150); // Cria um intervalo de estabilização de 150 milissegundos entre as alterações de valor
    }
    if (digitalRead(pinoBotaoDesce) == LOW) { // Lê o botão de navegação para baixo e confere se ele está pressionado
      emitirSom(pinoBuzzer1, 2200, 40); // Emite um som rápido e bem agudo de 2200Hz para decremento de escala no Buzzer 1
      escalaGrafico -= 5; // Reduz o valor de divisão do limite vertical do gráfico em 5 unidades
      if (escalaGrafico < 5) { // Aplica uma trava lógica de segurança para verificar se o valor ficou menor que 5
        escalaGrafico = 5; // Força o valor limite mínimo da escala a se manter travado em 5 para evitar erros matemáticos
      }
      delay(150); // Cria um intervalo de estabilização de 150 milissegundos entre as alterações de valor
    }
  }

  // --- MAPEAMENTO DE CORES E ALERTAS SONOROS INTELIGENTES (BUZZER 2 - 5V) ---
  int ldrPorcentagem = map(dadosRecebidos.mediaLdr, 0, 4095, 0, 100); // Transforma proporcionalmente o valor de luz bruto em percentual
  ldrPorcentagem = constrain(ldrPorcentagem, 0, 100); // Previne que erros de ruído tirem a porcentagem da faixa de 0 a 100%

  // Armazena em variáveis lógicas se algum parâmetro real da Vinharia Agnello saiu do limite correto de conservação
  bool alertaTemperatura = (dadosRecebidos.temperatura < 12.0 || dadosRecebidos.temperatura > 16.0); // Alerta se fora de 12°C a 16°C
  bool alertaUmidade     = (dadosRecebidos.humidade < 60.0 || dadosRecebidos.humidade > 80.0); // Alerta se fora de 60% a 80%
  bool alertaLuminosidade = (ldrPorcentagem < 40 || ldrPorcentagem > 69); // Alerta se a luz sair do intervalo seguro de 40% a 69%

  // Cria um temporizador estático para que o Buzzer 2 de alarmes não apite sem parar e congele os NeoPixels
  static unsigned long ultimoTempoBipAlerta = 0; // Armazena o registro de tempo da última execução de som de desvio
  if (tempoAtual - ultimoTempoBipAlerta > 4000) { // Cria uma janela de verificação que roda a cada 4 segundos exatos
    ultimoTempoBipAlerta = tempoAtual; // Atualiza o cronômetro salvando o instante da checagem atual

    if (alertaTemperatura) { // Caso a temperatura da adega tenha rompido as regras de qualidade
      // FALHA DE TEMPERATURA: Som intermitente rápido (3 bipes curtos e agudos simulando relógio digital)
      for(int i = 0; i < 3; i++) { // Cria um ciclo de repetição para gerar 3 bipes em sequência
        emitirSom(pinoBuzzer2, 1500, 100); // Emite um bipe agudo de 1500Hz com duração de 100 milissegundos
        delay(100); // Dá uma pausa silenciosa de 100 milissegundos entre as repetições do laço
      }
    } 
    else if (alertaUmidade) { // Caso a umidade do ar ameace o ressecamento ou bolor das rolhas Agnello
      // FALHA DE UMIDADE: Som de "claxon" longo (1 bipe grave e bem estendido de atenção)
      emitirSom(pinoBuzzer2, 800, 800); // Injeta uma frequência encorpada de 800Hz com duração longa de 800 milissegundos
    } 
    else if (alertaLuminosidade) { // Caso a claridade do ambiente ameace a estabilidade química dos vinhos finos
      // FALHA DE LUMINOSIDADE: Som de "sirene" alternada (2 bipes médios com variações de tons)
      emitirSom(pinoBuzzer2, 1000, 200); // Toca o primeiro tom da sirene em 1000Hz por 200 milissegundos
      delay(50); // Aplica um breve intervalo de separação de tons de 50 milissegundos
      emitirSom(pinoBuzzer2, 1200, 200); // Toca o segundo tom da sirene em 1200Hz por 200 milissegundos
    }
  }

  // Mapeia os dados recebidos do Master para gerar as transições suaves de cor nos Pixels
  int pctTemp = map((int)dadosRecebidos.temperatura, 12, 16, 0, 100); // Converte o intervalo térmico real Agnello para a escala interna do LED
  int pctHum  = map((int)dadosRecebidos.humidade, 60, 80, 0, 100); // Converte o intervalo de umidade real Agnello para a escala interna do LED
  int pctLuz  = ldrPorcentagem; // Define que a porcentagem de luz calculada dita o comportamento do LED de luminosidade

  // Envia as cores calculadas para cada anel/barra NeoPixel correspondente
  pixelTemp.setPixelColor(0, calcularCorSuave(pixelTemp, pctTemp)); // Calcula a cor do LED térmico e guarda na memória
  pixelHum.setPixelColor(0, calcularCorSuave(pixelHum, pctHum)); // Calcula a cor do LED de umidade e guarda na memória
  pixelLuz.setPixelColor(0, calcularCorSuave(pixelLuz, pctLuz)); // Calcula a cor do LED de luminosidade e guarda na memória
  
  // Atualiza fisicamente os LEDs na placa (Eles continuam operando e respondendo ao ESP-NOW mesmo com a tela apagada)
  pixelTemp.show(); // Atualiza o chip elétrico do LED NeoPixel de Temperatura para brilhar na cor correspondente
  pixelHum.show(); // Atualiza o chip elétrico do LED NeoPixel de Umidade para brilhar na cor correspondente
  pixelLuz.show(); // Atualiza o chip elétrico do LED NeoPixel de Luminosidade para brilhar na cor correspondente

  // --- RENDERIZAÇÃO DOS TEXTOS E GRÁFICOS NO DISPLAY ---
  if (painelLigado && !falhaComunicacao) { // Valida se o visor deve ser atualizado checando se a tela está ativa e a rede operacional
    if (modoExibicao == 0) { // Se o visor estiver configurado para o modo de leitura de texto direto
      // MODO 0: Exibição de texto tradicional limpa (Temperatura, Umidade e Luz da Adega)
      lcd.setCursor(0, 0); // Move o início da escrita para o canto superior esquerdo da primeira linha
      lcd.printf("T:%-4.1fC H:%-3.0f%%", dadosRecebidos.temperatura, dadosRecebidos.humidade); // Exibe formatado os valores de Temperatura e Umidade recebidos via rádio
      lcd.setCursor(0, 1); // Desloca a escrita para o início da segunda linha de caracteres inferior
      lcd.printf("luz adegaria: %-3d%% ", ldrPorcentagem); // Exibe o valor calculado de luminosidade da adega em formato de porcentagem limpa
    } 
    else { // Caso o visor tenha sido alternado para o modo de visualização gráfica
      // MODO 1: Exibição do Gráfico de Histórico da Luminosidade
      static unsigned long ultimoUpdateGrafico = 0; // Declara uma variável estática para controlar a velocidade de redesenho do gráfico
      
      if (tempoAtual - ultimoUpdateGrafico > 500) { // Verifica se já se passaram 500 milissegundos desde a última atualização do gráfico
        ultimoUpdateGrafico = tempoAtual; // Grava o instante atual como a última atualização gráfica processada
        
        // Desloca todas as leituras antigas uma casa para a esquerda (Efeito rolo)
        for (int i = 0; i < 15; i++) { // Cria um contador que caminha do índice 0 até o índice 14 do histórico
          historicoDados[i] = historicoDados[i + 1]; // Move o dado da direita um bloco para a esquerda gerando efeito de esteira
        }
        
        // Mapeia a luminosidade atual para um caractere customizado de barra (0 a 7 de altura)
        int novaBarra = map(ldrPorcentagem, 0, 100, 0, 7); // Transforma os 100% de luz na escala de 8 níveis de desenho das barras do LCD
        historicoDados[15] = constrain(novaBarra, 0, 7); // Adiciona com total segurança a barra mais recente na posição final da esteira
      }
      
      // Renderiza a linha superior fixa com dados de texto limpos
      lcd.setCursor(0, 0); // Posiciona o cursor no início da primeira linha superior do LCD
      lcd.print("LDR:"); // Imprime o rótulo fixo de classificação do sensor de luz
      lcd.print(ldrPorcentagem); // Escreve o valor numérico da porcentagem atual medida
      lcd.print("%  Esc:"); // Escreve o símbolo de por cento e o rótulo identificador de escala manual
      lcd.print(escalaGrafico); // Mostra o valor do multiplicador de escala ajustado pelos botões do usuário
      lcd.print("   "); // Imprime espaços vazios para limpar automaticamente números antigos menores
      
      // Renderiza as 16 barrinhas de histórico customizadas na linha inferior
      lcd.setCursor(0, 1); // Direciona o cursor de escrita para o início da segunda linha inferior
      for (int i = 0; i < 16; i++) { // Cria uma contagem de repetição para cobrir as 16 colunas físicas do visor LCD
        lcd.write((byte)historicoDados[i]); // Imprime o caractere customizado em formato de barra vertical correspondente àquela posição do tempo
      }
    }
  }
  
  // Aguarda um pequeno intervalo estável antes de repetir o loop
  delay(30); // Aplica uma pausa de 30 milissegundos para evitar sobrecarga no núcleo de processamento do ESP32-S3
}
