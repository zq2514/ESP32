int led_pin =25;


void setup() {
  // put your setup code here, to run once:
  pinMode(led_pin,OUTPUT);

  
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(500);

  digitalWrite(led_pin,HIGH);
  delay(500);

  digitalWrite(led_pin,LOW);
  
}
