<p align="center">
  <img src="https://user-images.githubusercontent.com/50468352/141820811-412e9364-7f5c-4889-826a-fcba23b92e23.png" width="350" alt="Logo do Projeto" />
</p>

<h3 align="center">📌 Projeto Integrador em Computação VI - 2026</h3>

<p align="center"><strong>Polo:</strong> DRP04 - Trocar pelo nome dos polos</p>
<p align="center"><strong>Orientadora do PI:</strong> Aline Santana</p>

---

## 👥 Integrantes do grupo

| Nome                                | RA         |
|-------------------------------------|------------|
| Daniel Anunciato                    | 2222677    |
| Eder Clauber dos Santos dos Anjos   | 1806662    |
| Felipe Rafael Henriques             | 2214261    |
| Flavio Jorge de Medeiros            | 23205233   |
| Francisco Ribeiro da Silva Junior   | 2108392    |
| Kelven Joseph Machado Santos        | 2100626    |
| Matheus Eduardo Peixoto de Carvalho | 2205301    |
| Nicolly de Sousa Lima               | 2205907    |

---

## 💡 Projeto: *CondoAcesso — Portaria RFID com integração Azure*

> **Dispositivo IoT para controle de acesso por RFID, armazenamento local e integração com serviços em nuvem.**

---

<details>
<summary>⚠️ <strong>Sobre o desafio encontrado</strong></summary>
<br/>

🔍 O desafio desta etapa foi desenvolver um dispositivo de portaria capaz de identificar TAGs RFID, verificar permissões de acesso e continuar operando mesmo durante falhas temporárias de conexão com a internet.

Para isso, foi necessário integrar o leitor **MFRC522**, três LEDs de sinalização, um buzzer passivo e o cartão microSD embutido à placa **ESP32-WROVER-KIT V4.1**. Também foi necessário conectar o firmware aos serviços do **Microsoft Azure**, mantendo uma cópia local das TAGs autorizadas e uma fila persistente dos eventos ainda não enviados.

A solução precisou conciliar leitura rápida das TAGs, resposta visual e sonora imediata, comunicação HTTPS, sincronização de horário, persistência no microSD e envio assíncrono dos registros sem interromper o funcionamento da portaria.

</details>

---

<details>
<summary>🎯 <strong>Solução implementada</strong></summary>
<br/>

✅ Desenvolvimento de um firmware para a **ESP32-WROVER-KIT** que conecta o dispositivo à rede Wi-Fi e sincroniza o relógio UTC por NTP.

✅ Download da lista de TAGs autorizadas no **Azure Storage**, com armazenamento de uma cópia local no cartão microSD.

✅ Leitura do UID das TAGs pelo **MFRC522** e comparação com a lista de permissões carregada em memória.

✅ Sinalização de acesso autorizado por LED verde durante 10 segundos e de acesso negado por LED vermelho e três alertas sonoros.

✅ Registro dos acessos autorizados e negados em arquivos NDJSON no microSD, preservando os eventos durante indisponibilidades da rede.

✅ Agrupamento dos registros em lotes de até 5 eventos ou 30 segundos para reduzir o número de conexões HTTPS.

✅ Envio assíncrono dos lotes ao **Azure IoT Hub**, sem bloquear o ciclo principal de leitura RFID.

✅ Processamento dos arquivos enviados por uma aplicação no Azure, responsável por inserir os registros no banco de dados MySQL.

Com essa implementação, a portaria fornece uma resposta local imediata e mantém os registros pendentes até que a comunicação com a nuvem seja restabelecida.

</details>

---

<details>
<summary>⚙️ <strong>Estrutura do projeto</strong></summary>
<br/>

O projeto foi estruturado com os seguintes componentes:

- **ESP32-WROVER-KIT V4.1:** executa o firmware e coordena os periféricos;
- **MFRC522:** realiza a leitura das TAGs RFID pelo barramento SPI;
- **LED azul:** indica a conexão com a rede Wi-Fi;
- **LED verde:** sinaliza um acesso autorizado;
- **LED vermelho:** indica o bloqueio da portaria e sinaliza acessos negados;
- **Buzzer passivo:** emite os alertas sonoros;
- **microSD embutido:** armazena a lista de TAGs e os eventos pendentes;
- **Azure Storage:** disponibiliza a lista de TAGs autorizadas e recebe os arquivos de log;
- **Azure IoT Hub:** autentica o dispositivo e coordena o upload dos registros;
- **Azure Container App:** processa os eventos enviados;
- **MySQL:** armazena o histórico de controle de acesso.

Fluxo de funcionamento:

