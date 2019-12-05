// StartKnop
#define KNOP_DEBOUNCE 50
#define KNOP_LANGDRUKKEN 2000

#define TIJD_MINUTEN 60
#define TIJD_SECONDEN 00

// Let op: Alles wat in een ISR gebruikt wordt, moet volatile zijn
volatile bool xDp = 1;
// Huidige waarde voor timer gestart
volatile bool xTimerGestart = 0;
// Huidige waarde op het display (0000..9999)
volatile int iDisplayWaarde;

int iTijdInSeconden = 0;


enum mp3s
{
  mp3_gehaald = 1,
  mp3_gefaald = 2
};

// Afblijven!!
const unsigned char ucNummers[12] = 
{
  // PC3, PC2, PC1, PC0, PD5, PD4, PD3, PD2
  //  D    E   dP    B    G    A    C    F
  0xD7, // 0
  0x12, // 1
  0xDC, // 2
  0x9E, // 3
  0x1B, // 4
  0x8F, // 5
  0xCF, // 6
  0x16, // 7
  0xDF, // 8
  0x9F, // 9
  0x08, // -
  0xFF  // 8. <- Alleen test! 
};

void setup()
{ 
  // Uitgangen
  // Digits 
  DDRB |= 0x03; // PB 1..0 -> Dig 3, 2
  DDRD |= 0xE0; // PD 7..5 -> Dig 4, 1, 0
  
  // Segmenten
  DDRC |= 0x0F; // PC 3..0 -> D, E, DP, B
  DDRB |= 0x1C; // PB 4..2 -> G, A, C, F
  
  // Knoppen
  // Ingangen
  DDRD &= ~((1 << PD2) | (1 << PD3));
  // Pullup-weerstanden aanzetten
  PORTD |= (1 << PD2) | (1 << PD3);
  
  // Display uit
  // Digits
  PORTB |= 0x03;
  PORTD |= 0xE0;
  // Segmenten
  PORTC |= 0x0F;
  PORTB |= 0x1C;

  // Timed interrupt voor multiplexen van display
  // Timer resetten
  TCCR2B = TCCR2A = TIMSK2 = 0;
  // Prescaler instellen op clkio/128 (mogelijk: uit, 1, 8, 32, 64, 128, 256, 1024)
  TCCR2B |= 0x5;
  // Timer overflow interrupt inschakelen
  TIMSK2 |= (1 << TOIE2);

  // Stopknop interrupt (PD3 = INT1)
  // Dalende flank op INT1 genereert een interrupt
  EICRA = (1 << ISC11);
  // Interrupt inschakelen
  EIMSK = (1 << INT1);

  // Alle (ingestelde) interrupts inschakelen
  sei();

  reset_tijd();
  
  // MP3-module instellen
  initialiseer_mp3();
}

void loop()
{
  // Lokale variabelen declareren
  // Statische variabelen (blijven behouden)
  static unsigned long ulVorigeMillis = 0;

  // Tijdelijke variabelen (worden steeds opnieuw geinitialiseerd)
  unsigned long ulNuMillis = millis();

  // Functie van de startknop
  startKnop(xTimerGestart, iTijdInSeconden == 0);
  
  // Indien de timer gestart is en het verschil tussen de actuele tijd en de vorige tijd
  // is hoger dan of gelijk aan 1000 (één seconde)
  if(xTimerGestart && ulNuMillis - ulVorigeMillis >= 500)
  {
    if(xDp == 1)
      xDp = 0;
    else
    {
      xDp = 1;
      if(iTijdInSeconden == 0)
      {
        xTimerGestart = 0;
        speelMp3(mp3_gefaald);
      }
      else
      {
        // Dan een seconde aftellen
        iTijdInSeconden--;
        // Indien de seconden lager zijn dan 0
      }
    }

    // Dit tijdstip onthouden voor de volgende keer
    ulVorigeMillis = ulNuMillis;
  }
  // : aan als timer niet gestart is.
  if(xTimerGestart == 0)
    xDp = 1;

  iDisplayWaarde = (iTijdInSeconden / 60) * 100 + iTijdInSeconden % 60;
}

