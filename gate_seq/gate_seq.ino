
//#define USE_LEDS 1
#define USE_NEOSTRIP 1


/*
 * TODO:
 * - Cap time at 10 seconds
 * - Don't measure distance while playing audio - it's unreliable
 * - Add electromagnet / solenoid activation
 * - Support config
 *   - Distance threshold
 */

#include <SD.h>
#include <TMRpcm.h>
#include <TM1637.h>

const int PIN_UTRIG = 4;
const int PIN_UECHO = 2;
const int PIN_PZ = 9;
const int PIN_SD_CS = 10;
const int PIN_SEGMENT_CLK = 7;
const int PIN_SEGMENT_DIO = 8;
const int PIN_START_BUTTON = A1;
const int PIN_RESET_BUTTON = A0;

#ifdef USE_LEDS
const int PIN_R = 8;
const int PIN_Y1 = 7;
const int PIN_Y2 = 6;
const int PIN_G = 5;
#endif // USE_LEDS

const int NEOPIXEL_BRIGHTNESS = 100; // Setting to >= 200 lead to unreliability - the stick would stop responding

const int BEAM_BREAK_MIN_DURATION = 50; // Milliseconds

enum {
  STATE_WAITING_FOR_START,
  STATE_START_SEQ,
  STATE_TIMING,
  STATE_BEAM_BROKEN
};

enum {
  LIGHT_ALLOFF,
  LIGHT_WAIT_START,
  LIGHT_R,
  LIGHT_Y1,
  LIGHT_Y2,
  LIGHT_G,
  LIGHT_FINISH
};

#ifdef USE_NEOSTRIP
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel neo_strip = Adafruit_NeoPixel(8, 6, NEO_GRB + NEO_KHZ800);
#endif // USE_NEOSTRIP

TMRpcm tmrpcm; // This needs to be global, otherwise resetting the Arduino fails to play audio
TM1637 segment(PIN_SEGMENT_CLK,PIN_SEGMENT_DIO);

int state;
int thresh_dist = 400;
unsigned long broken_beam_ms;
unsigned long timing_start_ms;

void setup() {
#ifdef USE_LEDS
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_Y1, OUTPUT);
  pinMode(PIN_Y2, OUTPUT);
  pinMode(PIN_G, OUTPUT);
#endif // USE_LEDS
  pinMode(PIN_PZ, OUTPUT);
  pinMode(PIN_UTRIG, OUTPUT);
  pinMode(PIN_UECHO, INPUT);
  pinMode(PIN_START_BUTTON, INPUT);
  pinMode(PIN_RESET_BUTTON, INPUT);

  segment.set(7);//BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  segment.init();
  segment.clearDisplay();

#ifdef USE_NEOSTRIP
  neo_strip.begin();
  neo_strip.show();
#endif // USE_NEOSTRIP

  randomSeed(analogRead(A5) + analogRead(A4) + analogRead(A3) + analogRead(A2) + analogRead(A1) + analogRead(A0));

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

  state_enter_waiting_for_start();
}

