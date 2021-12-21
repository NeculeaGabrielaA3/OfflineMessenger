#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

void creare_cont(int sd)
{
    char username[20], password[30];
    printf("Creaza username-ul dorit: \n");
    bzero(username, 20);
    read(0, username, 20);

    if(write(sd, username, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }
    printf("Creaza o parola: \n");
    bzero(password, 30);
    read(0, password, 30);

    if(write(sd, password, 30) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }
}

void log_in(int sd)
{
    char username[20], password[30];
    printf("Introduce-ti username-ul: \n");
    bzero(username, 20);
    read(0, username, 20);

    if(write(sd, username, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }
    printf("Introduce-ti parola: \n");
    bzero(password, 30);
    read(0, password, 30);

    if(write(sd, password, 30) <= 0)
    {
        perror("[client]Eroare la scriere spre client.\n");
        return;
    }

    int ok;
    if(read(sd, &ok, sizeof(ok)) <= 0)
    {
        perror("[client]Eroare citire de la server.");
        exit(1);
    }

    if(ok)
    {
        int verif;
        if(read(sd, &verif, sizeof(verif)) <= 0)
        {
            perror("[client]Eroare citire de la server.");
            exit(1);
        }

        if(verif)
        {
            int dim_msg;
            if(read(sd, &dim_msg, sizeof(dim_msg)) <= 0)
            {
                perror("[client]Eroare citire de la server.");
                exit(1);
            }

            char* oldmsg = (char*)malloc(dim_msg);
            if(read(sd, oldmsg, dim_msg) <= 0)
            {
                perror("[client]Eroare citire de la server.");
                exit(1);
            }
            printf("MESAJE NOI!! :%s\n", oldmsg);
        }
    }

}

void trimite_mesaj(int sd)
{
    char receiver_username[20];
    int ok = 0;
    bzero(receiver_username, 20);
    printf("Cui vreti sa trimiteti mesajul? Introduceti utilizatorul: \n");
    read(0, receiver_username, 20);

    if(write(sd, receiver_username, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }

    if(read(sd, &ok, sizeof(ok)) <= 0)
    {
        perror("[client]Eroare citire de la server.\n");
        return;
    }
    
    if(ok == 1)
    {
        char mesaj[2024];
        bzero(mesaj, 2024);

        printf("Introduce-ti mesajul pt utilziator: \n");
        read(0, mesaj, 2024);

        int size = strlen(mesaj);
        mesaj[strlen(mesaj) - 1] = '\0';
        
        if(write(sd, &size, sizeof(size)) <= 0)
        {
            perror("[client]Eroare la sciere spre server.\n");
            return;
        }
        if(write(sd, mesaj, size) <= 0)
        {
            perror("[client]Eroare la scriere spre server.\n");
            return;
        }
    }
}

void vizualizare_conversatie(int sd)
{
    char user_conv[20];
    bzero(user_conv, 20);
    printf("Conversaria cu cine vrei sa vezi? Introdu utilizatorul: \n");
    read(0, user_conv, 20);

    //scriem utilizatorul serverului
    if(write(sd, user_conv, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }

    int ok;
    if(read(sd, &ok, sizeof(ok)) <= 0)
    {
        perror("[client]Eroare citire de la server.\n");
        return;
    }

    user_conv[strlen(user_conv) - 1] = '\0';
    if(ok == 1)
    {
        int size_conversatie;
        if(read(sd, &size_conversatie, sizeof(size_conversatie)) <= 0)
        {
            perror("[client]Eroare citire de la server1.\n");
            exit(1);
        }

        char* conversatie = (char*)malloc(size_conversatie + 1);
        if(read(sd, conversatie, size_conversatie) <= 0)
        {
            perror("[client]Eroare citire de la server2.\n");
        }

        printf("\nConversatia cu %s: \n%s\n", user_conv, conversatie);
    }
}

int main (int argc, char *argv[])
{
    int sd;		
    struct sockaddr_in server;

    int nr=0;
    char buf[10];

    if (argc != 3)
    {
        printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi (argv[2]);

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Eroare la socket().\n");
        return;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);
    
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]Eroare la connect().\n");
        return;
    }

    char msg[1024];
    bzero(msg, 1024);
    //cititrea comenzilor
    if (read(sd, msg, 1024) < 0)
    {
        perror ("[client]Eroare la read() de la server.\n");
        return;
    }
   
    printf("[client]%s\n", msg);

    char comanda[100];
    bzero(comanda, 100);
    while(read(0, comanda, 100))
    {
        printf("[client]Ai tastat comanda: %s \n", comanda);
        fflush(stdout);
        comanda[strlen(comanda) - 1] = '\0';

        write(sd, comanda, 100);

        if(strcmp(comanda, "exit") == 0)
        {
            close(sd);
            return 0;
        }

        char rasp[100];
        int connected;   
        bzero(rasp, 100);
    
        if(strcmp(comanda, "Creare cont") == 0)
        {
            creare_cont(sd);
        }
        else if(strcmp(comanda, "Log in") == 0)
        {
            log_in(sd);
        }
        else if(strcmp(comanda, "Trimite mesaj catre") == 0)
        {
            if(read(sd, &connected, 1) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
            }
            if(connected == 1)
            {
                trimite_mesaj(sd);
            }
        }
        else if(strcmp(comanda, "Vizualizare conversatie") == 0)
        {
            if(read(sd, &connected, 1) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
            }
            if(connected == 1)
            {
                vizualizare_conversatie(sd);
            }
        }

        read(sd, rasp, 100);
        printf("Raspunsul este: %s\n", rasp);
        fflush(stdout);

        bzero(comanda, 100); 
    }
    close (sd);
}