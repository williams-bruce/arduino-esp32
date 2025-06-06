# Dispensador de Medicamentos Automatizado com ESP32

Este projeto implementa um dispensador de medicamentos automatizado utilizando um microcontrolador ESP32. O dispositivo pode ser conectado à internet via WiFi para obter a hora exata de um servidor NTP e ser controlado através de uma interface web. Ele permite agendar horários para a dispensação de comprimidos, além de acionar a dispensação manualmente. Um display OLED exibe informações importantes como hora, status da conexão e próximos agendamentos.

## Funcionalidades Principais

*   **Dispensação Automatizada:** Dispensa a quantidade correta de comprimidos em horários pré-agendados.
*   **Controle via Web:** Servidor web embarcado para configuração e controle.
*   **Modo de Ponto de Acesso (AP):** Facilita a configuração inicial da rede WiFi.
*   **Sincronização de Horário (NTP):** Mantém o relógio interno sempre sincronizado com a internet para garantir a precisão dos horários.
*   **Display OLED:** Mostra informações em tempo real como hora atual, status e mensagens.
*   **Sensor de Retirada:** Utiliza um sensor infravermelho para detectar se o medicamento foi retirado e emite lembretes sonoros caso o usuário se esqueça.
*   **Notificações Sonoras:** Um buzzer emite bipes para alertar sobre a dispensação e lembretes.
*   **Persistência de Dados:** Salva as configurações de WiFi e agendamentos na memória não volátil do ESP32.

## Hardware Necessário

*   ESP32 Dev Kit
*   Display OLED SSD1306 128x64 I2C
*   Servo Motor (SG90 ou similar)
*   Sensor Infravermelho (IR) de Obstáculo
*   Buzzer Ativo
*   Protoboard e Jumpers

## Bibliotecas

Para compilar o código, é necessário as seguintes bibliotecas instaladas na Arduino IDE:
*   `Wire`
*   `Adafruit_SSD1306`
*   `Adafruit GFX` (dependência da biblioteca do display)
*   `NTPClient` by Fabrice Weinberg
*   `WiFi`
*   `ESP32Servo`
*   `WebServer` (para ESP32)
*   `ArduinoJson` by Benoit Blanchon
*   `Preferences`

## Configuração e Uso

1.  **Montagem do Hardware:** Conecte todos os componentes ao ESP32 conforme os pinos definidos no início do arquivo `controle_remedio_final_version.ino`.
    *   `SERVO1_PIN`: 18
    *   `BUZZER_PIN`: 23
    *   `IR_SENSOR_PIN`: 25
    *   Display OLED: Pinos I2C (SDA, SCL)
2.  **Instalação das Bibliotecas:** Abra a Arduino IDE, vá em `Sketch > Include Library > Manage Libraries...` e instale as bibliotecas listadas acima.
3.  **Compilação e Upload:** Abra o arquivo `.ino` na Arduino IDE, selecione a placa "ESP32 Dev Module" (ou a correspondente) e a porta COM correta. Clique em "Upload" para gravar o firmware no dispositivo.
4.  **Configuração Inicial (Modo AP):**
    *   Na primeira vez que o dispositivo é ligado (ou se ele não conseguir se conectar a uma rede salva), ele iniciará um Ponto de Acesso WiFi.
    *   Conecte seu celular ou computador à rede WiFi com o SSID: **`MedicineDispenser`** e a senha: **`1234abcd!`**.
    *   Após conectar, o endereço IP do Ponto de Acesso será `192.168.4.1`. Você pode usar este IP para acessar a API e configurar a conexão com a sua rede WiFi doméstica através da rota `/configure_wifi`.
5.  **Uso Normal:**
    *   Uma vez conectado à sua rede WiFi, o display mostrará o endereço IP do dispositivo.
    *   Você pode usar este novo endereço IP para acessar a API, configurar os horários de dispensação ou acionar a dispensação manual.

## Documentação da API REST
O servidor web embarcado fornece uma API REST para interagir com o dispensador. Todas as rotas suportam CORS para permitir requisições de outras origens.

#### `GET /info`
*   **Descrição:** Retorna informações de status do dispositivo.
*   **Resposta (JSON):**
    ```json
    {
      "device_name": "Medicine Dispenser",
      "version": "1.0",
      "configured": true,
      "current_time": "14:30:00",
      "schedule_count": 2,
      "wifi_connected": true,
      "ap_ip": "192.168.4.1",
      "wifi_ip": "192.168.1.10"
    }
    ```

#### `POST /configure_wifi`
*   **Descrição:** Configura a conexão WiFi do dispositivo.
*   **Corpo da Requisição (JSON):**
    ```json
    {
      "ssid": "NomeDaSuaRede",
      "password": "SenhaDaSuaRede",
      "timezone_offset": -3
    }
    ```
*   **Resposta:**
    *   `200 OK`: `{"status":"success","message":"WiFi connected"}`
    *   `400 Bad Request`: `{"status":"error","message":"Failed to connect to WiFi"}`

#### `POST /configure_schedule`
*   **Descrição:** Define a agenda de dispensação de medicamentos. A agenda antiga é substituída.
*   **Corpo da Requisição (JSON):**
    ```json
    {
      "schedule": [
        {"hour": 8, "minute": 0, "pills": 1},
        {"hour": 20, "minute": 0, "pills": 2}
      ]
    }
    ```
*   **Resposta:** `200 OK`: `{"status":"success","message":"Schedule configured"}`

#### `GET /schedule`
*   **Descrição:** Retorna a agenda de dispensação atual.
*   **Resposta (JSON):**
    ```json
    {
      "schedule": [
        {"hour": 8, "minute": 0, "pills": 1, "active": true},
        {"hour": 20, "minute": 0, "pills": 2, "active": true}
      ]
    }
    ```

#### `POST /dispense`
*   **Descrição:** Aciona a dispensação manual de um número específico de comprimidos.
*   **Corpo da Requisição (JSON):**
    ```json
    {
      "pills": 1
    }
    ```
*   **Resposta:** `200 OK`: `{"status":"success","message":"Pills dispensed"}`

É por meio desta API REST que o sistema embarcado é controlado e envia informações para o aplicativo mobile.