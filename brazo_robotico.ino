#include <Servo.h>

Servo base;
Servo brazo;
Servo antebrazo;
Servo garra;

int posBase = 90;
int posBrazo = 90;
int posAntebrazo = 90;
int posGarra = 20;
char cmd;

void setup() {
  Serial.begin(9600);

  base.attach(4);
  brazo.attach(5);
  antebrazo.attach(6);
  garra.attach(7);

  base.write(posBase);
  brazo.write(posBrazo);
  antebrazo.write(posAntebrazo);
  garra.write(posGarra);
}

void loop() {
  if (Serial.available()) {
    cmd = Serial.read();
  }

  // Movimiento más rápido y fluido
  switch (cmd) {
    case 'F': posBrazo = constrain(posBrazo + 3, 0, 180); break;
    case 'B': posBrazo = constrain(posBrazo - 3, 0, 180); break;
    case 'L': posBase = constrain(posBase + 3, 0, 180); break;
    case 'R': posBase = constrain(posBase - 3, 0, 180); break;
    case 'T': posAntebrazo = constrain(posAntebrazo + 3, 0, 180); break;
    case 'X': posAntebrazo = constrain(posAntebrazo - 3, 0, 180); break;
    case 'S': posGarra = constrain(posGarra + 2, 10, 50); break;
    case 'C': posGarra = constrain(posGarra - 2, 10, 50); break;
  }

  base.write(posBase);
  brazo.write(posBrazo);
  antebrazo.write(posAntebrazo);
  garra.write(posGarra);

  delay(10); // Menor retardo → movimientos más suaves
}