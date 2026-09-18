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

## 💡 Projeto: *CondoAcesso — Leitor RFID com display LCD 1602*

> **Protótipo educacional para leitura de TAGs RFID e exibição do UID em um display LCD 16x2.**

---

<details>
<summary>⚠️ <strong>Sobre o desafio encontrado</strong></summary>
<br/>

🔍 O desafio desta etapa foi validar a leitura de TAGs RFID e fornecer uma resposta visual imediata ao usuário sem depender de conexão Wi-Fi, servidor, nuvem ou armazenamento persistente.

Para isso, foi necessário integrar o leitor **MFRC522**, que se comunica por SPI, e o display **LCD 1602**, controlado em modo paralelo de 4 bits, à placa **ESP32-WROVER-KIT V4.1**. A montagem também exigiu a definição correta dos GPIOs, alimentação em 3,3 V, compartilhamento do GND e ajuste de contraste do LCD por meio de um potenciômetro de 10 kΩ.

</details>

---

<details>
<summary>🎯 <strong>Solução implementada</strong></summary>
<br/>

✅ Desenvolvimento de um sketch para a **ESP32-WROVER-KIT** capaz de inicializar o leitor RFID MFRC522 e o display LCD 1602.

✅ Exibição da mensagem `Aproxime a TAG / no leitor RFID` enquanto o dispositivo aguarda uma leitura.

✅ Ao identificar uma TAG, o sistema apresenta o UID em hexadecimal no monitor serial e no LCD durante 3 segundos.

✅ Após a exibição, o dispositivo retorna automaticamente à tela de espera e fica pronto para uma nova leitura.

✅ Disponibilização de uma simulação no **Wokwi**, permitindo validar o circuito e o comportamento do protótipo antes da montagem física.

Este protótipo não realiza autenticação, consulta a uma lista de permissões, persistência de dados ou comunicação com a nuvem. Seu objetivo é validar a leitura RFID e a interface visual que servirão de base para etapas posteriores do sistema de controle de acesso.

</details>

---

<details>
<summary>⚙️ <strong>Estrutura do projeto</strong></summary>
<br/>

O protótipo foi estruturado com os seguintes componentes:

- **ESP32-WROVER-KIT V4.1:** executa o firmware e coordena os periféricos;
- **MFRC522:** realiza a leitura das TAGs RFID por meio do barramento SPI;
- **LCD 1602 com controlador HD44780:** exibe as mensagens e o UID da TAG em modo paralelo de 4 bits;
- **Potenciômetro de 10 kΩ:** ajusta o contraste do display;
- **Monitor serial USB:** apresenta informações de diagnóstico e o UID lido a 115200 bps.

Fluxo de funcionamento:

`TAG RFID → MFRC522 (SPI) → ESP32-WROVER-KIT → LCD 1602 + Monitor serial`

### Pinos utilizados

| Função | GPIO | Notas |
|---|---|---|
| RFID SS (SDA) | 5 | VSPI CS |
| RFID SCK | 18 | VSPI CLK |
| RFID MISO | 19 | VSPI MISO |
| RFID MOSI | 23 | VSPI MOSI |
| RFID RST | 21 | Reset do MFRC522 |
| LCD RS | 32 | Register Select |
| LCD E | 33 | Enable |
| LCD D4 | 25 | Dado 4 |
| LCD D5 | 26 | Dado 5 |
| LCD D6 | 27 | Dado 6 |
| LCD D7 | 14 | Dado 7 |
| LCD RW | GND | Fixo em escrita — não é ligado ao ESP32 |
| LCD V0 | Wiper do potenciômetro | Ajuste de contraste |
| LCD A (backlight+) | 3.3V | Direto no barramento de alimentação |
| LCD K (backlight-) | GND | Direto no barramento GND |
| Alimentação (3.3V e GND) | trilhos + / − da protoboard | ESP32 → protoboard → RFID + LCD |

O modo paralelo 4-bit envia cada byte em dois nibbles pelos pinos D4–D7. Isso reduz de 8 para 4 os fios de dado, deixando os pinos D0–D3 do LCD sem uso.

### Diagrama de blocos e ligação

```
                     +--------------+
                     |  MFRC522     |
                     |  RFID Reader |
                     +------+-------+
                            |  SPI: SS=5, SCK=18, MISO=19, MOSI=23, RST=21
                            |
          +---(3V3)---------+-------+
          |                         |
   +------+-----+            +------+-------+
   | ESP32      |----(D32)---| RS           |
   | WROVER-KIT |----(D33)---| E            |
   |            |----(D25)---| D4           |
   |            |----(D26)---| D5           |   LCD 1602
   |            |----(D27)---| D6           |   HD44780
   |            |----(D14)---| D7           |   modo 4-bit
   |            |            | RW ---(GND)  |
   |            |            | V0 --[pot]-- (VCC/GND)
   |            |            | A  ---(3V3)  |
   |            |            | K  ---(GND)  |
   +------+-----+            +--------------+
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
- Biblioteca `MFRC522` 1.4.x;
- Biblioteca `LiquidCrystal`;
- Carregue o arquivo esp32_portaria_lcd.ino no dispositivo montado, através do Arduino IDE.

</details>

---

## 🧰 Tecnologias e ferramentas utilizadas

<p>
  <img src="https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32 Badge"/>
  <img src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++ Badge"/>
  <img src="https://img.shields.io/badge/Visual_Studio_Code-007ACC?style=for-the-badge&logo=visualstudiocode&logoColor=white" alt="Visual Studio Code Badge"/>
  <img src="https://img.shields.io/badge/GitHub-181717?style=for-the-badge&logo=github&logoColor=white" alt="GitHub Badge"/>
</p>

---