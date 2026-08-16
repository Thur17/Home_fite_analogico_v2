# 🏋️ Home Fit Analógico V2

Hub de monitoramento e gerenciamento de treinos baseado em ESP32, desenvolvido para uso em ambiente de academia/residencial, com interface web local, monitoramento cardíaco, velocidade de bicicleta, zonas de esforço, armazenamento de histórico em cartão SD e integração opcional com Discord.

---

## 📖 Sobre o projeto

O **Home Fit Analógico V2** nasceu com a proposta de transformar equipamentos de treino convencionais em equipamentos inteligentes, utilizando um ESP32 como controlador central. 

O sistema permite iniciar uma sessão de treinamento pelo celular, selecionar o atleta e a modalidade, acompanhar os dados em tempo real e armazenar os resultados para consulta posterior. A ideia principal é criar um Hub de Treino independente, sem depender de internet ou de um servidor externo. O ESP32 cria sua própria rede Wi-Fi:

```text
Celular
   │
   │ Wi-Fi
   ▼
┌───────────────────────┐
│       ESP32           │
│   HOME FIT HUB        │
├───────────────────────┤
│ Web Server            │
│ Monitor BPM           │
│ Monitor Velocidade    │
│ Zonas Cardíacas       │
│ RTC DS3231            │
│ MicroSD               │
│ Buzzer                │
└───────────────────────┘
```

O celular acessa diretamente a interface web disponibilizada pelo ESP32.

---

## ✨ Principais funcionalidades

### 🏋️ Gerenciamento de treinos
O sistema permite:
* Selecionar o atleta e adicionar novos atletas
* Selecionar a modalidade e adicionar novas modalidades
* Informar a idade do atleta
* Iniciar e finalizar uma sessão
* Calcular automaticamente as estatísticas
* Salvar os resultados no cartão SD

Modalidades podem ser cadastradas dinamicamente, por exemplo:
🚴 Bike Indoor | 💪 Musculação | 🥊 Luta | ⚡ Funcional | 🏃 Corrida

### ❤️ Monitoramento cardíaco
O projeto utiliza um sensor analógico conectado ao Hand Grip para detectar os pulsos cardíacos. O sinal é processado pelo ESP32 através do ADC.

**Processamento:**
* Leitura analógica e Threshold de detecção
* Filtro temporal (faixa válida de ~46 a 200 BPM)
* Média móvel de 3 leituras
* Detecção de ausência de contato (o BPM é zerado automaticamente após 4 segundos sem pulso)
* Registro de BPM médio e máximo

```text
Sensor Hand Grip ──► ADC ESP32 ──► Threshold ──► Filtro temporal ──► Cálculo BPM ──► Média móvel ──► BPM atual
```

### ❤️‍🔥 Zonas cardíacas
O sistema calcula a frequência cardíaca máxima (FCM) estimada utilizando a fórmula: `FCM = 208 - (0,7 × idade)`. A partir disso, classifica o esforço em cinco zonas e contabiliza o tempo acumulado em cada uma:

| Zona | Percentual da FCM | Classificação |
| :--- | :--- | :--- |
| 🟩 **Z1** | < 60% | Leve |
| 🟦 **Z2** | 60–69% | Gordura |
| 🟨 **Z3** | 70–79% | Aeróbico |
| 🟧 **Z4** | 80–89% | Intenso |
| 🟥 **Z5** | ≥ 90% | Máximo |

> **Importante:** As zonas são estimativas baseadas na fórmula implementada no firmware e não substituem avaliação ou orientação profissional.

### 🚨 Alarme de Zona 5
Quando o atleta permanece em Zona 5 (BPM ≥ 90% FCM), o sistema ativa um alerta sonoro através do buzzer com uma sequência de bipes (sem bloquear o funcionamento principal do ESP32). Quando o BPM retorna para uma zona inferior, o alarme é desligado.

### 🚴 Monitoramento da Bike Indoor
Para a modalidade Bike Indoor, um sensor conectado ao GPIO 14 gera pulsos a cada volta. O ESP32 utiliza uma interrupção para detectar os pulsos e calcular a velocidade. 
Foi implementado *debounce* de ~50 ms para reduzir ruídos. O sistema registra: **Velocidade atual**, **Média** e **Máxima**.

> **Atenção:** O cálculo atual utiliza uma constante específica para a configuração mecânica do projeto. Caso o sensor ou a relação da roda seja alterada, a fórmula deverá ser recalibrada.

### 🕒 RTC DS3231
Mantém data e hora para registrar início/término do treino, identificar logs e registrar dados no cartão SD. Caso o RTC perca a configuração, o firmware utiliza a data/hora de compilação como referência inicial.

### 💾 Armazenamento em MicroSD
Os dados são armazenados localmente em arquivos CSV.
* **Resumo geral** (`/Resumo_Geral.csv`): `Data; Usuario; Atividade; Hora Inicio; Hora Fim; BPM Medio; BPM Maximo; Velocidade Media; Velocidade Maxima`
* **Log detalhado** (`/NomeDoAtleta_log.csv`): Registra `Data/Hora; BPM; Velocidade(km/h)` a cada 30 segundos.

