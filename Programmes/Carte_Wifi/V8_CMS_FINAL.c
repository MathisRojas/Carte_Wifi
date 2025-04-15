#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <xc.h>                 //Fichier include générique pour tous les Pics
#include "Uart.h"
#include "pic18f26k22.h"

// Remarque, liste des pragma config dans C:\Program Files\Microchip\xc8\v1.35\docs\chips
#pragma config WDTEN=OFF        //Watchdog contrôlé par SWDTEN
#pragma config WDTPS=512        //Postdiviseur Watchdog -> période 2s environ
#pragma config PWRTEN=OFF       //Power up timer désactivé
#pragma config BOREN=OFF        //
#pragma config BORV=250         //Seuil BOR 2.5V
#pragma config MCLRE=EXTMCLR    //Master reset externe
#pragma config DEBUG=OFF        //Mode debug désactivé
#pragma config LVP=OFF          //Mode programmation Low Voltage désactivé

#pragma config IESO=OFF
#pragma config PRICLKEN=OFF     //primary clock
#pragma config FCMEN=OFF        //fail-safe clock monitor
#pragma config FOSC=INTIO67     //Oscillateur interne


//Fréquence d'horloge choisie: (en MHz)
#define FCLK 1    
#define _XTAL_FREQ 1000000      //define nécessaire pour l'utilisation des fonctions __delay

#if (FCLK==1)||(FCLK==2)||(FCLK==4)||(FCLK==8)||(FCLK==16)
#pragma config PLLCFG=OFF       //PLL désactivée
#endif

#if (FCLK==32)||(FCLK==64)
#pragma config PLLCFG=ON        //PLL désactivée
#endif


//Définition des entrées/sorties du système
#define Reset_R1  LATBbits.LB1   //commande ouvrir relai 1
#define Set_R1    LATBbits.LB0 //commande fermee relai 1
#define Reset_R2   LATBbits.LB3   //commande ouvrir relai 2
#define Set_R2 LATBbits.LB2   //commande fermee relai 2



const char version[] @ 0xF00000 = "V1.00 - LedK22 - 01/10/2023";


//Registre d'état global
volatile unsigned char STATM @ 0x060;   
typedef union {
  struct {
    unsigned TEF    : 1;           //B0: à 1, indique un top d'échabtillonnage
    unsigned VaON   : 1;           //B1: à 1, relai alim activé
    unsigned        : 6;           //
  };
} STATMbits_t;  
STATMbits_t STATMbits @ 0x060;     //Déclaration de STATM bit à bit

#define  TEF_bit     STATMbits.TEF
#define  VaON_bit    STATMbits.VaON


//Variables de fonctionnement du module en Banque 0:
unsigned short cpt        @0x061;



//-----------------------------------------------------------------------------------------------------------------
// Fonction pour ouvrir le relai 1
void R1_ouvert(void)
{
    Set_R1=1;
    __delay_ms(10);
    Set_R1=0; 
}
// Fonction pour fermer le relai 1
void R1_fermee(void)
{
    Reset_R1=1;
    __delay_ms(10);
    Reset_R1=0; 
}
// Fonction pour ouvrir le relai 2
void R2_ouvert(void)
{
    Set_R2=1;
    __delay_ms(10);
    Set_R2=0; 
}
// Fonction pour fermer le relai 2
void R2_fermee(void)
{
    Reset_R2=1;
    __delay_ms(10);
    Reset_R2=0; 
}
// Fonction pour envoyer une chaîne de caractères via UART
void envoyer_UART(char* buffer)
{
    for(short i=0; buffer[i]!='\0'; i++)
      {
        UartWriteChar(buffer[i]);
      }
}

// Envoyer une condition de démarrage I2C
void I2C_Start(void) {
    SSP1CON2bits.SEN = 1;  // Générer une condition de start p.255 bit 0 du SSP2CON2
    while (SSP1CON2bits.SEN);  // Attendre la fin de la condition de start
}

