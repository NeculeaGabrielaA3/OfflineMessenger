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
char connected_username[20];

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
    
    printf("<>Creaza o parola: \n");
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
    printf("<>Introduceti username-ul: \n");
    bzero(username, 20);
    read(0, username, 20);

    if(write(sd, username, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        return;
    }
    printf("<>Introduceti parola: \n");
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
            strcpy(connected_username, username);
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
            printf("<>MESAJE NOI PRIMITE OFFLINE!!: %s\n", oldmsg);
        }
    }

}

void trimite_mesaj(int sd)
{
    char receiver_username[20];
    int ok = 0;
    bzero(receiver_username, 20);
    printf("<>Cui vreiti sa trimiti mesajul? Introduceti utilizatorul: \n");
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

        printf("<>Introduceti mesajul pt utilizator: \n");
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
    printf("<>Ce conversatie doriti sa vedeti? Introduceti utilizatorul: \n");
    read(0, user_conv, 20);

    //scriem utilizatorul serverului
    if(write(sd, user_conv, 20) <= 0)
    {
        perror("[client]Eroare la scriere spre server.\n");
        exit(2);
    }

    int ok;
    if(read(sd, &ok, sizeof(ok)) <= 0)
    {
        perror("[client]Eroare citire de la server.\n");
        exit(2);
    }

    user_conv[strlen(user_conv) - 1] = '\0';
    if(ok == 1)
    {
        int size_conversatie;
        if(read(sd, &size_conversatie, sizeof(size_conversatie)) <= 0)
        {
            perror("[client]Eroare citire de la server1.\n");
            exit(2);
        }

        char* conversatie = (char*)malloc(size_conversatie + 1);
        if(read(sd, conversatie, size_conversatie) <= 0)
        {
            perror("[client]Eroare citire de la server2.\n");
            exit(2);
        }

        printf("\n<>Conversatia cu %s: \n%s\n", user_conv, conversatie);
    }
}

void raspunde_mesaj(int sd)
{
    char user_conv[20];
    bzero(user_conv, 20);

    printf("\n<>Introdu username valid: \n");
    read(0, user_conv, 20);

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

    if(ok == 1)
    {
        int rs;
        if(read(sd, &rs, sizeof(rs)) <= 0)
        {
            perror("[client]Eroare citire de la server.\n");
            exit(1);
        }

        //conversatia exista
        if(rs == 100)
        {
            int size_conversatie;
            if(read(sd, &size_conversatie, sizeof(size_conversatie)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
                exit(1);
            }

            char* conversatie = (char*)malloc(size_conversatie + 1);
            if(read(sd, conversatie, size_conversatie) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
                exit(1);
            }

            int nr_maxmsg;

            if(read(sd, &nr_maxmsg, sizeof(nr_maxmsg)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
                exit(1);
            }

            printf("%s\n<>La care mesaj vreti sa raspundeti?\n<>Alegeti numarul de ordine al mesajului: \n", conversatie);
            int nr_mesaj;

            scanf("%d", &nr_mesaj);

            if(write(sd, &nr_mesaj, sizeof(nr_mesaj)) <= 0)
            {
                perror("[client]Eroare la scriere spre server.\n");
                exit(1);
            }

            if(nr_mesaj > nr_maxmsg)
            {
                printf("<>Nu exista mesaj cu numarul %d.\n", nr_mesaj);
                return 0;
            }
            else
            {
                char reply[1024];
                bzero(reply, 1024);
                printf("<>Tastati raspunsul pentru mesajul cu numarul %d: \n", nr_mesaj);
                read(0, reply, 1024);
                int dim_reply = strlen(reply);

                if(write(sd, reply, 1024) <= 0)
                {
                    perror("[client]Eroare citire de la server.\n");
                    exit(2);
                }
            }
        }

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
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);
    
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]Eroare la connect().\n");
        return -1;
    }

    char msg[1024];
    bzero(msg, 1024);
    //cititrea comenzilor
    if (read(sd, msg, 1024) < 0)
    {
        perror ("[client]Eroare la read() de la server.\n");
        return 0;
    }
   
    printf("[client]%s\n", msg);

    char comanda[100];
    bzero(comanda, 100);
    while(read(0, comanda, 100))
    {
        comanda[strlen(comanda) - 1] = '\0';
        //printf("[client]Ai tastat comanda: %s \n", comanda);
        fflush(stdout);
        write(sd, comanda, 100);

        char rasp[100];
        int connected;   
        bzero(rasp, 100);

        if(strcmp(comanda, "exit") == 0)
        {
            close(sd);
            return 0;
        }
        else if(strcmp(comanda, "Creare cont") == 0)
        {
            creare_cont(sd);
        }
        else if(strcmp(comanda, "Log in") == 0)
        {
            if(read(sd, &connected, sizeof(connected)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
                exit(1);
            }
            if(connected == 0)
            {
                log_in(sd);
            }
        }
        else if(strcmp(comanda, "Trimite mesaj catre") == 0)
        {
            if(read(sd, &connected, sizeof(connected)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
                exit(1);
            }
            if(connected == 1)
            {
                trimite_mesaj(sd);
            }
        }
        else if(strcmp(comanda, "Vizualizare conversatie") == 0)
        {
            if(read(sd, &connected, sizeof(connected)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
            }
            if(connected == 1)
            {
                vizualizare_conversatie(sd);
            }
        }
        else if(strcmp(comanda, "Log out") == 0)
        {
            strcpy(connected_username, "");
        }
        else if(strcmp(comanda, "Raspunde la mesaj") == 0)
        {
            if(read(sd, &connected, sizeof(connected)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
            }
            if(connected == 1)
            {
                raspunde_mesaj(sd);
            }
        }
        else if(strcmp(comanda, "Refresh mesaje") == 0)
        {
            if(read(sd, &connected, sizeof(connected)) <= 0)
            {
                perror("[client]Eroare citire de la server.\n");
            }
        }
        else
        {
            int size;
            if(read(sd, &size, sizeof(size)) <= 0)
            {
                perror("[client]Eroare citire de la server");
                exit(1);
            }
            char* lista = (char*)malloc(size + 1);
            if(read(sd, lista, size) <= 0)
            {
                perror("[client]Eroare citire de la server");
                exit(1);     
            }
            printf("%s\n", lista);
        }

        bzero(rasp, 100);
        read(sd, rasp, 100);
        if(strcmp(rasp, "")!=0)
            printf("<>%s\n", rasp);
        fflush(stdout);

        int conn;
        if(read(sd, &conn, sizeof(conn)) <= 0)
        {
            perror("[client]Eroare citire de la server.\n");
            exit(1);
        }
        //printf("%d\n", conn);
        if(conn == 1)
        {
            int dimrec;
            if(read(sd, &dimrec, sizeof(dimrec)) <= 0)
            {
                perror("[client]Eroare ciire de la server!\n");
                exit(1);
            }
            char* recmsg = (char*)malloc(dimrec);
            if(read(sd, recmsg, dimrec) <= 0)
            {
                perror("[client]Eroare citire de la server.");
                exit(1);
            }
            if(strcmp(recmsg, "Nu aveti mesaje noi!") != 0)
                printf("<>Mesaje primit online: %s\n", recmsg);
        } 
        bzero(comanda, 100);
    }
    close (sd);
}