void loop() {
  bool test_loop_mode = false;
  
  if (STATE_WAITING_FOR_START == state) {
    // Wait for start button
    if (HIGH == digitalRead(PIN_START_BUTTON) || test_loop_mode) {
      Serial.println("Start button pressed");
      state = STATE_START_SEQ;
    }
  }
  if (STATE_START_SEQ == state) {
    show_start_light(LIGHT_ALLOFF);
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
      random();

      if (check_for_reset_button()) {
        // Abort. State will be reset
        return;
      }
    }
    int avg_dist = total_dist / cnt_dist;
    thresh_dist = avg_dist * 2 / 3;
  
    Serial.print("Calibrated distance: ");
    Serial.println(avg_dist);
    Serial.print("Threshold distance: ");
    Serial.println(thresh_dist);
  
    Serial.println("Riders ready. Watch the gate.");
    tmrpcm.play((char*)"READY.WAV");
  
    // Wait till finished talking
    while (tmrpcm.isPlaying()) {
      delay(10);
      if (check_for_reset_button()) {
        // Abort. State will be reset
        return;
      }
    }
    int delay_ms = random(100, 2700);
    
    Serial.print("Random delay = ");
    Serial.println(delay_ms);

    start_ms = millis();
    while ((millis() - start_ms) < delay_ms) {
      delay(10);
      if (check_for_reset_button()) {
        // Abort. State will be reset
        return;
      }
    }
  
    show_start_light(LIGHT_R);
    tmrpcm.play((char*)"T_LIGHT.WAV");
    delay(120);             
    
    show_start_light(LIGHT_Y1);
    tmrpcm.play((char*)"T_LIGHT.WAV");
    delay(120);
  
    show_start_light(LIGHT_Y2);
    tmrpcm.play((char*)"T_LIGHT.WAV");
    delay(120);
  
    Serial.println("Green and gate drop");
    tmrpcm.quality(0); // Drop quality now to minimise interference with usonic sensor timing
    show_start_light(LIGHT_G);
    tmrpcm.play((char*)"T_GATE.WAV");
    timing_start_ms = millis();
  
    // Wait till finised playing audio before timing to ensure consistent usonic signal
    while (tmrpcm.isPlaying()) {
      delay(10);
    }
  
  
    Serial.println("Timer active");
    state = STATE_TIMING;
  }

  else if (STATE_TIMING == state) {
    int dist = measure_distance();
  
    //Serial.print("Distance: ");
    //Serial.println(dist);
      
    if (dist <= thresh_dist || test_loop_mode) {
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

    if (dist > thresh_dist && (!test_loop_mode)) {
      Serial.println("Beam lost");
      state = STATE_TIMING;
      }
      else if (test_loop_mode || ((millis() - broken_beam_ms) >= BEAM_BREAK_MIN_DURATION)) {
        // We're done
        // Calculate time taken
        unsigned long time_taken_ms = broken_beam_ms - timing_start_ms;

        // Format as seconds
        String seconds = String(time_taken_ms / 1000.0, 2);

        // Show on display
        int8_t disp[4];
        disp[0] = 17; // space
        disp[1] = seconds.c_str()[0] - '0';
        disp[2] = seconds.c_str()[2] - '0';
        disp[3] = seconds.c_str()[3] - '0';
        segment.point(true);                   
        segment.display(disp);

        Serial.print("Finished! Time taken = ");
        Serial.println(seconds);

        // Light up start lights
        show_start_light(LIGHT_FINISH);

        // We can switch back to full quality now
        tmrpcm.quality(1);
        tmrpcm.play((char*)"DING.WAV");
        
        while (tmrpcm.isPlaying()) {
          delay(10);
        }

        if (test_loop_mode) {
          delay(5000);
        }

        // Clear start lights
        show_start_light(LIGHT_ALLOFF);

        // Output how long we took
        speak_seconds(seconds);

        // Back to the waiting state
        state_enter_waiting_for_start();
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

void state_enter_waiting_for_start() {
  show_start_light(LIGHT_WAIT_START);
  state = STATE_WAITING_FOR_START;
}

void show_start_light(int light)
{
#ifdef USE_LEDS
  if (LIGHT_FINISH == light) {
    digitalWrite(PIN_R, HIGH);
    digitalWrite(PIN_Y1, HIGH);
    digitalWrite(PIN_Y2, HIGH);
    digitalWrite(PIN_G, HIGH);
  }
  else if (LIGHT_WAIT_START == light) {
    digitalWrite(PIN_R, LOW);
    digitalWrite(PIN_Y1, HIGH);
    digitalWrite(PIN_Y2, LOW);
    digitalWrite(PIN_G, LOW);
   }
  else {
    digitalWrite(PIN_R, light == LIGHT_R ? HIGH : LOW);
    digitalWrite(PIN_Y1, light == LIGHT_Y1 ? HIGH : LOW);
    digitalWrite(PIN_Y2, light == LIGHT_Y2 ? HIGH : LOW);
    digitalWrite(PIN_G, light == LIGHT_G ? HIGH : LOW);
  }
#endif // USE_LEDS

#ifdef USE_NEOSTRIP
  int b = NEOPIXEL_BRIGHTNESS;

  if (LIGHT_FINISH == light) {
    for (int i = 0;i < 8;i++) {
      neo_strip.setPixelColor(i, 0, b, 0);
    }
  }
  else if (LIGHT_WAIT_START == light) {
    for (int i = 0;i < 8;i++) {
      neo_strip.setPixelColor(i, 0, 0, i == 7 ? b : 0);
    }    
  }
  else {
    neo_strip.setPixelColor(7, light == LIGHT_R ? b : 0, 0, 0);
    neo_strip.setPixelColor(6, light == LIGHT_R ? b : 0, 0, 0);
    neo_strip.setPixelColor(5, light == LIGHT_Y1 ? b : 0, light == LIGHT_Y1 ? b : 0, 0);
    neo_strip.setPixelColor(4, light == LIGHT_Y1 ? b : 0, light == LIGHT_Y1 ? b : 0, 0);
    neo_strip.setPixelColor(3, light == LIGHT_Y2 ? b : 0, light == LIGHT_Y2 ? b : 0, 0);
    neo_strip.setPixelColor(2, light == LIGHT_Y2 ? b : 0, light == LIGHT_Y2 ? b : 0, 0);
    neo_strip.setPixelColor(1, 0, light == LIGHT_G ? b : 0, 0);
    neo_strip.setPixelColor(0, 0, light == LIGHT_G ? b : 0, 0);    
  }
  neo_strip.show();
#endif // USE_NEOSTRIP
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

// Returns state to waitint for start and returns true if reset button pressed
bool check_for_reset_button()
{
  if (HIGH == digitalRead(PIN_RESET_BUTTON)) {
    Serial.println("Reset button pressed");
    tmrpcm.play((char*)"ABORT.WAV");
    state_enter_waiting_for_start();
    return true;
  }

  return false;
}

