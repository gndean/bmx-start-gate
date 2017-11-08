// Returns the distance in CM, up to a maximum of 400
int measure_distance()
{
  // Clears the trigPin
  digitalWrite(PIN_UTRIG, LOW);
  delayMicroseconds(2);
  
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(PIN_UTRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_UTRIG, LOW);
  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  // Set timeout to equivalent to 4 metres
  long duration = pulseIn(PIN_UECHO, HIGH, 23529);
  if (duration) {
    // Calculating the distance
    return duration * 0.034 / 2;
  }
  else {
    return 400;
  }
}

