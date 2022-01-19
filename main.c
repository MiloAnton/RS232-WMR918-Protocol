// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clib.h>
#include <conio.h>
#include <FOSSIL.H>
#include "dos.h"

// Constantes
#define TASK_STACKSIZE 1024 // WORDS => 2048 Bytes, was 1024
#define TASK1_DEBUG         // debug switch
#define MAX_BUFSIZE 1024
#define MAX_TIMEOUT 4000L
#define TM_PORT_ECHO 7 /*well known echo port*/

// Variables globales et prototypes
void huge task1(void);
void huge task2(void);

struct sockaddr_in addr;
struct sockaddr_in claddr;

unsigned int task1_stack[TASK_STACKSIZE];
int task1ID;

TaskDefBlock task1defblock =
    {
        task1,
        {'T', 'A', 'S', '1'},         // un nom: 4 chars
        &task1_stack[TASK_STACKSIZE], // top stack
        TASK_STACKSIZE * sizeof(int), // size stack
        0,                            // attributes, not supported
        26,                           // priority 3(high) ... 127(low)
        0,                            // time slice (if any), not supported
        0,
        0,
        0,
        0 // mailbox depth, not supported
};

unsigned int task2_stack[TASK_STACKSIZE];
int task2ID;

TaskDefBlock task2defblock =
    {
        task2,
        {'T', 'A', 'S', '2'},         // un nom : 4 chars
        &task2_stack[TASK_STACKSIZE], // top stack
        TASK_STACKSIZE * sizeof(int), // size stack
        0,                            // attributes, not supported
        27,                           // priority 3(high) ... 127(low)
        0,                            // time slice (if any), not supported
        0,
        0,
        0,
        0 // mailbox depth, not supported
};

// Initialiser une variable pour régler l'heure système
TimeDate_Structure td =
    {
        0,  /* seconds  (0-59)   */
        0,  /* minutes  (0-59)   */
        16, /* hours (0-23)      */
        20, /* day      (1-31)   */
        1,  /* month (1-12)      */
        0,  /* year     (0-99)   */
        3,  /* day of week (Mon=1 to Sun=7)     */
        20  /* century */
};

TimeDate_Structure tdget;

int print_semaphoreID; /*semaphoreID*/
long semaphore_timout = 0L;
char print_sem[5] = {'P', 'S', 'E', '1', 0};
int task1run;
int task2run;

/******************************************************************************
* Structure pour strocker les résultats des décodages de trame
******************************************************************************/
struct decVent
{
   short direction, GustVitesse, AverageVitesse, ressenti;
};

struct decPluie
{
   short quantite, totalmm, totalmmYd;
};

struct decHumidite
{
   short temperature, humidite, temprosee;
};

struct decEXTBTH
{
   short temperature, humidite, temprosee;
};

struct Decode
{
   struct decVent ptr1;
   struct decPluie ptr2;
   struct decHumidite ptr3;
   struct decEXTBTH ptr4;
}meteo;

/******************************************************************************
* Fonction d'initialisation du port RS232 pour la réception de trames
* task 1
******************************************************************************/
int initialisationPort()
{
   printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
   if (fossil_init(FOSSIL_EXT) != 0x1954)
   {
      printf("\r\nInit failed\r\n");
      return -1;
   }
   printf("Getting ready to receive data from meteo station\n");
   fossil_setbaud(FOSSIL_EXT, 9600, FOSSIL_PARITY_NO, 8, 1);
   printf("\nBaud successfully set to 9600\n");
   fossil_set_flowcontrol(FOSSIL_EXT, FOSSIL_FLOWCTRL_RTSCTS);
   printf("\nPort set to RTS (ready to send)\n");
   fossil_purge_input(FOSSIL_EXT);
   printf("\nPort configured, input buffer emptied");
   return 0; // Initialisation réussie
}