### 📈 Histórico gráfico
A interface web possui uma área de histórico onde o usuário visualiza a evolução dos batimentos. O gráfico utiliza **Chart.js** (carregado via SD).

### 🌐 Interface Web e Wi-Fi
O ESP32 trabalha no modo Access Point (AP) criando a rede:
* **SSID:** `Hub_Treino_Academia`
* **Senha:** `12345678` *(Recomenda-se alterar antes da implantação real)*

A interface HTTP possui áreas para **Treinar** (setup e monitoramento em tempo real) e **Histórico** (consulta de treinos e gráficos).

### 💬 Integração com Discord (Opcional)
Suporte para envio automático dos resultados via Webhook, incluindo dados do atleta, tempos em zonas cardíacas, BPM e velocidades.
> **⚠️ Segurança:** Nunca publique a URL real do seu Webhook no GitHub. Remova ou oculte a variável `SUA_URL_DO_WEBHOOK_DO_DISCORD_AQUI` antes de realizar commits públicos.

---

## 🔌 Hardware e Pinagem

| GPIO | Função | Hardware | Interface |
| :--- | :--- | :--- | :--- |
| **GPIO 5** | CS | Módulo MicroSD | SPI |
| **GPIO 14** | Pulso da roda | Sensor de velocidade | GPIO + Interrupt |
| **GPIO 34** | Entrada analógica | Sensor de BPM / Hand Grip | ADC |
| **GPIO 13** | Saída | Buzzer ativo | GPIO |
| **SDA/SCL** | Comunicação | RTC DS3231 | I²C |

---

## 📚 Bibliotecas e API

**Bibliotecas utilizadas:**
```cpp
#include <WiFi.h>       // Access Point Wi-Fi
#include <WebServer.h>  // Servidor HTTP
#include <FS.h>         // Sistema de arquivos
#include <SD.h>         // Cartão MicroSD
#include <SPI.h>        // Comunicação SPI
#include <Wire.h>       // Comunicação I²C
#include "RTClib.h"     // Controle do DS3231
```

**API HTTP Interna:**
| Endpoint | Método | Função |
| :--- | :--- | :--- |
| `/` | GET | Interface principal |
| `/chart.js` | GET | Biblioteca Chart.js |
| `/listas` | GET | Lista de usuários e atividades |
| `/adicionar` | POST | Adiciona usuário/atividade |
| `/remover` | POST | Remove usuário/atividade |
| `/dados` | GET | Dados em tempo real |
| `/dados_historico`| GET | Histórico de treinos |
| `/dados_minuto` | GET | Dados para gráfico |
| `/iniciar` | POST | Inicia treino |
| `/encerrar` | POST | Finaliza treino |

---

## 🛠️ Instalação e Configuração

**1. Clonar o projeto**
```bash
git clone https://github.com/Thur17/Home_fite_analogico_v2.git
```

**2. Abrir na Arduino IDE**
Abra o arquivo `Home_fite_analogico_v2.ino`.

**3. Instalar bibliotecas**
Instale a `RTClib` e o suporte à placa ESP32 pelo Gerenciador de Placas/Bibliotecas.

**4. Upload**
Selecione `Tools > Board > ESP32 > ESP32 Dev Module`, conecte o ESP32 via USB e faça o upload.

**Configurações no código (`.ino`):**
As principais configurações (SSID, Senha, GPIOs, Threshold, Webhook) ficam no início do código e podem ser customizadas.

---

## 📌 Estado Atual e Roadmap

### ✅ Implementado (V2.0)
- [x] Access Point Wi-Fi e Servidor Web com interface responsiva
- [x] Cadastro de usuários e atividades
- [x] Monitoramento e filtro de BPM (Média móvel)
- [x] Zonas cardíacas e alarme de Zona 5
- [x] Monitoramento da Bike Indoor (Velocidade)
- [x] RTC DS3231 e armazenamento em MicroSD (CSV e Gráficos)
- [x] Integração com Discord e API HTTP interna

### 🚀 Roadmap
**V2.1**
- [ ] Configuração do Wi-Fi e da roda da bicicleta pelo navegador
- [ ] Gerenciamento completo de usuários (exclusão pela interface)
- [ ] Melhor tratamento de caracteres especiais e erros do SD

**V2.2**
- [ ] Dashboard mais completo (Gráficos de velocidade e zonas)
- [ ] Comparação entre treinos e estatísticas semanais/mensais
- [ ] Calorias estimadas

**V3.0**
- [ ] MQTT, Banco de dados e Dashboard remoto
- [ ] Identificação automática do atleta e atualização OTA (Over-The-Air)

---

## 👨‍💻 Autor e Contribuição

Desenvolvido por **Arthur Felippe / Thur17**
* **GitHub:** [https://github.com/Thur17](https://github.com/Thur17)

**Objetivo:** Transformar equipamentos de treinamento tradicionais em equipamentos conectados e inteligentes utilizando tecnologias abertas (ESP32 + Wi-Fi + MicroSD).

⭐ **Se este projeto for útil para você, considere deixar uma estrela no repositório!**