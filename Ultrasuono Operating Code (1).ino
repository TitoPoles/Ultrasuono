#include "DaisyDuino.h"
#include <U8g2lib.h>
#include <Adafruit_AHTX0.h>
#include "daisysp.h"

// Definizione delle variabili globali
DaisyHardware hw;
daisysp::ReverbSc reverb;
daisysp::Svf filter;
daisysp::Overdrive overdrive;
daisysp::Flanger flanger;
Adafruit_AHTX0 aht;

// Configurazione OLED
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R1, /* reset=/U8X8_PIN_NONE, / scl=/12, / sda=*/13);

// Stato del LED e del pulsante
bool ledState = false;       // Stato attuale del LED
bool lastButtonState = false; // Stato precedente del pulsante
const int buttonPin = A2;     // Pin 24 configurato come GPIO
const int ledPin = A4;        // Pin 26 configurato come GPIO

// Pin per il controllo del filtro cutoff
const int lightSensorPin = A1;

// Icone (termometro, umidità, luminosità e logo)
const uint8_t temp_icon[] = { 0x38,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x44,0x00,0x82,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x82,0x00,0x7c,0x00 };
const uint8_t humid_icon[] = {  0x20,0x00,0x20,0x00,0x30,0x00,0x50,0x00,0x48,0x00,0x88,0x00,0x04,0x01,0x04,0x01,0x82,0x02,0x02,0x03,0x01,0x05,0x01,0x04,0x02,0x02,0x02,0x02,0x0c,0x01,0xf0,0x00 };
const uint8_t light_icon[] = {  0x80, 0x00, 0x84, 0x10, 0x08, 0x08, 0xc0, 0x01, 0x31, 0x46, 0x12, 0x24, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x12, 0x24, 0x31, 0x46, 0xc0, 0x01, 0x08, 0x08, 0x84, 0x10, 0x80, 0x00, 0x00, 0x00 };
const uint8_t Logo[]= { 0x03,0x00,0x36,0xfc,0xfb,0x07,0xfc,0x03,0x00,0x36,0xfc,0xfb,0x0f,0xfe,0x03,0x00,0x36,0x60,0x18,0x0e,0xc6,0x03,0x00,0x36,0x60,0x18,0x0c,0xc3,0x03,0x00,0x36,0x60,0x18,0x06,0xc3,0x03,0x00,0x36,0x60,0xf8,0x87,0xff,0x03,0x00,0x36,0x60,0xf8,0x8f,0xff,0x03,0x00,0x36,0x60,0x18,0xcc,0xc0,0x07,0x00,0x37,0x60,0x18,0xcc,0xc0,0xfe,0xff,0xf3,0x6f,0x18,0x6c,0xc0,0xfc,0xff,0xf1,0x6f,0x18,0x6c,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3c,0xdf,0xe7,0xff,0x79,0x3c,0x1e,0x66,0x04,0x31,0x00,0xe3,0x10,0x33,0x43,0x04,0x19,0x00,0xa6,0x90,0x61,0x03,0x04,0x0d,0x00,0xac,0xd1,0xc0,0x0e,0x04,0x05,0x00,0x28,0x51,0x80,0x18,0x04,0x05,0x00,0x28,0x51,0x80,0x70,0x04,0x0d,0x00,0x2c,0x53,0x80,0xc1,0x04,0x09,0x00,0x24,0xd2,0xc0,0xc3,0x8c,0x19,0x00,0x26,0x96,0x61,0x66,0xdc,0x31,0x00,0x23,0x14,0x33,0x3c,0x70,0xe0,0xff,0x79,0x1c,0x1e };

// Callback audio
void AudioCallback(float *in, float *out, size_t size) {
    for (size_t i = 0; i < size; i += 2) {
        float mic_signal = in[i];  // Segnale dal microfono

        filter.Process(mic_signal);
        float filtered_signal = filter.Low();

        // Applicare Overdrive
        float overdriven_signal = overdrive.Process(filtered_signal);

        // Applicare Flanger
        float flanged_signal = flanger.Process(overdriven_signal);

        // Mix il segnale dry e wet del flanger (wet 20%)
        float wet_signal = flanged_signal;
        float dry_signal = overdriven_signal;
        float flanger_mixed_signal = (0.4f * wet_signal) + (0.9f * dry_signal);  // 20% wet, 80% dry
        
        // Applicare Reverb
        float reverb_signal;
        reverb.Process(flanger_mixed_signal, flanger_mixed_signal, &reverb_signal, &reverb_signal);

        // Combinare wet e dry del reverb
        float wet_reverb_signal = reverb_signal;
        float dry_signal_after_reverb = flanger_mixed_signal; // Il dry segnale rimane lo stesso
        float reverb_mixed_signal = (0.7f * wet_reverb_signal) + (0.9f * dry_signal_after_reverb); // Mix dinamico

        out[i] = reverb_mixed_signal;     
        out[i + 1] = reverb_mixed_signal;
    }
}