// Envoyer une condition d'arrêt I2C
void I2C_Stop(void) {
    SSP1CON2bits.PEN = 1;  // Générer une condition de stop p.255 bit 2 du SSP2CON2
    while (SSP1CON2bits.PEN);  // Attendre la fin de la condition de stop
}

// Envoyer un octet sur le bus I2C
void I2C_Write(unsigned char data) {
    SSP1BUF = data;  // Charger l?octet à envoyer
    while (SSP1STATbits.BF);           // Wait for the data to be sent
    while (SSP1CON2bits.ACKSTAT); // Check for ACK (0 = ACK received)
    //while(SSP1CON2bits.ACKSTAT==1);
    //while(SSPSTATbits.RW);
    //while (!PIR1bits.SSP1IF);  // Attendre que la transmission soit terminée
    //PIR1bits.SSP1IF = 0;       // Effacer le flag
}
// Lire un octet depuis le bus I2C
unsigned char I2C_Read(unsigned char ack0_nack1) {
    unsigned char data;
    SSP1CON2bits.RCEN = 1;  // Activer la réception p.255 bit 3 du SPPCON2
    while (!SSP1STATbits.BF);  // Attendre que la réception soit terminée bit0 p.252
    data = SSP1BUF;  // Lire l?octet reçu
    // Envoyer un ACK ou un NACK
    SSP1CON2bits.ACKDT = ack0_nack1;  // 0 = ACK, 1 = NACK p.255 bit 5 du SPPCON2
    SSP1CON2bits.ACKEN = 1;  // Envoyer le bit d'acknowledge
    while (SSP1CON2bits.ACKEN);  // Attendre la fin de l?acknowledge
    return data;
}
// Initialisation du module I2C
void I2C_Init(void) {
    SSP1CON1 = 0x28; //0b0010 1000 p.254
    //0010(bits7-4) Active le port série et configure le SDAx et le SCL
    //1000(bits3-0) I2C Host mode, clock = FOSC / (4 * (SSPxADD+1))
    SSP1CON2 = 0; //p.255
    SSP1ADD = (_XTAL_FREQ / (4 * 100000)) - 1;  // Baudrate 100 kHz P.258 formule p.251
    SSP1STAT = 0;

}
// Lire la température depuis le capteur SHT21
float SHT21_lecture_Temperature(void) {
    unsigned int valeur=0;
    float temperature=0;
    
    I2C_Start();
    I2C_Write((0x40)<<1);        // Adresse SHT21 0x40 + bit0 d?écriture(0)
    I2C_Write(0xE3);  // Envoyer la commande de mesure de la température 0b1110 0011 = 0xE3
    I2C_Stop();
    I2C_Start();
    I2C_Write((0x40 << 1) | 1);  // Adresse SHT21 0x40 + bit0 de lecture (1)
    valeur = (I2C_Read(0) << 8);    // Lire le MSB avec ACK
    valeur |= I2C_Read(1);          // Lire le LSB avec NACK
    I2C_Stop();
    
    // Conversion des données en température (datasheet SHT21 p.10)
    temperature = (-46.85 + 175.72 * ((float)valeur / 65536.0));
    
    return temperature;
}
// Lire l'humidité depuis le capteur SHT21
float SHT21_lecture_humidite(void) {
    unsigned int valeur=0;
    float humidite=0;
    
    I2C_Start();
    I2C_Write(0x40 << 1);        // Adresse SHT21 0x40 + bit0 d?écriture(0)
    I2C_Write(0xE5);  // Envoyer la commande de mesure de l?humidité 0b1110 0101  = 0xE5
    I2C_Stop();
    I2C_Start();
    I2C_Write((0x40 << 1) | 1);  // Adresse SHT21 0x40 + bit0 de lecture (1)
    valeur = (I2C_Read(0) << 8);    // Lire le MSB avec ACK
    valeur |= I2C_Read(1);          // Lire le LSB avec NACK
    I2C_Stop();
    
    // Conversion des données brutes en humidité (datasheet SHT21 p.10)
    humidite = (-6.0 + 125.0 * ((float)valeur / 65536.0));
    
    return humidite;
}
//fonction pour effacer une chaine de caractère
void effacer(char *chaine) {
    int i = 0;
    while (chaine[i] != '\0') {
        chaine[i++] = '\0';
    }
}
//configuration de l esp(creation du server TCP)
void Iinit_ESP(char *envoi){
  RCSTAbits.CREN = 0;  // Désactive le module réception
  sprintf(envoi,"ATE0\r\n");//commande pour désactiver l'echo
  envoyer_UART(envoi);//désactiver echo
  __delay_ms(300);//attente qui permet à l esp de bien prendre en compte la commande
    sprintf(envoi,"AT+CIPMUX=1\r\n");//On autorise plusieurs connexions parallèles
  envoyer_UART(envoi);//envoi la commande a l'ESP
  __delay_ms(300);//attente qui permet à l esp de bien prendre en compte la commande
  sprintf(envoi,"AT+RFPOWER=0\r\n"); // puissance de TX (0 ~ 82, unit:0.25dBm)
  envoyer_UART(envoi);//envoi la commande a l'ESP
  __delay_ms(300);//attente qui permet à l esp de bien prendre en compte la commande
  sprintf(envoi,"AT+CIPSERVER=1,8080\r\n");//(1 : Active le serveur,8080 : PORT)
  envoyer_UART(envoi);//envoi la commande a l'ESP
  __delay_ms(300);//attente qui permet à l esp de bien prendre en compte la commande
   RCSTAbits.CREN = 1;  // active le module réception
  effacer(envoi); //effacer chaine de caractère
}
void Init_Interruption(void){
      PIE1bits.RC1IE=1; //active les interruptions en réception (sur RX)) p.112
      INTCONbits.TMR0IE=1; //active l interruption sur l ocerflow du timer0 p.109
      INTCONbits.PEIE=1; //active les interruptions périphérique p.109
      INTCON2bits.TMR0IP=0;//définir l'interruption du timer0 comme basse priorité p.110
      IPR1bits.RC1IP=1;//Définir récéption UART comme haut priorité p.121
      RCONbits.IPEN=1; //Active système de priorités d'interruptions p.56
      INTCONbits.GIE=1; //active les interruptions Global p.109
    
}
void Init_Timer0(void)// // Configuration du Timer0 pour 0,2Hz (T=5s)
{
 T0CONbits.TMR0ON=0; //timer désactivé p.154
 T0CONbits.T08BIT=0; //configuration du compteur sur 16bits p.154
 T0CONbits.T0SE=0; // incrémentation sur front montant p.154
 T0CONbits.T0CS=0; //Fosc/4 p.154
 T0CONbits.PSA=0; //Activation du prédiviseur P.154
 T0CONbits.T0PS=4; //prédiviseur par 32 : Fos/(4*32)=1M/128= 7812,5Hz->T=128µs p.154
 // Calcul du préchargement : 65536 - (39062) = 26474
 // En hexadécimal : 26474 = 0x676A
 TMR0H= 0x67; // Partie haute du registre TMR0 (0x67)
 TMR0L = 0x6A; // Partie basse du registre TMR0 (0x6A)
 INTCONbits.TMR0IF = 0;   // RAZ du flag TMR0IF
}
//-----------------------------------------------------------------------------------------------------------------
//Initialisation générale du Pic
void InitPic(void)
{
/*
//Initialisation du vecteur d'interruption
#asm
PSECT VectInterrupt,class=CODE,abs
ORG 008h
  goto _TraitementIT
ORG 018h
  goto _TraitementIT
#endasm
*/
                                   //Au reset INTSRC=0 et MFIOSEL=0

#if FCLK==1                        //Tcyc=4µs
  OSCCON=0b00110010;               //IRCF=%011 (1 MHz) et SCS=%10: horloge interne réglée sur 1 MHz
#endif

#if FCLK==2                        //Tcyc=2µs
  OSCCON=0b01000010;               //IRCF=%100 (2 MHz) et SCS=%10: horloge interne réglée sur 2 MHz
#endif

#if FCLK==4                        //Tcyc=1µs
  OSCCON=0b01010010;               //IRCF=%101 (4 MHz) et SCS=%10: horloge interne réglée sur 4 MHz
#endif

#if FCLK==8                        //Tcyc=500ns
  OSCCON=0b01100010;               //IRCF=%110 (8 MHz) et SCS=%10: horloge interne réglée sur 8 MHz
#endif

#if FCLK==16                       //Tcyc=250ns
  OSCCON=0b01110010;               //IRCF=%111 (16 MHz) et SCS=%10: horloge interne réglée sur 16 MHz
#endif

#if FCLK==20                       //Tcyc=200ns
  OSCCON=0b00000000;               //IRCF=%000 horloge interne réglée sur 31kHz dévalidée
#endif

#if FCLK==32                       //Tcyc=125ns
  OSCCON=0b01100000;               //IRCF=%110 (8 MHz) et SCS=%00: horloge interne réglée sur 8 MHz
  OSCTUNEbits.PLLEN=1;             //active la PLL
#endif

#if FCLK==64                       //Tcyc=62.5ns
  OSCCON=0b01110000;               //IRCF=%111 (16 MHz) et SCS=%00: horloge interne réglée sur 16 MHz
  OSCTUNEbits.PLLEN=1;             //active la PLL
#endif

//  INTCONbits.PEIE=1;               //validation des interruptions périphériques

  STATM=0;


//Initialisation des ports d'entrée/sortie

//RB0 (Reset_R1), RB1 (Set_R1), RB2 (Reset_R2), RB3 (Set_R2) en sortie logique numérique
  TRISBbits.RB0=0; //RB0 en sortie
  TRISBbits.RB1=0; //RB1 en sortie
  TRISBbits.RB2=0; //RB2 en sortie 
  TRISBbits.RB3=0; //RB3 en sortie
  ANSELBbits.ANSB0=0;//désactivation du mode analogique de RB0
  ANSELBbits.ANSB1=0;//désactivation du mode analogique de RB1
  ANSELBbits.ANSB2=0;//désactivation du mode analogique de RB2
  ANSELBbits.ANSB3=0;//désactivation du mode analogique de RB3
  // Configuration des broches I2C
  TRISCbits.RC3 = 1;  // Configurer RC3 (SCL) en entrée
  TRISCbits.RC4 = 1;  // Configurer RC4 (SDA) en entrée
  ANSELCbits.ANSC3=0; //désactivation du mode analogique de RB0
  ANSELCbits.ANSC4=0; //désactivation du mode analogique de RB1

  Reset_R1=0;
  Set_R1=0;
  Reset_R2=0;
  Set_R2=0;

  TEF_bit=0;
  VaON_bit=0;


}
unsigned char envoi[30];
char data[20];
char recu[20];
char capteur[30];
char connexion=0;


