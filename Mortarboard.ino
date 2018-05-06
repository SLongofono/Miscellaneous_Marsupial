#define NUMLEDS 12
#define NUMPATTERNS 14
#define PERSISTDELAY 252                        // Persistence of vision time
#define LAMPDELAY PERSISTDELAY/NUMLEDS          // Delay between individual LED illuminations
#define ITERATIONS 4                            // Iteration delay between changing patterns
                                                // Changes patterns roughly every second (1008 ms for persistDelay of 252)

// Current LED vals
int LEDS[NUMLEDS];


// Patterns for animation of LED vals
int patterns[NUMPATTERNS][NUMLEDS] = {
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {LOW,LOW,LOW,LOW,HIGH,HIGH,HIGH,HIGH,LOW,LOW,LOW,LOW},
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,HIGH,HIGH,HIGH,HIGH},
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH},
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH},
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH},
  {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW},
  {HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH,HIGH}
};

// Counts iterations on the active pattern
int *patternCount = new int[1];

// Tracks next active pattern
int *nextPatt = new int[1];

// Updates LEDs to match the given pattern, and advances the pattern to the next in sequence
void changePattern(int *patt){
  for(int i = 0; i<NUMLEDS; ++i){
    LEDS[i] = patterns[*patt][i];  
  }
  if(*patt == NUMPATTERNS-1){
    *patt = 0;  
  }
  else{
    (*patt)++;  
  }
}

// Track iterations and change pattern when appropriate
void advanceState(int *count){
  if(*count == ITERATIONS){
    *count = 0;
    changePattern(nextPatt);
  }
  else{
    (*count)++;
  }
}

void setup() {
  *nextPatt = 0;
  *patternCount = 0;
}

void loop() {
  // Update state
  advanceState(patternCount);
  
  // Cycle through and flash LEDs
  for(int i = 0; i<NUMLEDS; ++i){
    digitalWrite(i, LEDS[i]); 
    delay(LAMPDELAY);
    digitalWrite(i, LOW);
  }
}
