/*
 * TODO:
 * - Randomise start
 * - Cap time at 10 seconds
 * - Don't measure distance while playing audio - it's unreliable
 * - Have distance threshold set via pot. Setting pot to 0 uses fixed distance (2m, say)
 */

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

const int BEAM_BREAK_MIN_DURATION = 50; // Milliseconds

enum {
  STATE_TIMING,
  STATE_BEAM_BROKEN,
  STATE_DONE
};

TMRpcm tmrpcm; // This needs to be global, otherwise resetting the Arduino fails to play audio

int state;
int thresh_dist = 400;
unsigned long broken_beam_ms;
unsigned long timing_start_ms;

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

  Serial.println("OK riders. Random start.");
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

  Serial.println("Riders ready. Watch the gate.");
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

  Serial.println("Green and gate drop");
  tmrpcm.quality(0); // Drop quality now to minimise interference with usonic sensor timing
  show_start_light(PIN_G);
  tmrpcm.play((char*)"T_GATE.WAV");
  timing_start_ms = millis();

  // Wait till finised playing audio before timing to ensure consistent usonic signal
  //while (tmrpcm.isPlaying()) {
  //  delay(10);
  //}

  Serial.println("Timer active");
  state = STATE_TIMING;
}

void loop() {
  if (STATE_TIMING == state) {
    int dist = measure_distance();
  
    //Serial.print("Distance: ");
    //Serial.println(dist);
      
    if (dist <= thresh_dist) {
      broken_beam_ms = millis();
      state = STATE_BEAM_BROKEN;
      Serial.println("Beam broken");
      Serial.print("Distance: ");
      Serial.println(dist);
  
    }

    delay(10);
  }
  else if (STATE_BEAM_BROKEN == state) {
    int dist = measure_distance();

    if (dist > thresh_dist) {
      Serial.println("Beam lost");
      state = STATE_TIMING;
      }
      else if ((millis() - broken_beam_ms) >= BEAM_BREAK_MIN_DURATION) {
        // We're done
        // Calculate time taken
        unsigned long time_taken_ms = broken_beam_ms - timing_start_ms;

        // Format as seconds
        String seconds = String(time_taken_ms / 1000.0, 2);

        Serial.print("Finished! Time taken = ");
        Serial.println(seconds);

        // Light up start lights
        digitalWrite(PIN_R, HIGH);
        digitalWrite(PIN_Y1, HIGH);
        digitalWrite(PIN_Y2, HIGH);
        digitalWrite(PIN_G, HIGH);

        // We can switch back to full quality now
        tmrpcm.quality(1);
        tmrpcm.play((char*)"DING.WAV");
        
        while (tmrpcm.isPlaying()) {
          delay(10);
        }

        // Clear start lights
        show_start_light(-1);

        // Output how long we took
        speak_seconds(seconds);
  
        state = STATE_DONE;
      }
      else {
        // Beam is broken. Wait a little longer
        delay(10);
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

void speak_seconds(String seconds)
{
  for (int i = 0;i < seconds.length();i++) {
    char c = seconds.c_str()[i];

    if (c >= '0' && c <= '9') {
      String fname = String(c);
      fname.concat(".WAV");
      tmrpcm.play((char*)fname.c_str());      
    }
    else if (c == '.') {
      tmrpcm.play((char*)"POINT.WAV");
    }
    while (tmrpcm.isPlaying()) {
      delay(10);
    } 
  }
  tmrpcm.play((char*)"SECONDS.WAV");
}