// Multiplex ISR
ISR(TIMER2_OVF_vect)
{
  static volatile int iDigitTeller = 0;
  //static volatile int iHelderheidTeller = 0;
  unsigned char ucHuidigeNummerData;

  iDigitTeller++;
  if(iDigitTeller >= 5) iDigitTeller = 0;

  // Digit eerst uitzetten (om ghosting te voorkomen)
  // Digits
  PORTB |= 0x03;
  PORTD |= 0xE0;
  // Segmenten
  PORTC |= 0x1F;
  PORTB |= 0x3F;
  
  switch(iDigitTeller)
  {
    case 0:
      if(iDisplayWaarde >= 1000)
        ucHuidigeNummerData = ucNummers[(iDisplayWaarde / 1000) % 10];
      else if(iDisplayWaarde < 0)
        ucHuidigeNummerData = ucNummers[10];
      else
        ucHuidigeNummerData = 0;
      break;
    case 1:
      ucHuidigeNummerData = ucNummers[(iDisplayWaarde / 100) % 10];
      break;
    case 2:
      ucHuidigeNummerData = ucNummers[(iDisplayWaarde / 10) % 10];
      break;
    case 3:
      ucHuidigeNummerData = ucNummers[(iDisplayWaarde / 1) % 10];
      break;
    case 4:
      // H/M scheidingsteken zit op segment F van digit 4
      ucHuidigeNummerData = (xDp ? 0x01 : 0x00); //| (xBuzzer ? 0x20 : 0x00);
      break;
  }
  
  // Segmenten klaarzetten voor het volgende digit
  PORTC &= ~(ucHuidigeNummerData >> 4);
  PORTB &= ~(ucHuidigeNummerData << 2);

  // Digits om de beurten aanzetten
  switch(iDigitTeller)
  {
    case 0: PORTD &= ~(1 << 5); break;
    case 1: PORTD &= ~(1 << 6); break;
    case 2: PORTB &= ~(1 << 0); break;
    case 3: PORTB &= ~(1 << 1); break;
    case 4: PORTD &= ~(1 << 7); break;
    default: break; 
  }
}

ISR(INT1_vect)
{
  if(xTimerGestart){
    xTimerGestart = 0;
    speelMp3(mp3_gehaald);
  }
}
////////////////////////////
//////// MP3 module ////////
////////////////////////////
void initialiseer_mp3()
{
  Serial.begin(9600);
}
uint16_t fc_crc(unsigned char ucData[10])
{
  uint16_t crc = 0;
  for(int i = 1; i <= 6; i++)
  {
    crc += ucData[i];
  }
  crc = -crc;
  return crc;
}
// Speelt een muziekje uit de map '01'
// Nummer moet tussen 1 en 255 liggen
void speelMp3(int iNummer)
{
  if(iNummer <= 0)  iNummer = 1;
  if(iNummer > 255) iNummer = 255;

  unsigned char ucMp3Data[10] = 
  {
    0x7E, 0xFF, 0x06, 0x0F, 0x00, 0x01, (uint8_t)iNummer, 0x00, 0x00, 0xEF
  };
  // Speel geluid
  // Cyclic Redundancy Check op de data uitvoeren
  uint16_t crc = fc_crc(ucMp3Data);

  // CRC invullen in datapakket
  ucMp3Data[7] = crc >> 8;
  ucMp3Data[8] = crc & 0xFF;

  // Data naar MP3-module sturen
  Serial.write(ucMp3Data, 10);
}

///////////////////////////
//////// Startknop ////////
///////////////////////////
void startKnop(volatile bool &xStartSignaal, bool xBlokkeerStart)
{
  static unsigned long ulStartMillis = 0;
  static bool xOudKnopStatus = 0;
  static bool xEersteKeerFlag = 0;
  
  unsigned long ulMillis = millis();
  bool xKnopStatus = ~(PIND >> PD2) & 0x01;
  
  if(xKnopStatus != xOudKnopStatus)
  {
    unsigned long ulDelta = ulMillis - ulStartMillis;
    
    xOudKnopStatus = xKnopStatus;
    if(xKnopStatus)
      ulStartMillis = ulMillis;
    else{
      if(ulDelta >= KNOP_DEBOUNCE && ulDelta < KNOP_LANGDRUKKEN)
      {
        if(xTimerGestart == 0 && xBlokkeerStart == 0) 
          // Timer starten inverteren (gestart = stoppen en andersom)
          xStartSignaal = 1;
        else
          xStartSignaal = 0;
      }
      xEersteKeerFlag = 0;
    }
  }

  if(xKnopStatus && ulMillis - ulStartMillis >= KNOP_LANGDRUKKEN && xEersteKeerFlag == 0)
  {
    xEersteKeerFlag = 1;
    xTimerGestart = 0;
    reset_tijd();
  }
}

void reset_tijd()
{
  iTijdInSeconden = TIJD_SECONDEN + TIJD_MINUTEN * 60;
}