/******************************************************************************
* Fonction dde décodage d'une trame VENT
* task 1
******************************************************************************/
int decoderVent(char *adresseTrame)
{
   short u, d, c;               // Direction vent
   short u1, d1, c1;            // Gust wind speed
   short u2, d2, c2;            // average wind speed
   short u3, d3;                // Wind chill
   if (adresseTrame[1] == NULL) // Test erreur trame
   {
      return -1;
   }
   printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
   printf("Frame decoding : \n");
   // Direction vent
   u = adresseTrame[1] & 0x0F;
   d = (adresseTrame[1] >> 4) & 0x0F;
   c = adresseTrame[2] & 0x0F;
   meteo.ptr1.direction = c * 100 + d * 10 + u; // Resultat envoyé dans la structure
   // Gust wind speed
   u1 = (adresseTrame[2] >> 4) & 0x0F;
   d1 = adresseTrame[3] & 0x0F;
   c1 = (adresseTrame[3] >> 4) & 0x0F;
   meteo.ptr1.GustVitesse = c1 * 10 + d1 + u1 / 10; // Resultat envoyé dans la structure
   // Average wind speed
   u2 = adresseTrame[4] & 0x0F;
   d2 = (adresseTrame[4] >> 4) & 0x0F;
   c2 = adresseTrame[5] & 0x0F;
   meteo.ptr1.AverageVitesse = c2 * 10 + d2 + u2 / 10; // Resultat envoyé dans la structure
   // Wind chill
   u3 = adresseTrame[6] & 0x0F;
   d3 = (adresseTrame[6] >> 4) & 0x0F;
   meteo.ptr1.ressenti = d3 * 10 + u3; // Resultat envoyé dans la structure
   return 0;
}

/******************************************************************************
* Fonction dde décodage d'une trame PLUIE
* task 1
******************************************************************************/
int decoderPluie(char *adresseTrame)
{
   short u, d, c, r;             // Current Rain Rate
   short i1, u1, d1, c1, m1, r1; // Total rainfall
   short u2, d2, c2, m2, r2;     // Yesterday rainfall
   if (adresseTrame[1] == NULL)  // Test erreur trame
   {
      return -1;
   }
   printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
   printf("Frame decoding : \n");
   // Current Rain Rate
   u = adresseTrame[1] & 0x0F;
   d = (adresseTrame[1] >> 4) & 0x0F;
   c = adresseTrame[2] & 0x0F;
   meteo.ptr2.quantite = c * 100 + d * 10 + u; // Resultat envoyé dans la structure
   // Total rainfall
   i1 = (adresseTrame[2] >> 4) & 0x0F;
   u1 = adresseTrame[3] & 0x0F;
   d1 = (adresseTrame[3] >> 4) & 0x0F;
   c1 = adresseTrame[4] & 0x0F;
   m1 = (adresseTrame[4] >> 4) & 0x0F;
   meteo.ptr2.totalmm = m1 * 1000 + c1 * 100 + d1 * 10 + u1 + i1 / 10; // Resultat envoyé dans la structure
   // Yesterday rainfall
   u2 = adresseTrame[5] & 0x0F;
   d2 = (adresseTrame[5] >> 4) & 0x0F;
   c2 = adresseTrame[6] & 0x0F;
   m2 = (adresseTrame[6] >> 4) & 0x0F;
   meteo.ptr2.totalmmYd = m2 * 1000 + c2 * 100 + d2 * 10 + u2; // Resultat envoyé dans la structure
   return 0;
}

/******************************************************************************
* Fonction dde décodage d'une trame HUMIDITE
* task 1
******************************************************************************/
int decoderHumidite(char *adresseTrame)
{
   short ou;                    // over/under
   short sign;                  // sign to check over/under
   short i, u, d, c;            // Temperature
   short u1, d1;                // Humidité
   short u2, d2;                // Rosée temperature
   if (adresseTrame[2] == NULL) // Test erreur trame
   {
      return -1;
   }
   printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
   printf("Frame decoding : \n");
   ou = (adresseTrame[2] >> 6) & 0x03;
   sign = (adresseTrame[2] >> 7) & 0x01;
   if (ou == 1)
   {
      switch (sign)
      {
      case 0:
         printf("\n(INT)The temperature is TOO HIGH for the sensors !! :(\n");
         return 0;
      case 1:
         printf("\n(INT)The temperature is TOO LOW for the sensors !! :(\n");
         return 0;
      default:
         printf("\n(INT)Could not check temperature value\n");
      }
   }
   // Temperature
   i = adresseTrame[1] & 0x0F;
   u = (adresseTrame[1] >> 4) & 0x0F;
   d = adresseTrame[2] & 0x0F;
   c = (adresseTrame[2] >> 4) & 0x0F;
   meteo.ptr3.temperature = c * 100 + d * 10 + u + i / 10; // Resultat envoyé dans la structure
   // Humidité
   u1 = adresseTrame[3] & 0x0F;
   d1 = (adresseTrame[3] >> 4) & 0x0F;
   meteo.ptr3.humidite = d1 * 10 + u1; // Resultat envoyé dans la structure
   // Rosée temperature
   u2 = adresseTrame[4] & 0x0F;
   d2 = (adresseTrame[4] >> 4) & 0x0F;
   meteo.ptr3.temprosee = d2 * 10 + u2; // Resultat envoyé dans la structure
   return 0;
}

