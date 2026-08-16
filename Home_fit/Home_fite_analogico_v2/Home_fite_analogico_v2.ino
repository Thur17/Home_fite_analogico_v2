#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"

// ---- CONFIGURAÇÕES GERAIS ----
const char* ssid_ap = "Hub_Treino_Academia";
const char* password_ap = "12345678";

WebServer server(80);
RTC_DS3231 rtc;

// Pinos de Hardware
const int pino_CS_SD = 5;
const int pinoVelocidade = 14;      // P2 Roda (Giro)
const int pinoBpmAnalogico = 34;    // P2 Guidão (Hand Grip)
const int pinoBuzzer = 13;          // Buzzer Ativo 5V (Alarme)

// Listas do Sistema
String listaUsuarios = "Egydio,Cristina,Arthur";
String listaAtividades = "Bike Indoor 🚴,Musculacao 💪,Luta 🥊,Funcional ⚡";

// Dados do Treino
String usuarioAtivo = "";
String atividadeAtiva = "";
bool treinoIniciado = false;
int bpmAtual = 0;
float velocidadeAtual = 0.0;
int idadeAtleta = 50; 

// Segurança de Módulos
bool sdDisponivel = false;
bool rtcDisponivel = false;

// Estatísticas e Acumuladores
DateTime horaInicio;
unsigned long ultimoRegistroSD = 0;
unsigned long somaBPM = 0, leiturasBPMCount = 0;
int bpmMaximo = 0;
float somaVelocidade = 0.0; 
unsigned long leiturasVelCount = 0; 
float velocidadeMaxima = 0.0;
unsigned long tempoZonas[5] = {0, 0, 0, 0, 0}; 

// Variáveis Otimizadas para a Velocidade da Bike (ISR)
volatile int voltasBike = 0; 
volatile unsigned long ultimoTempoPulsado = 0;
volatile unsigned long deltaTempoISR = 0;
volatile bool calcularNovaVelocidade = false;

// Variáveis para o Leitor Analógico e Média Móvel (Hand Grip)
unsigned long ultimoPicoCardio = 0;
const int thresholdSinal = 2200; 
int historicoBPM[3] = {0, 0, 0};
int indiceBPM = 0;

// Variáveis para o Alarme do Buzzer
bool tocandoAlarmeZ5 = false;
unsigned long tempoUltimoApito = 0;
int contadorApitos = 0;

// INTERRUPÇÃO DA BIKE (Agora Blindada e Otimizada)
void IRAM_ATTR contarVoltaBike() {
  if (!treinoIniciado || (!atividadeAtiva.startsWith("Bike Indoor"))) return;
  unsigned long tempoAtual = millis();
  
  if (tempoAtual - ultimoTempoPulsado > 50) { // Debounce anti-ruído
    voltasBike++;
    deltaTempoISR = tempoAtual - ultimoTempoPulsado; // Apenas salva o tempo
    ultimoTempoPulsado = tempoAtual;
    calcularNovaVelocidade = true; // Avisa o loop para fazer a matemática
  }
}