void main (void)
{
  InitPic(); // Configuration du PIC
  I2C_Init(); // configuration du module de communication I2C
  InitUart9600(FCLK); // Initialisation de la communication UART à 9600 bauds
  Init_Interruption(); // Configuration des interruptions
  Init_Timer0(); // Configuration du Timer0 pour envoi des données toutes les 5s
  Iinit_ESP(envoi); // Initialisation du module ESP pour la communication WiFi  
  BAUDCONbits.WUE=1; //Active le réveille sur RCIF p.127
  float temperature=0;
  float humidite=0;
  while (1) // Boucle infinie
  {
      if(connexion==0) // Si aucune connexion n'est établie
      {
          SLEEP(); // Met le microcontrôleur en mode veille pour économiser l'énergie
      }
  }
}
// Routine d'interruption de haute priorité
void __interrupt (high_priority) HighISR() {
    char raz; // Variable de réinitialisation
    static unsigned short i=0; // Index du buffer de réception
    char caractere_recu; // Stocke le caractère reçu
    unsigned short etat_relai_1; // État du relais 1
    unsigned short etat_relai_2; // État du relais 2
    
    if (PIR1bits.RCIF == 1) { // Vérifie si une interruption de réception UART est déclenchée
        // Resynchronisation de l'UART (perte du premier octet du au temps de reveille par la reception UART)
        RCSTAbits.CREN = 0;  // Désactive la réception UART
        __delay_ms(1); // Petit délai pour stabiliser la réception
        raz = RCREG1; // Lecture du registre de réception pour vider le buffer
        RCSTAbits.CREN = 1;  // Réactivation de la réception UART

        do {
            caractere_recu = UartReadChar(); // Lire un caractère depuis l'UART
            data[i] = caractere_recu; // Stocker dans le buffer de réception
            i++;
        } while(((caractere_recu != '\n' )|| i <= 2) && (i <= 11));  // Lire jusqu'à '\n' ou atteindre la taille max

        i=0; // Réinitialisation de l?index

        if(strncmp(data+2,"IPD,0,2:", 8) == 0){ // Vérifie si la commande reçue est une commande de mise à jour des relais
            etat_relai_1 = (data[10] - '0'); // Conversion du caractère en valeur numérique (0 ou 1)
            etat_relai_2 = (data[11] - '0');

            if(etat_relai_1){
                R1_ouvert(); // Ouvre le relais 1
            } else {
                R1_fermee(); // Ferme le relais 1
            }

            if(etat_relai_2){
                R2_ouvert(); // Ouvre le relais 2
            } else {
                R2_fermee(); // Ferme le relais 2
            }
        }
        else if(strncmp(data+1,"CONNECT", 7) == 0){// Vérifie si une connexion est établie
            T0CONbits.TMR0ON=1; //timer activé p.154
            TMR0H= 0x67; // Partie haute du registre TMR0 (0x67)
            TMR0L = 0x6A; // Partie basse du registre TMR0 (0x6A)
            INTCONbits.TMR0IF = 0;   // RAZ du flag TMR0IF
            connexion=1;
        }
        else if(strncmp(data+1,"CLOSED", 6) == 0){ // Vérifie si la connexion est fermée
            T0CONbits.TMR0ON = 0; // Désactive le Timer0
            effacer(data); // Efface les données reçues
            connexion = 0; // Marque la connexion comme inactive
        }
    }
    BAUDCONbits.WUE=1; //Active le réveille sur RCIF p.127
}
void __interrupt (low_priority) LowISR() {
    float temperature=0;
    float humidite=0;
    char raz;
    if (INTCONbits.TMR0IF){
        // Réinitialiser le Timer0 pour la prochaine période de comptage
        TMR0H= 0x67; // Partie haute du registre TMR0 (0x67)
        TMR0L = 0x6A; // Partie basse du registre TMR0 (0x6A)
        INTCONbits.TMR0IF = 0;   // RAZ du flag TMR0IF
        //mesure de la température et de l'humidité + envoi
        humidite=SHT21_lecture_humidite(); //mesure humidité 
        temperature=SHT21_lecture_Temperature(); //mesure température
        sprintf(capteur,"%.1f*%.1f",temperature,humidite);
        sprintf(envoi,"AT+CIPSEND=0,9\r\n");
        envoyer_UART(envoi); // Envoie la commande AT pour transmettre les données
        __delay_ms(100); // Attente pour stabiliser l'envoi
        envoyer_UART(capteur); // Envoie les données de température et humidité
    }
}