// Variabili per l'animazione
float wavePhase = 0.9; // Fase dell'onda sonora
const float waveSpeed = 0.4; // Velocità di animazione

void drawWave() {
    for (int x = 0; x < 128; x++) {
        int y = 51 + 8 * sin(wavePhase + x * 0.2); // Calcola posizione dell'onda
        oled.drawPixel(x, y); // Disegna il pixel
    }
    wavePhase += waveSpeed; // Aggiorna la fase dell'onda
}

void setup() {
    hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
    DAISY.begin(AudioCallback);

    // Configurazione del filtro passa-basso
    filter.Init(48000.0f);
    filter.SetFreq(20000.0f);    // 20 kHz     
    filter.SetRes(0.5f);         // Risonanza non moderata

    reverb.Init(48000.0f);
    reverb.SetFeedback(0.7f);
    reverb.SetLpFreq(12000.0f);

    overdrive.Init();
    overdrive.SetDrive(0.2f);  // Da 0.0 → 0.3 per evitare distorsione totale;

    flanger.Init(48000.0f);
    flanger.SetLfoDepth(0.1f);
    flanger.SetLfoFreq(0.7f);

    // Configurazione OLED
    oled.begin();
    oled.setFont(u8g2_font_helvR08_tf);

    // Configura i pin per I2C
    Wire.setSDA(15);
    Wire.setSCL(14);
    Wire.begin();

    // Inizializzazione del sensore AHT20
    if (!aht.begin()) {
        oled.clearBuffer();
        oled.setCursor(0, 20);
        oled.print("AHT20 ERR");
        oled.sendBuffer();
        while (1);
    }

    // Configurazione dei pin pulsante e LED
    pinMode(buttonPin, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
}

void loop() {
    // Gestione del pulsante
    bool currentButtonState = !digitalRead(buttonPin); 
    if (currentButtonState && !lastButtonState) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState ? HIGH : LOW);
    }
    lastButtonState = currentButtonState;

    // Lettura sensori
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    float lightLevel = analogRead(A1) / 1023.0 * 100;

    // Lettura di un valore da un sensore o da un potenziometro
    float cutoffFreq = analogRead(A1);  // Leggi il valore dal pin A1
    cutoffFreq = map(cutoffFreq, 0, 1023, 200, 10000);  // Mappa il valore in Hz
    filter.SetFreq(cutoffFreq);  // Imposta la frequenza di taglio del filtro

    // Riverbero: Umidità mappata con una sensibilità maggiore
    float reverbFeedback = pow(humidity.relative_humidity / 100.0f, 2); // Curva quadratica per maggiore sensibilità
    reverbFeedback = constrain(reverbFeedback, 0.1f, 0.9f);            // Evitare valori estremi
    reverb.SetFeedback(reverbFeedback);

    // Aumento progressivo dell'overdrive con temperatura
    float overdriveDrive = map(temp.temperature, 0, 40, 0.2f, 0.9f);
    overdriveDrive = constrain(overdriveDrive, 0.2f, 0.9f);
    overdrive.SetDrive(overdriveDrive);

    // Flanger: Intensifica in base all'umidità
    float flangerDepth = humidity.relative_humidity / 100.0f; // Più forte con alta umidità
    flangerDepth = constrain(flangerDepth, 0.1f, 0.9f); // Mantieni dentro i limiti
    flanger.SetLfoDepth(flangerDepth);

    // Aggiornamento OLED
    oled.clearBuffer();

    if (ledState) {
        drawWave(); // Disegna l'onda sonora
    } else {
        oled.drawLine(0, 51, 127, 51); // Disegna una linea retta
    }

    // Disegna icone e valori
    oled.drawXBM(4, 15, 56, 24, Logo);
    oled.drawXBM(7, 65, 16, 16, temp_icon);
    oled.setCursor(25, 74);
    oled.print(temp.temperature);
    oled.print(" C");

    oled.drawXBM(6, 85, 16, 16, humid_icon);
    oled.setCursor(25, 96);
    oled.print(humidity.relative_humidity);
    oled.print("%");

    oled.drawXBM(4, 105, 16, 16, light_icon);
    oled.setCursor(25, 116);
    oled.print(lightLevel);
    oled.print("%");

    oled.sendBuffer();
    delay(5);
}