`TAG RFID → MFRC522 → ESP32-WROVER-KIT → Validação local → LEDs/Buzzer → microSD → Azure IoT Hub → Azure Storage → Container App → MySQL`

Este é o layout capturado para simulação Wokwi. Em hardware real o microSD é embutido — o elemento externo do diagrama serve apenas para representar visualmente a mesma pinagem SDMMC (CLK=14, CMD=15, D0=2).

```
                                                     +---------+
                                                     |  RFID   |
                                                     | MFRC522 |
                                                     +----+----+
                                                          |
                       +---(3V3)-----------------------+  |  SPI: SS=5 SCK=18 MISO=19 MOSI=23 RST=21
                       |                                | |
                   +---+---+                        +---+-+---+
                   | ESP32 |------(D26)---[Buz]---(GND)         Buzzer passivo 2 kHz
                   | WROVER|
                   | KIT   |------(D32)---[330Ω]---[LED]---(GND)   LED Wi-Fi (azul)
                   |       |------(D33)---[330Ω]---[LED]---(GND)   LED Verde (liberado)
                   |       |------(D25)---[330Ω]---[LED]---(GND)   LED Vermelho (bloqueio)
                   |       |
                   |       |------(D2)----(D14)---(D15)         (SDMMC 1-bit onboard)
                   +---+---+
                       |
                     (GND)
```

</details>

---

<details>
<summary>🛠️ <strong>Como executar o projeto</strong></summary>
<br/>

✅ **Requisitos:**

- Arduino CLI ou Arduino IDE;
- Core `esp32:esp32@3.3.11`;
- Bibliotecas `MFRC522` 1.4.x e `ArduinoJson` 7.x;
- Placa ESP32-WROVER-KIT e cartão microSD;
- Rede Wi-Fi com acesso à internet;
- Recursos do projeto configurados no Microsoft Azure;
- Credenciais válidas do dispositivo IoT e permissão de leitura do Azure Storage;
- Extensão Wokwi para simulação no Visual Studio Code, quando necessário.

✅ **Configurar o firmware:**

1. Abra o arquivo [`esp32_wrover_wifi.ino`](esp32_wrover_wifi.ino).
2. Informe o SSID e a senha da rede Wi-Fi nos campos de configuração.
3. Configure a URL e a SAS de leitura do arquivo de TAGs no Azure Storage.
4. Informe o hostname, o identificador e a chave do dispositivo cadastrado no Azure IoT Hub.
5. Não publique as credenciais reais em repositórios ou documentos.

✅ **Compilar com Arduino CLI:**

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32wrover esp32_wrover_wifi
```

✅ **Executar na placa física:**

1. Monte o circuito conforme o arquivo [`diagram.json`](diagram.json).
2. Insira um cartão microSD e conecte a ESP32-WROVER-KIT ao computador pela porta UART.
3. Compile e envie o firmware para a porta COM atribuída à placa.
4. Configure o monitor serial para **115200 bps**.
5. Aproxime uma TAG e acompanhe a validação pelos LEDs, pelo buzzer e pelo monitor serial.

✅ **Executar no Wokwi:**

1. Abra a pasta `esp32_wrover_wifi` no Visual Studio Code.
2. Instale a extensão Wokwi.
3. Execute o comando **Wokwi: Start Simulator**.
4. Utilize o cartão RFID virtual para gerar leituras.

> **Observação:** a simulação do cartão microSD e o acesso aos serviços externos podem apresentar limitações no Wokwi. Na placa física, o slot embutido utiliza o periférico SDMMC.

</details>

---

## 🧰 Tecnologias e ferramentas utilizadas

<p>
  <img src="https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32 Badge"/>
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++ Badge"/>
  <img src="https://img.shields.io/badge/Microsoft_Azure-0078D4?style=for-the-badge&logo=microsoftazure&logoColor=white" alt="Microsoft Azure Badge"/>
  <img src="https://img.shields.io/badge/Azure_IoT_Hub-0078D4?style=for-the-badge&logo=microsoftazure&logoColor=white" alt="Azure IoT Hub Badge"/>
  <img src="https://img.shields.io/badge/MySQL-4479A1?style=for-the-badge&logo=mysql&logoColor=white" alt="MySQL Badge"/>
  <img src="https://img.shields.io/badge/ArduinoJson-00979D?style=for-the-badge&logo=arduino&logoColor=white" alt="ArduinoJson Badge"/>
  <img src="https://img.shields.io/badge/Visual_Studio_Code-007ACC?style=for-the-badge&logo=visualstudiocode&logoColor=white" alt="Visual Studio Code Badge"/>
  <img src="https://img.shields.io/badge/GitHub-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub Badge"/>
</p>

---