// ---- INTERFACE WEB DO USUÁRIO (HTML / CSS / JS) ----
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hub de Treino Pro</title>
    <script src="/chart.js"></script>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 10px; }
        .container { max-width: 600px; margin: 0 auto; }
        .nav-tabs { display: flex; justify-content: space-around; background: #1e293b; padding: 10px; border-radius: 12px; margin-bottom: 15px; }
        .tab-btn { background: none; border: none; color: #94a3b8; font-weight: bold; font-size: 1rem; cursor: pointer; padding: 5px 10px; }
        .tab-btn.active { color: #38bdf8; border-bottom: 2px solid #38bdf8; }
        .tab-content { display: none; }
        .tab-content.active { display: block; }
        .card { background: #1e293b; padding: 20px; border-radius: 15px; margin-bottom: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.3); border: 1px solid #334155; }
        label { display: block; margin-bottom: 5px; color: #94a3b8; font-weight: bold; }
        select, input { width: 100%; padding: 10px; border-radius: 8px; background: #0f172a; color: #fff; border: 1px solid #475569; margin-bottom: 15px; font-size: 1rem; box-sizing: border-box; }
        .inline-group { display: flex; gap: 10px; margin-bottom: 15px; }
        .btn { padding: 10px 15px; border-radius: 8px; border: none; font-weight: bold; cursor: pointer; }
        .btn-start { width: 100%; padding: 12px; font-size: 1.1rem; font-weight: bold; color: #fff; border-radius: 8px; border: none; cursor: pointer; }
        .grid-status { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        .status-box { background: #0f172a; padding: 15px; border-radius: 10px; text-align: center; transition: 0.3s; }
        .status-val { font-size: 2.8rem; font-weight: 800; }
        
        .zona-1 { border-left: 8px solid #10b981; } 
        .zona-2 { border-left: 8px solid #3b82f6; } 
        .zona-3 { border-left: 8px solid #eab308; } 
        .zona-4 { border-left: 8px solid #f97316; } 
        .zona-5 { border-left: 8px solid #ef4444; } 
        
        .history-item { padding: 12px; background: #0f172a; border-radius: 8px; margin-bottom: 8px; cursor: pointer; display: flex; justify-content: space-between; }
    </style>
</head>
<body>
    <div class="container">
        <div class="nav-tabs">
            <button class="tab-btn active" onclick="switchTab('aba-treino')">⚡ Treinar</button>
            <button class="tab-btn" onclick="switchTab('aba-historico')">📊 Histórico</button>
        </div>

        <div id="aba-treino" class="tab-content active">
            <div class="card">
                <h3>Configurar Sessão</h3>
                <label>Atleta 👤</label>
                <div class="inline-group">
                    <select id="selectUsuario"></select>
                    <input type="text" id="novoUsuario" placeholder="Nome..." style="display:none;">
                    <button class="btn" style="background:#38bdf8" id="btnNovoUser" onclick="toggleField('Usuario')">+</button>
                </div>
                <label>Idade do Atleta (para Zonas de Esforço)</label>
                <input type="number" id="idadeAtleta" value="50">
                
                <label>Modalidade 🏋️</label>
                <div class="inline-group">
                    <select id="selectAtividade"></select>
                    <input type="text" id="novaAtividade" placeholder="Ex: Corrida 🏃..." style="display:none;">
                    <button class="btn" style="background:#38bdf8" id="btnNovaAtiv" onclick="toggleField('Atividade')">+</button>
                </div>
                <button class="btn-start" id="btnAcao" onclick="controlarTreino()">Iniciar Sessão</button>
            </div>

            <div class="card">
                <h3>Monitoramento Ativo</h3>
                <p>Atleta: <b id="lblUser" style="color:#38bdf8">Nenhum</b> | Modo: <b id="lblAtiv" style="color:#38bdf8">Nenhum</b></p>
                <div class="grid-status">
                    <div class="status-box" id="boxBPM">
                        <div class="status-val" id="valBPM" style="color:#ef4444">--</div>
                        <div id="lblZonaText" style="color:#94a3b8; font-size:0.8rem; font-weight:bold;">BPM</div>
                    </div>
                    <div class="status-box" id="boxVelocidade">
                        <div class="status-val" id="valVel" style="color:#eab308">0.0</div>
                        <div style="color:#94a3b8">km/h</div>
                    </div>
                </div>
            </div>
        </div>

        <div id="aba-historico" class="tab-content">
            <div class="card">
                <h3>Resumos Salvos no SD</h3>
                <div id="listaHistorico">Carregando treinos...</div>
            </div>
            <div class="card" id="cardGrafico" style="display:none;">
                <h3 id="tituloGrafico">Gráfico do Treino</h3>
                <canvas id="meuGrafico"></canvas>
            </div>
        </div>
    </div>

    <script>
        var emTreino = false;
        var chartInstance = null;

        window.addEventListener('load', () => {
            atualizarListas();
            setInterval(atualizarDadosTempoReal, 1000);
        });

        function switchTab(tabId) {
            document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.getElementById(tabId).classList.add('active');
            event.currentTarget.classList.add('active');
            if(tabId === 'aba-historico') carregarHistoricoSD();
        }

        function atualizarDadosTempoReal() {
            fetch('/dados').then(r => r.json()).then(data => {
                document.getElementById('valBPM').innerText = data.bpm > 0 ? data.bpm : "--";
                document.getElementById('valVel').innerText = data.velocidade ? data.velocidade : "0.0";
                document.getElementById('lblUser').innerText = data.usuario;
                document.getElementById('lblAtiv').innerText = data.atividade;
                document.getElementById('boxVelocidade').style.opacity = (data.atividade.includes("Bike Indoor")) ? "1" : "0.3";

                var boxBPM = document.getElementById('boxBPM');
                boxBPM.className = "status-box " + data.zona_classe;
                document.getElementById('lblZonaText').innerText = data.zona_nome + (data.bpm > 0 ? " (" + data.bpm + " BPM)" : "");

                emTreino = data.treinando;
                var btn = document.getElementById('btnAcao');
                btn.innerText = emTreino ? "Finalizar e Salvar" : "Iniciar Sessão";
                btn.style.background = emTreino ? "#ef4444" : "#22c55e";
            });
        }

        function carregarHistoricoSD() {
            fetch('/dados_historico').then(r => r.text()).then(csvText => {
                var linhas = csvText.split('\n');
                var html = "";
                for(var i = 1; i < linhas.length; i++) {
                    if(!linhas[i].trim()) continue;
                    var col = linhas[i].split(';');
                    html += `<div class="history-item" onclick="gerarGraficoTreino('${col[1]}')">
                        <div><b>${col[0]}</b> - ${col[1]} (${col[2]})</div>
                        <div style="color:#38bdf8">Med BPM: ${col[5]} ➔</div>
                    </div>`;
                }
                document.getElementById('listaHistorico').innerHTML = html || "Nenhum treino registrado no momento.";
            });
        }

        function gerarGraficoTreino(usuario) {
            document.getElementById('cardGrafico').style.display = "block";
            document.getElementById('tituloGrafico').innerText = "Evolução do Cardio: " + usuario;

            fetch(`/dados_minuto?user=${usuario}`).then(r => r.json()).then(dadosLog => {
                var ctx = document.getElementById('meuGrafico').getContext('2d');
                if(chartInstance) chartInstance.destroy(); 

                chartInstance = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: dadosLog.tempos,
                        datasets: [{
                            label: 'Batimentos (BPM)',
                            data: dadosLog.bpms,
                            borderColor: '#ef4444',
                            backgroundColor: 'rgba(239, 68, 68, 0.1)',
                            borderWidth: 2,
                            fill: true
                        }]
                    },
                    options: { responsive: true, scales: { y: { min: 40, max: 200 } } }
                });
            });
        }

        function atualizarListas() { fetch('/listas').then(r => r.json()).then(data => { popularSelect('selectUsuario', data.usuarios); popularSelect('selectAtividade', data.atividades); }); }
        
        function popularSelect(id, array) {
            var select = document.getElementById(id); select.innerHTML = "";
            array.forEach(item => { var opt = document.createElement('option'); opt.value = item; opt.innerText = item; select.appendChild(opt); });
        }
        
        function toggleField(tipo) {
            var sel = document.getElementById('select' + tipo); var inp = document.getElementById('novo' + tipo); var btn = document.getElementById('btnNovo' + tipo);
            if(inp.style.display === 'none') { sel.style.display = 'none'; inp.style.display = 'block'; btn.innerText = '💾'; } 
            else {
                if(inp.value.trim() !== "") {
                    fetch('/adicionar', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ tipo: tipo.toLowerCase(), valor: inp.value.trim() }) }).then(() => { atualizarListas(); });
                }
                sel.style.display = 'block'; inp.style.display = 'none'; inp.value = ""; btn.innerText = '+';
            }
        }

        // --- INTEGRAÇÃO DISCORD VIA CELULAR ---
        function controlarTreino() {
            if(!emTreino) {
                fetch('/iniciar', { 
                    method: 'POST', 
                    headers: {'Content-Type': 'application/json'}, 
                    body: JSON.stringify({ 
                        usuario: document.getElementById('selectUsuario').value, 
                        activity: document.getElementById('selectAtividade').value, 
                        idade: document.getElementById('idadeAtleta').value 
                    }) 
                });
            } else {
                fetch('/encerrar', { method: 'POST' })
                .then(response => response.json())
                .then(dadosFinais => {
                    
                    // COLOQUE O SEU LINK AQUI
                    var urlWebhook = "SUA_URL_DO_WEBHOOK_DO_DISCORD_AQUI"; 
                    
                    var mensagemDiscord = {
                        "embeds": [{
                            "title": "🏋️‍♂️ Novo Treino Concluido na Academia!",
                            "color": 3447003,
                            "fields": [
                                { "name": "👤 Atleta", "value": dadosFinais.usuario, "inline": true },
                                { "name": "💪 Modalidade", "value": dadosFinais.atividade, "inline": true },
                                { "name": "⏱️ Duracao", "value": "Das " + dadosFinais.inicio + " as " + dadosFinais.fim, "inline": false },
                                { "name": "❤️ Batimento Medio", "value": dadosFinais.bpm_media + " BPM", "inline": true },
                                { "name": "🔥 Batimento Maximo", "value": dadosFinais.bpm_max + " BPM", "inline": true },
                                { 
                                    "name": "📊 Tempo por Zona Cardiaca", 
                                    "value": 
                                        "🟩 **Z1 (Leve):** " + dadosFinais.z1 + " min\n" +
                                        "🟦 **Z2 (Gordura):** " + dadosFinais.z2 + " min\n" +
                                        "🟨 **Z3 (Aerobico):** " + dadosFinais.z3 + " min\n" +
                                        "🟧 **Z4 (Intenso):** " + dadosFinais.z4 + " min\n" +
                                        "🟥 **Z5 (Maximo):** " + dadosFinais.z5 + " min", 
                                    "inline": false 
                                }
                            ],
                            "footer": { "text": "Hub de Treino Home Fit" }
                        }]
                    };

                    if (dadosFinais.atividade.includes("Bike Indoor")) {
                        mensagemDiscord.embeds[0].fields.splice(5, 0, 
                            { "name": "🚴 Velocidade Media", "value": dadosFinais.vel_media + " km/h", "inline": true },
                            { "name": "⚡ Velocidade Maxima", "value": dadosFinais.vel_max + " km/h", "inline": true }
                        );
                    }

                    fetch(urlWebhook, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(mensagemDiscord) });
                });
            }
        }
    </script>
</body>
</html>
)rawliteral";

// ---- FUNÇÕES DO SERVIDOR (BACK-END) ----

void handleRoot() { server.send(200, "text/html", index_html); }

void verificarCabecalhosCSV() {
  if (sdDisponivel && !SD.exists("/Resumo_Geral.csv")) {
    File f = SD.open("/Resumo_Geral.csv", FILE_WRITE);
    if (f) { f.println("Data;Usuario;Atividade;Hora Inicio;Hora Fim;BPM Medio;BPM Maximo;Velocidade Media;Velocidade Maxima"); f.close(); }
  }
}

void handleDados() {
  int fcm = 208 - (0.7 * idadeAtleta); 
  String zonaNome = "Aguardando Sensor...";
  String zonaClasse = "zona-1";

  if (treinoIniciado && bpmAtual > 40) {
    float pct = ((float)bpmAtual / fcm) * 100.0;
    if (pct >= 90) { zonaNome = "Zona 5 (Maximo)"; zonaClasse = "zona-5"; }
    else if (pct >= 80) { zonaNome = "Zona 4 (Intenso)"; zonaClasse = "zona-4"; }
    else if (pct >= 70) { zonaNome = "Zona 3 (Aerobico)"; zonaClasse = "zona-3"; }
    else if (pct >= 60) { zonaNome = "Zona 2 (Gordura)"; zonaClasse = "zona-2"; }
    else { zonaNome = "Zona 1 (Leve)"; zonaClasse = "zona-1"; }
  }

  String strVel = (atividadeAtiva.startsWith("Bike Indoor")) ? String(velocidadeAtual, 1) : "";
  String json = "{\"bpm\":" + String(bpmAtual) + ",\"velocidade\":\"" + strVel + "\",\"usuario\":\"" + usuarioAtivo + "\",\"atividade\":\"" + atividadeAtiva + "\",\"treinando\":" + String(treinoIniciado ? "true" : "false") + ",\"zona_nome\":\"" + zonaNome + "\",\"zona_classe\":\"" + zonaClasse + "\"}";
  server.send(200, "application/json", json);
}

void handleHistorico() {
  if (sdDisponivel && SD.exists("/Resumo_Geral.csv")) {
    File f = SD.open("/Resumo_Geral.csv", FILE_READ);
    server.streamFile(f, "text/csv");
    f.close();
  } else {
    server.send(200, "text/plain", "Data;Usuario;Atividade;Hora Inicio;Hora Fim;BPM Medio;BPM Maximo;Velocidade Media;Velocidade Maxima\n24/06/2026;Egydio;Bike Indoor 🚴;08:15:00;08:45:00;128.4;152;22.4;31.8\n");
  }
}

void handleDadosMinuto() {
  String user = server.arg("user");
  String nomeArq = "/" + user + "_log.csv";
  
  String json = "{\"tempos\":[],\"bpms\":[]}";
  if (sdDisponivel && SD.exists(nomeArq)) {
    File f = SD.open(nomeArq, FILE_READ);
    
    String temposArr = ""; 
    String bpmsArr = "";
    
    // Otimização de Memória: Protege a RAM do ESP32 travamentos em treinos longos
    temposArr.reserve(2000); 
    bpmsArr.reserve(2000);
    
    bool primeiraLinha = true;

    while (f.available()) {
      String linha = f.readStringUntil('\n');
      if (primeiraLinha) { primeiraLinha = false; continue; } 
      linha.trim(); 
      if (linha.isEmpty()) continue; 

      int primPontoVirgula = linha.indexOf(';');
      int segPontoVirgula = linha.indexOf(';', primPontoVirgula + 1); 
      
      String dataHora = linha.substring(0, primPontoVirgula);
      String horaApenas = dataHora.substring(dataHora.indexOf(' ') + 1); 
      String bpmValor = linha.substring(primPontoVirgula + 1, segPontoVirgula);

      temposArr += "\"" + horaApenas + "\",";
      bpmsArr += bpmValor + ",";
    }
    f.close();

    if (temposArr.length() > 0) temposArr.remove(temposArr.length() - 1);
    if (bpmsArr.length() > 0) bpmsArr.remove(bpmsArr.length() - 1);

    json = "{\"tempos\":[" + temposArr + "],\"bpms\":[" + bpmsArr + "]}";
  } else {
    json = "{\"tempos\":[\"08:15\",\"08:20\",\"08:25\",\"08:30\",\"08:35\"],\"bpms\":[115,128,145,132,122]}";
  }
  server.send(200, "application/json", json);
}

void handleIniciar() {
  if (server.hasArg("plain")) {
    String corpo = server.arg("plain");
    int inUser = corpo.indexOf("\"usuario\":\"") + 11; usuarioAtivo = corpo.substring(inUser, corpo.indexOf("\"", inUser));
    int inAtiv = corpo.indexOf("\"activity\":\"") + 12; 
    if (inAtiv < 12) inAtiv = corpo.indexOf("\"atividade\":\"") + 13;
    atividadeAtiva = corpo.substring(inAtiv, corpo.indexOf("\"", inAtiv));
    
    int inIdade = corpo.indexOf("\"idade\":\"") + 9; 
    if(inIdade > 9) idadeAtleta = corpo.substring(inIdade, corpo.indexOf("\"", inIdade)).toInt();
    
    for(int i=0; i<5; i++) tempoZonas[i] = 0;
    voltasBike = 0; velocidadeAtual = 0.0; bpmAtual = 0;
    somaBPM = 0; leiturasBPMCount = 0; bpmMaximo = 0;
    somaVelocidade = 0.0; leiturasVelCount = 0; velocidadeMaxima = 0.0;
    ultimoPicoCardio = millis();
    
    // Zera os filtros analógicos
    historicoBPM[0] = 0; historicoBPM[1] = 0; historicoBPM[2] = 0;
    tocandoAlarmeZ5 = false;
    
    if (rtcDisponivel) horaInicio = rtc.now();
    treinoIniciado = true;
    ultimoRegistroSD = millis();

    if (sdDisponivel) {
      File f = SD.open("/" + usuarioAtivo + "_log.csv", FILE_WRITE);
      if(f) { f.println("Data/Hora;BPM;Velocidade(km/h)"); f.close(); }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleEncerrar() {
  if (!treinoIniciado) return;
  treinoIniciado = false;
  digitalWrite(pinoBuzzer, LOW); // Garante que o alarme desligue

  float bpmMedio = (leiturasBPMCount > 0) ? (float)somaBPM / leiturasBPMCount : 0.0;
  float velMedia = (leiturasVelCount > 0) ? somaVelocidade / leiturasVelCount : 0.0;

  String strData = "25/06/2026"; String strInicio = "08:15:00"; String strFim = "08:45:00";

  if (rtcDisponivel) {
    DateTime horaFim = rtc.now();
    char dataStr[12]; snprintf(dataStr, sizeof(dataStr), "%02d/%02d/%04d", horaInicio.day(), horaInicio.month(), horaInicio.year());
    char hInicioStr[9], hFimStr[9];
    snprintf(hInicioStr, sizeof(hInicioStr), "%02d:%02d:%02d", horaInicio.hour(), horaInicio.minute(), horaInicio.second());
    snprintf(hFimStr, sizeof(hFimStr), "%02d:%02d:%02d", horaFim.hour(), horaFim.minute(), horaFim.second());
    strData = String(dataStr); strInicio = String(hInicioStr); strFim = String(hFimStr);
  }

  if (sdDisponivel) {
    File arquivoResumos = SD.open("/Resumo_Geral.csv", FILE_APPEND);
    if (arquivoResumos) {
      arquivoResumos.print(strData + ";"); arquivoResumos.print(usuarioAtivo + ";"); arquivoResumos.print(atividadeAtiva + ";");
      arquivoResumos.print(strInicio + ";"); arquivoResumos.print(strFim + ";");
      arquivoResumos.print(String(bpmMedio, 1) + ";"); arquivoResumos.print(String(bpmMaximo) + ";");
      if (atividadeAtiva.startsWith("Bike Indoor")) { arquivoResumos.print(String(velMedia, 1) + ";"); arquivoResumos.println(String(velocidadeMaxima, 1)); } 
      else { arquivoResumos.println(";"); }
      arquivoResumos.close();
    }
  }

  float z1Min = tempoZonas[0] / 60.0; float z2Min = tempoZonas[1] / 60.0;
  float z3Min = tempoZonas[2] / 60.0; float z4Min = tempoZonas[3] / 60.0;
  float z5Min = tempoZonas[4] / 60.0;

  String jsonResposta = "{";
  jsonResposta += "\"usuario\":\"" + usuarioAtivo + "\",";
  jsonResposta += "\"atividade\":\"" + atividadeAtiva + "\",";
  jsonResposta += "\"inicio\":\"" + strInicio + "\",";
  jsonResposta += "\"fim\":\"" + strFim + "\",";
  jsonResposta += "\"bpm_media\":\"" + String(bpmMedio, 1) + "\",";
  jsonResposta += "\"bpm_max\":\"" + String(bpmMaximo) + "\",";
  jsonResposta += "\"vel_media\":\"" + String(velMedia, 1) + "\",";
  jsonResposta += "\"vel_max\":\"" + String(velocidadeMaxima, 1) + "\",";
  jsonResposta += "\"z1\":\"" + String(z1Min, 1) + "\",";
  jsonResposta += "\"z2\":\"" + String(z2Min, 1) + "\",";
  jsonResposta += "\"z3\":\"" + String(z3Min, 1) + "\",";
  jsonResposta += "\"z4\":\"" + String(z4Min, 1) + "\",";
  jsonResposta += "\"z5\":\"" + String(z5Min, 1) + "\"";
  jsonResposta += "}";

  usuarioAtivo = "Nenhum"; atividadeAtiva = "Nenhuma";
  server.send(200, "application/json", jsonResposta);
}

void handleServeChartJS() {
  if (sdDisponivel && SD.exists("/chart.js")) {
    File f = SD.open("/chart.js", FILE_READ); server.streamFile(f, "application/javascript"); f.close();
  } else { server.send(200, "application/javascript", "/* Chart offline */"); }
}

void handleListas() {
  String json = "{\"usuarios\":[";
  String tempUsers = listaUsuarios;
  while(tempUsers.indexOf(",") != -1) { int idx = tempUsers.indexOf(","); json += "\"" + tempUsers.substring(0, idx) + "\","; tempUsers = tempUsers.substring(idx+1); }
  json += "\"" + tempUsers + "\"],\"atividades\":[";
  String tempAtiv = listaAtividades;
  while(tempAtiv.indexOf(",") != -1) { int idx = tempAtiv.indexOf(","); json += "\"" + tempAtiv.substring(0, idx) + "\","; tempAtiv = tempAtiv.substring(idx+1); }
  json += "\"" + tempAtiv + "\"]}";
  server.send(200, "application/json", json);
}

void handleAdicionar() {
  if (server.hasArg("plain")) {
    String corpo = server.arg("plain");
    int inVal = corpo.indexOf("\"valor\":\"") + 9; String val = corpo.substring(inVal, corpo.indexOf("\"", inVal));
    if(corpo.indexOf("\"tipo\":\"usuario\"") != -1) listaUsuarios += "," + val;
    else if(corpo.indexOf("\"tipo\":\"atividade\"") != -1) listaAtividades += "," + val;
  }
  server.send(200, "text/plain", "OK");
}

void handleRemover() {
  if (server.hasArg("plain")) {
    String corpo = server.arg("plain");
    int inVal = corpo.indexOf("\"valor\":\"") + 9; String val = corpo.substring(inVal, corpo.indexOf("\"", inVal));
    if(corpo.indexOf("\"tipo\":\"usuario\"") != -1) { listaUsuarios.replace(","+val, ""); listaUsuarios.replace(val+",", ""); listaUsuarios.replace(val, ""); }
    else { listaAtividades.replace(","+val, ""); listaAtividades.replace(val+",", ""); listaAtividades.replace(val, ""); }
  }
  server.send(200, "text/plain", "OK");
}

// ---- SETUP E CONFIGURAÇÃO INICIAL ----

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  pinMode(pinoBuzzer, OUTPUT);
  digitalWrite(pinoBuzzer, LOW);
  
  pinMode(pinoVelocidade, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinoVelocidade), contarVoltaBike, FALLING);

  // Prepara o ADC para ler o sensor analógico de 0 a 3.3V
  analogSetAttenuation(ADC_11db);

  if (!rtc.begin()) rtcDisponivel = false;
  else { rtcDisponivel = true; if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); }
  
  if(!SD.begin(pino_CS_SD)) sdDisponivel = false;
  else { sdDisponivel = true; verificarCabecalhosCSV(); }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid_ap, password_ap, 11, 0, 2);
  delay(2000);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/chart.js", HTTP_GET, handleServeChartJS);
  server.on("/listas", HTTP_GET, handleListas);
  server.on("/adicionar", HTTP_POST, handleAdicionar);
  server.on("/remover", HTTP_POST, handleRemover);
  server.on("/dados_historico", HTTP_GET, handleHistorico);
  server.on("/dados_minuto", HTTP_GET, handleDadosMinuto);
  server.on("/dados", HTTP_GET, handleDados);
  server.on("/iniciar", HTTP_POST, handleIniciar);
  server.on("/encerrar", HTTP_POST, handleEncerrar);
  
  server.begin();
  Serial.println("Sucesso: Hub de Treino Ativo!");
}

// ---- LOOP PRINCIPAL ----

void loop() {
  server.handleClient();
  static unsigned long ultimoSegundoZonas = 0;
  static unsigned long ultimaLeituraAnalogica = 0;

  if (treinoIniciado) {
    
    // --- 1. OTIMIZAÇÃO DA BIKE (Matemática fora da Interrupção) ---
    if (calcularNovaVelocidade && deltaTempoISR > 0) {
      velocidadeAtual = (3600.0 / deltaTempoISR) * 2.0; 
      calcularNovaVelocidade = false;
    }
    
    // --- 2. LEITURA DO HAND GRIP COM FILTRO DE MÉDIA MÓVEL ---
    if (millis() - ultimaLeituraAnalogica > 10) { 
      ultimaLeituraAnalogica = millis();
      int valorSinal = analogRead(pinoBpmAnalogico);
      
      if (valorSinal > thresholdSinal) {
        unsigned long tempoAtual = millis();
        unsigned long intervalo = tempoAtual - ultimoPicoCardio;
        
        // Filtro Biológico: só aceita pulsos entre 46 e 200 BPM
        if (intervalo > 300 && intervalo < 1300) {
          int bpmCru = 60000 / intervalo; 
          
          // Alimenta o Array da Média Móvel
          historicoBPM[indiceBPM] = bpmCru;
          indiceBPM = (indiceBPM + 1) % 3;
          
          if (historicoBPM[2] != 0) {
            bpmAtual = (historicoBPM[0] + historicoBPM[1] + historicoBPM[2]) / 3;
          } else {
            bpmAtual = bpmCru; // Até preencher os 3, usa o direto
          }
          
          somaBPM += bpmAtual; leiturasBPMCount++;
          if (bpmAtual > bpmMaximo) bpmMaximo = bpmAtual;
          ultimoPicoCardio = tempoAtual;
        }
      }
    }

    // Limpa a medição se tirar a mão
    if (millis() - ultimoPicoCardio > 4000) {
      bpmAtual = 0;
      historicoBPM[0] = 0; historicoBPM[1] = 0; historicoBPM[2] = 0;
    }

    // --- 3. ALARME DE SEGURANÇA E CONTAGEM DE ZONAS ---
    if (millis() - ultimoSegundoZonas >= 1000) {
      ultimoSegundoZonas = millis();
      int fcm = 208 - (0.7 * idadeAtleta);
      if (bpmAtual > 40) {
        float pct = ((float)bpmAtual / fcm) * 100.0;
        
        if (pct >= 90) { // ZONA 5: Risco Máximo
          tempoZonas[4]++;
          if (!tocandoAlarmeZ5) { tocandoAlarmeZ5 = true; contadorApitos = 0; }
        } else {
          tocandoAlarmeZ5 = false; // Coração baixou, desliga o alarme
          if (pct >= 80) tempoZonas[3]++;
          else if (pct >= 70) tempoZonas[2]++;
          else if (pct >= 60) tempoZonas[1]++;
          else tempoZonas[0]++;
        }
      }
    }

    // Rotina autônoma para tocar os 3 bipes do Buzzer (Sem travar o código)
    if (tocandoAlarmeZ5 && contadorApitos < 6) { 
      if (millis() - tempoUltimoApito > 200) { // Alterna a cada 200ms
        tempoUltimoApito = millis();
        digitalWrite(pinoBuzzer, !digitalRead(pinoBuzzer)); 
        contadorApitos++;
      }
    } else if (!tocandoAlarmeZ5) {
      digitalWrite(pinoBuzzer, LOW);
    }

    // --- 4. PARADA DA BIKE E GRAVAÇÃO ---
    if (atividadeAtiva.startsWith("Bike Indoor")) {
      somaVelocidade += velocidadeAtual; leiturasVelCount++;
      if (velocidadeAtual > velocidadeMaxima) velocidadeMaxima = velocidadeAtual;
      if (millis() - ultimoTempoPulsado > 3000) velocidadeAtual = 0.0;
    }

    if (sdDisponivel && (millis() - ultimoRegistroSD >= 30000)) { 
      ultimoRegistroSD = millis();
      DateTime agora = rtc.now();
      char buf[20]; snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d:%02d", agora.day(), agora.month(), agora.hour(), agora.minute(), agora.second());
      File logFile = SD.open("/" + usuarioAtivo + "_log.csv", FILE_APPEND);
      if (logFile) { 
        logFile.print(String(buf) + ";"); 
        logFile.print(String(bpmAtual) + ";"); 
        logFile.println((atividadeAtiva.startsWith("Bike Indoor")) ? String(velocidadeAtual, 1) : ""); 
        logFile.close(); 
      }
    }
  } else { 
    bpmAtual = 0; 
    velocidadeAtual = 0.0; 
    digitalWrite(pinoBuzzer, LOW);
  }
}