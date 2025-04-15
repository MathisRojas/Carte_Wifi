//Projet exemple pour carte évaluation 26K22

//01/10/2023: V1.00 - Initialisation du projet

//Fichiers d'include génériques
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <xc.h>                 //Fichier include générique pour tous les Pics

#include "Uart.h"


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
#define Reset_R1  LATBbits.LB0   //commande ouvrir relai 1
#define Set_R1    LATBbits.LB1 //commande fermee relai 1
#define Reset_R2   LATBbits.LB2   //commande ouvrir relai 2
#define Set_R2 LATBbits.LB3   //commande fermee relai 2



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
void R1_ouvert(void)
{
    Set_R1=1;
    __delay_ms(10);
    Set_R1=0; 
}

void R1_fermee(void)
{
    Reset_R1=1;
    __delay_ms(10);
    Reset_R1=0; 
}
void R2_ouvert(void)
{
    Set_R2=1;
    __delay_ms(10);
    Set_R2=0; 
}

void R2_fermee(void)
{
    Reset_R2=1;
    __delay_ms(10);
    Reset_R2=0; 
}

void envoyer_UART(char* buffer)
{
    for(short i=0; buffer[i]!='\0'; i++)
      {
        UartWriteChar(buffer[i]);
      }
}

void effacer(char *chaine) {
    int i = 0;
    while (chaine[i] != '\0') {
        chaine[i++] = '\0';
    }
}
void Iinit_ESP(char *envoi){
  sprintf(envoi,"ATE0\r\n");//commande pour désactiver l'echo
  envoyer_UART(envoi);//désactiver echo
  __delay_ms(300);
    sprintf(envoi,"AT+CIPMUX=1\r\n");//On autorise plusieurs connexions parallèles
  envoyer_UART(envoi);
  __delay_ms(300);
  sprintf(envoi,"AT+RFPOWER=0\r\n"); // puissance de TX (0 ~ 82, unit:0.25dBm)
  envoyer_UART(envoi);
  __delay_ms(300);
  sprintf(envoi,"AT+CIPSERVER=1,8080\r\n");//(1 : Active le serveur,8080 : PORT)
  envoyer_UART(envoi);
  __delay_ms(300);
  effacer(envoi);
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
  ANSELBbits.ANSB0=0;              //désactivation du mode analogique de RB0
  ANSELBbits.ANSB1=0;              //désactivation du mode analogique de RB1
  ANSELBbits.ANSB2=0;              //désactivation du mode analogique de RB2
  ANSELBbits.ANSB3=0;              //désactivation du mode analogique de RB3

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


void main (void)
{
  InitPic();
  InitUart9600(FCLK);
  Iinit_ESP(envoi);
  R1_ouvert();
  R2_ouvert();
  __delay_ms(1000);
  R1_fermee();
  R2_fermee();
  __delay_ms(1000);
  R1_ouvert();
  R2_ouvert();
  __delay_ms(1000);
  R1_fermee();
  R2_fermee();
  while (1)
  {

  }
}