/******************************************************************************
* Fonction dde décodage d'une trame EXTBTH
* task 1
******************************************************************************/
int decoderEXTBTH(char *adresseTrame)
{
   short ou = 0;                // over/under
   short sign;                  // sign to check over/under
   short i, u, d, c;            // Temperature
   short u1, d1;                // Humidité
   short u2, d2;                // Rosée temperature
   if (adresseTrame[1] == NULL) // Test erreur trame
   {
      return -1;
   }
   printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
   printf("Frame decoding : \n");
   ou = (adresseTrame[2] >> 6) & 0x03;
   sign = (adresseTrame[2] >> 7) & 0x01;
   if (ou == 1)
   {
      switch (sign)
      {
      case 0:
         printf("\n(EXT)The temperature is TOO HIGH for the sensors !! :(\n");
         return 0;
      case 1:
         printf("\n(EXT)The temperature is TOO LOW for the sensors !! :(\n");
         return 0;
      default:
         printf("\n(EXT)Could not check temperature value\n");
      }
   }
   // Temperature
   i = adresseTrame[1] & 0x0F;
   u = (adresseTrame[1] >> 4) & 0x0F;
   d = adresseTrame[2] & 0x0F;
   c = (adresseTrame[2] >> 4) & 0x0F;
   meteo.ptr4.temperature = c * 100 + d * 10 + u + i / 10; // Resultat envoyé dans la structure
   // Humidité
   u1 = adresseTrame[3] & 0x0F;
   d1 = (adresseTrame[3] >> 4) & 0x0F;
   meteo.ptr4.humidite = d1 * 10 + u1; // Resultat envoyé dans la structure
   // Rosée temperature
   u2 = adresseTrame[4] & 0x0F;
   d2 = (adresseTrame[4] >> 4) & 0x0F;
   meteo.ptr4.temprosee = d2 * 10 + u2; // Resultat envoyé dans la structure
   return 0;
}

/******************************************************************************
* Fonction d'envoi de la structure
* task 2
******************************************************************************/
int laSend(int sd)
{
   int result; // return des fonctions
   char *bufptr;  // reception TCP
   int error;  // Codes erreur
   do
   {
      result = recv(sd, bufptr, MAX_BUFSIZE, MSG_TIMEOUT, MAX_TIMEOUT, &error);
      if (result == API_ERROR)
      {
         break;
      }
      //Envoyer data au client
      if (result > 0)
      {
         RTX_Sleep_Time(20);
         // Envoi de la structure de structures : (char *)&meteo
         result = send(sd, (char *)&meteo, sizeof(meteo), MSG_BLOCKING, &error);
         if (result == API_ERROR)
         {
            printf("\r\n[TCP Server] Error while sending data. %d", error);
            return -1;
         }
      } //if(result>0) continuer à recevoir
   } while (1);
   return 0;
}

