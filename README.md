# ❖ Sistema Multiversati para Vinherias e Adegas — Stage 2 (ESP32 & IoT)
> **CTRL + 5 — Sempre um passo à frente.**

Este documento detalha o ecossistema avançado do **Stage 2** do Sistema Multiversati, desenvolvido pelo grupo **CTRL + 5** em parceria com a **FIAP**. Deixamos para trás as limitações do monitoramento físico isolado e entramos na **Indústria 4.0**, utilizando o poder de processamento do chip **ESP32** para criar uma rede sem fios resiliente focada na manutenção das condições biológicas perfeitas do vinho.

---

## 🍇 As Condições Ideais do Vinho vs. Ameaças Ambientais
O armazenamento de safras nobres exige estabilidade absoluta. O Stage 2 atua diretamente na prevenção de três grandes inimigos das adegas de luxo:

1. **Luminosidade (Raios UV):** Dispara reações fotoquímicas nos taninos, gerando o defeito do **"gosto de luz"** (*goût de lumière*). O sistema exige escuridão máxima (valores ideais de luminosidade abaixo de 20%).
2. **Temperatura:** Deve ser mantida constante entre **12°C e 16°C**. Oscilações aceleram o envelhecimento precoce do vinho e destroem os compostos aromáticos.
3. **Umidade Relativa:** Deve oscilar de forma controlada entre **65% e 75%**. Baixa umidade resseca a rolha de cortiça, permitindo a entrada de oxigênio e vinagrando o lote; alta umidade gera fungos e danifica os rótulos.

---

## 📡 Engenharia de Conectividade: Arquitetura ESP-NOW (Multi-Placas)
A grande evolução do Stage 2 é a eliminação total de fiação de longo alcance. O sistema passa a operar em uma rede distribuída composta por duas ou mais placas ESP32 se comunicando em tempo real:

### 📡 1. Módulo Sensor (O Mestre na Adega)
Fica posicionado estrategicamente junto aos barris de maturação. Ele realiza as leituras físicas de luz (LDRs redundantes), temperatura e umidade, processa a média aritmética e transmite os dados instantaneamente via rádio.

### 📥 2. Módulo Receptor (A Central de Comando)
Fica na sala de monitoramento ou no balcão da gerência. Recebe os pacotes de dados brutos e comanda os atuadores remotos (Display LCD, alertas visuais LED e alarmes sonoros via Buzzer).

### 🚀 Vantagens Comerciais do Protocolo ESP-NOW (2.4 GHz):
* **Independência de Rede:** A comunicação ocorre via rádio nativo diretamente entre os chips. Não depende de roteadores, sinal de internet ou infraestrutura de Wi-Fi local para proteger a adega.
* **Alcance Industrial:** Garante cobertura de sinal estável em distâncias de **até 400 metros**, ideal para grandes adegas subterrâneas ou galpões de armazenamento.
* **Baixa Latência:** O tempo de resposta entre a detecção de um feixe de luz inadequado no mestre e o disparo do alarme no receptor é inferior a **150ms**.

---

## 🛡️ Resiliência Baseada em Wi-Fi, LoRa e Data Logging
Para investidores e grandes vinícolas, o Sistema Multiversati oferece um plano de contingência triplo para garantir que **nenhuma informação seja perdida**, mesmo em cenários de falha crítica:

* **Integração Wi-Fi da Loja:** Paralelamente ao rádio ESP-NOW, o receptor conecta-se à rede local da empresa para enviar relatórios de telemetria direto para o smartphone ou dashboard do gestor.
* **Módulos LoRa (Long Range):** Preparação de hardware para comunicação em sub-GHz, furando barreiras físicas severas (como paredes de pedra de adegas subterrâneas) onde o Wi-Fi tradicional falha.
* **Data Logging Ativo (Armazenamento Local):** O grande diferencial de segurança. Caso a comunicação sem fios caia por qualquer interferência, o ESP32 Mestre armazena todas as leituras de ambiente em memória não-volátil local. Assim que o sinal é restabelecido, os dados históricos são sincronizados automaticamente, eliminando "pontos cegos" de auditoria.

---

## 🏛️ Instituição e Desenvolvimento
* **Grupo:** CTRL + 5 (Eduardo, Flávia, Gabriel, Lirity e Nicolle)
* **FIAP — 2026**
* Projeto: *Evolução Tecnológica e Escalabilidade IoT Aplicada à Viticultura*
