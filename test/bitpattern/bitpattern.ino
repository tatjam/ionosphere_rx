#define DATA 9
#define TRIGGER 8

void setup() {
  pinMode(DATA, OUTPUT);
  pinMode(TRIGGER, OUTPUT);
}

void loop() {
  digitalWrite(DATA, HIGH);
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(1);
  digitalWrite(DATA, LOW);
  digitalWrite(TRIGGER, LOW);
  delayMicroseconds(1);
  digitalWrite(DATA, HIGH);
  delayMicroseconds(1);
  digitalWrite(DATA, LOW);
  delayMicroseconds(1);
  delayMicroseconds(1);
  digitalWrite(DATA, HIGH);
  delayMicroseconds(1);
  delayMicroseconds(1);
  digitalWrite(DATA, LOW);
  delayMicroseconds(1);
  delayMicroseconds(1000);
}