/******************************************************************************
* task1()
* Decodage trame : Identification Trame / Appels fonctions correspondantes
******************************************************************************/
void huge task1(void)
{

   int result;
   int initialisation = initialisationPort();
   int cpt = 0;
   char carac = 0;                                           // Dernier caractere recupéré
   char caracpre = 0;                                        // Caractere précédent
   char trame[50];                                           // Stockage de la trame
   char *adresseTrame = trame;                               // Pointeur de trame
   int ventDec = 0, pluiDec = 0, humiDec = 0, EXTBTHdec = 0; // Retours fonctions
   task1run = 1;

   while (task1run)
   {
      RTX_Sleep_Time(500);      /*sleep*/
      if (initialisation == -1) // Erreur sur port RS232
      {
         printf("\nErreur d'initialisation du port RS232\n");
      }
      else
      {
         // Boucle récupération et tri d'une seule trame pour fonction correspondante :
         printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
         printf("Recherche de FF : \n");
         while (carac != -1)
         {
            carac = fossil_getbyte_wait(FOSSIL_EXT);
         }
         caracpre = carac;
         carac = fossil_getbyte_wait(FOSSIL_EXT);
         if (carac == -1 && caracpre == -1)
         {
            printf("\nDeux FF trouves, identification de la trame :\n");
         }
         else
         { // Si la recherche de trame se bloque entre deux F
            printf("\nErreur récupération de trame, redémmarrez l'application.\n");
            // return 0;
         }
         // Identification
         caracpre = carac;                        // Stockage du caractère précédent
         carac = fossil_getbyte_wait(FOSSIL_EXT); // Récupération d’un caractère suivant
         switch (carac)
         {
         case 0x00:                       // VENT
            for (cpt = 0; cpt < 7; cpt++) // Récupération données de la trame
            {
               carac = fossil_getbyte_wait(FOSSIL_EXT);
               trame[cpt] = carac;
            }
            printf("\n VENT : \n");
            printf("Received frame : \n");
            for (cpt = 0; cpt < 7; cpt++) // Affichage trame récupérée
            {
               printf(" %02X", trame[cpt]);
            }
            ventDec = decoderVent(adresseTrame); // Envoi à la fonction correspondante
            printf("\nWind direction : %d°", meteo.ptr1.direction);
            printf("\nGust wind speed : %d m/sec", meteo.ptr1.GustVitesse);
            printf("\nAverage wind speed : %d m/sec", meteo.ptr1.AverageVitesse);
            printf("\nWind chill : %d°C", meteo.ptr1.ressenti);
            if (ventDec == -1)
            {
               printf("ERROR WHILE DECODING FRAME");
            }
            break;
         case 0x01:                        // PLUIE
            for (cpt = 0; cpt < 12; cpt++) // Récupération données de la trame
            {
               carac = fossil_getbyte_wait(FOSSIL_EXT);
               trame[cpt] = carac;
            }
            printf("\n PLUIE : \n");
            printf("Received frame : \n");
            for (cpt = 0; cpt < 12; cpt++) // Affichage trame récupérée
            {
               printf(" %02X", trame[cpt]);
            }
            pluiDec = decoderPluie(adresseTrame); // Envoi à la fonction correspondante
            printf("\nCurrent rain rate : %d mm/hr", meteo.ptr2.quantite);
            printf("\nTotal rainfall today : %d mm", meteo.ptr2.totalmm);
            printf("\nTotal rainfall yesterday : %d mm", meteo.ptr2.totalmmYd);
            if (pluiDec == -1)
            {
               printf("ERROR WHILE DECODING FRAME");
            }
            break;
         case 0x03:                       // HUMIDITE
            for (cpt = 0; cpt < 5; cpt++) // Récupération données de la trame
            {
               carac = fossil_getbyte_wait(FOSSIL_EXT);
               trame[cpt] = carac;
            }
            printf("\n HUMIDITE : \n");
            printf("Received frame : \n");
            for (cpt = 0; cpt < 5; cpt++) // Affichage trame récupérée
            {
               printf(" %02X", trame[cpt]);
            }
            humiDec = decoderHumidite(adresseTrame); // Envoi à la fonction correspondante
            printf("\nExterior Temperature : %d °C", meteo.ptr3.temperature);
            printf("\nExterior Humidity : %d %%", meteo.ptr3.humidite);
            printf("\nExterior Dew temperature : %d °C", meteo.ptr3.temprosee);
            if (humiDec == -1)
            {
               printf("ERROR WHILE DECODING FRAME");
            }
            break;
         case 0x0E: // CLOCK
            printf("\nClock checked\n");
            break;
         case 0x06:                        // EXTBTH : interieur
            for (cpt = 0; cpt < 10; cpt++) // Récupération données de la trame
            {
               carac = fossil_getbyte_wait(FOSSIL_EXT);
               trame[cpt] = carac;
            }
            printf("\n EXTBTH : \n");
            printf("Received frame : \n");
            for (cpt = 0; cpt < 10; cpt++) // Affichage trame récupérée
            {
               printf(" %02X", trame[cpt]);
            }
            EXTBTHdec = decoderEXTBTH(adresseTrame); // Envoi à la fonction correspondante
            printf("\nInterior temperature : %d °C", meteo.ptr4.temperature);
            printf("\nInterior humidity : %d %%", meteo.ptr4.humidite);
            printf("\nInterior dew temperature : %d °C", meteo.ptr4.temprosee);
            if (EXTBTHdec == -1)
            {
               printf("ERROR WHILE DECODING FRAME");
            }
            break;
         default:
            printf("\nERROR WHILE IDENTIFYING FRAME\n");
         }
      }
      printf("\n¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤\n");
      printf("EOF");
   }
   printf("\r\nKill task1");
   result = RTX_Kill_Task(RTX_Get_TaskID());
   if (result != 0)
   {
      printf(" failed kill task 1, %d  %d\r\n", result, task1ID);
   }
}

