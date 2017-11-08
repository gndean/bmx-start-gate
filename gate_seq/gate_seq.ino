#include <SD.h>
#include <TMRpcm.h>

const int PIN_UTRIG = 3;
const int PIN_UECHO = 2;
const int PIN_R = 8;
const int PIN_Y1 = 7;
const int PIN_Y2 = 6;
const int PIN_G = 5;
const int PIN_PZ = 9;
const int PIN_SD_CS = 10;

enum {
  STATE_TIMING,
  STATE_BEAM_BROKEN,
  STATE_DONE
};

TMRpcm tmrpcm; // This needs to be global, otherwise resetting the Arduino fails to play audio

int state;
int thresh_dist = 400;
unsigned long broken_beam_ms;

void setup() {
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_Y1, OUTPUT);
  pinMode(PIN_Y2, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_PZ, OUTPUT);
  pinMode(PIN_UTRIG, OUTPUT);
  pinMode(PIN_UECHO, INPUT);
  
  Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print("Initializing SD card...");
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  //sd_print_root
  
  tmrpcm.speakerPin = PIN_PZ;
  tmrpcm.setVolume(5);
  tmrpcm.quality(1);
  tmrpcm.loop(0);

  Serial.println("Playing...");

  tmrpcm.play((char*)"RANDOM.WAV");
  
  // Calibrate by measuring average distance for n seconds
  unsigned long start_ms = millis();

  unsigned long total_dist = 0;
  int cnt_dist = 0;
  
  while ((millis() - start_ms) < 4000) {
    total_dist += measure_distance();
    cnt_dist++;

    delay(10);
  }
  int avg_dist = total_dist / cnt_dist;
  thresh_dist = avg_dist * 2 / 3;

  Serial.print("Calibrated distance: ");
  Serial.println(avg_dist);
  Serial.print("Threshold distance: ");
  Serial.println(thresh_dist);

  tmrpcm.play((char*)"READY.WAV");
  
  delay(3000);

  show_start_light(PIN_R);
  tmrpcm.play((char*)"T_LIGHT.WAV");
  delay(120);
  
  show_start_light(PIN_Y1);
  tmrpcm.play((char*)"T_LIGHT.WAV");
  delay(120);

  show_start_light(PIN_Y2);
  tmrpcm.play((char*)"T_LIGHT.WAV");
  delay(120);

  show_start_light(PIN_G);
  tmrpcm.play((char*)"T_GATE.WAV");
  delay(2250);

  state = STATE_TIMING;
}

void loop() {
  if (STATE_TIMING == state) {
    int dist = measure_distance();
  
    Serial.print("Distance: ");
    Serial.println(dist);
      
    if (dist <= thresh_dist) {
      broken_beam_ms = millis();
      state = STATE_BEAM_BROKEN;
      Serial.println("Beam broken");
    }
  }
  else if (STATE_BEAM_BROKEN == state) {
    int dist = measure_distance();

    Serial.print("Distance: ");
    Serial.println(dist);
     if (dist > thresh_dist) {
      Serial.println("Beam lost");
      state = STATE_TIMING;
      }
      else if ((millis() - broken_beam_ms) > 100) {
        // We're done
        tmrpcm.play((char*)"T_LIGHT.WAV");
  
        digitalWrite(PIN_R, HIGH);
        digitalWrite(PIN_Y1, HIGH);
        digitalWrite(PIN_Y2, HIGH);
        digitalWrite(PIN_G, HIGH);
  
        delay(500);
  
        show_start_light(-1);
  
        state = STATE_DONE;
      }
      else {
        // Beam is broken. Wait a little longer
      }
  }
  else {
    // Do nothing
    delay(100);
  }
}

void show_start_light(int light)
{
  digitalWrite(PIN_R, light == PIN_R ? HIGH : LOW);
  digitalWrite(PIN_Y1, light == PIN_Y1 ? HIGH : LOW);
  digitalWrite(PIN_Y2, light == PIN_Y2 ? HIGH : LOW);
  digitalWrite(PIN_G, light == PIN_G ? HIGH : LOW);
}