/******************************************************************************
* task2()
* SERVEUR
******************************************************************************/
void huge task2(void)
{
   int result;
   int socket;
   int error = 0;
   int port = 10000;
   char ClientIP[17];
   int las = 0;
   task2run = 1;

   while (task2run)
   {
      // Ouvrir un socket TCP
      result = opensocket(SOCK_STREAM, &error);
      if (result == API_ERROR)
      {
         printf("\r\n[TCP server], Open socket failed %d", error);
         break;
      }
      else
      {
         socket = result;
         printf("\r\n[TCP server]: Open listening socket %d", socket);
      }

      // Bind a socket, assigner une adresse
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = 0L;
      result = bind(socket, &addr, &error);
      if (result == API_ERROR)
      {
         printf("\r\n[TCP server], Bind socket failed %d", error);
         break;
      }
      else if (result == 0)
      {
         printf("\r\n[TCP server], Bind socket successfull %d", socket);
      }

      // Main server loop, listen , accept, and send
      while (task2run)
      {
         // listen for a client
         result = listen(socket, 1, &error);
         printf("\r\nListening for a connection\r\n");
         if (result == API_ERROR)
         {
            printf("\r\n[TCP server], listen failed %d", error);
            break;
         }

         // accept and establish a connection
         claddr.sin_family = PF_INET;
         claddr.sin_port = 0;         //clear
         claddr.sin_addr.s_addr = 0L; //clear

         result = accept(socket, &claddr, &error);
         if (result == API_ERROR)
         {
            printf("\r\n[TCP server], accept failed %d", error);
            break;
         }
         InetToAscii(&claddr.sin_addr.s_addr, ClientIP);
         printf("\r\n[TCP server]: Connected with %s , Port %u\r\n", ClientIP, claddr.sin_port);
         las = laSend(result);
         if (las == -1)
         {
            printf("\r\n[TCP server], failed to send data to %s : %d", ClientIP, error);
            break;
         }
      }
   }
   printf("\r\nKill task2");
   result = RTX_Kill_Task(RTX_Get_TaskID());
   if (result != 0)
   {
      printf(" failed kill task 2, %d  %d\r\n", result, task1ID);
   }
}

/******************************************************************************
* main()
******************************************************************************/
int main(void)
{
   int result;
   int key = 0;
   static int firsttime = 1;
   // Passer stdio pour notre app
   BIOS_Set_Focus(FOCUS_APPLICATION);

   printf("\r\nRTOS API example1\r\n");
   RTX_Set_TimeDate(&td);
   RTX_Get_TimeDate(&tdget);

   // Créer un semaphore ressource pour la fonction print_task_msg
   result = RTX_Create_Sem(&print_semaphoreID, print_sem, -1);
   if (result != 0)
   {
      printf("\r\nCreating the semaphore failed %d, exit program\r\n", result);
      return 0;
   }

   // Créer/Redémarrer et démarrer la task1
   if (firsttime == 1)
      result = RTX_Create_Task(&task1ID, &task1defblock);
   else
      result = RTX_Restart_Task(task1ID);
   if (result != 0)
   {
      printf("\r\nCreating/restart TASK_1 failed %d, exit program\r\n", result);
      // Supprimer print_semaphore
      RTX_Delete_Sem(print_semaphoreID);
      return 0;
   }
   // Créer/Redémarrer et démarrer la task2
   if (firsttime == 1)
      result = RTX_Create_Task(&task2ID, &task2defblock);
   else
      result = RTX_Restart_Task(task2ID);

   firsttime = 0;
   if (result != 0)
   {
      printf("\r\nCreating/restart TASK_2 failed %d, exit program\r\n", result);
      // Supprimer task1
      RTX_Delete_Task(task1ID);
      // Supprimer print_semaphore
      RTX_Delete_Sem(print_semaphoreID);
      return 0;
   }

   printf("\r\nPress ESC to exit the program\r\n");
   while (key != 27)
   {
      key = getch();
      RTX_Sleep_Time(1000);
   }

   // Informer task1 and task2 de quitter la boucle
   task1run = task2run = 0;
   RTX_Sleep_Time(3000);
   printf("\r\nDeleting the semaphore ");
   result = RTX_Delete_Sem(print_semaphoreID);
   if (result != 0)
   {
      printf(" failed, %d\r\n", result);
   }
   printf("\r\nExit program\r\n\r\n");
   //Passer stdio en command shell
   BIOS_Set_Focus(FOCUS_BOTH);
   return 0;
}

// End of file