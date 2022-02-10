#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>

#define PORT 2909

extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de progrm
    int cl; // descriptorul intors de accept
}thData;

//structura de date in care pastram cine e conectat
typedef struct conect{
    char username[20];
    int descriptor;
};

struct conect connected_users[100];
int count_connected;

sqlite3 *db;
char *zErrMsg;
int dbproject;

char connected_username[20] = "";

static void *treat(void *); 
void raspunde(int, int);

int validare_username(char username[])
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT username FROM users WHERE username = ?1;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc;
    int ok = 0;
    while( (rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *value = sqlite3_column_text(stmt, 0);
        if(strcmp(value, username) == 0)
        {
            ok = 1;
            break;
        }
    }
    return ok;
}

void mutare_mesaje(char connected_username[])
{
    //cautam mesajele ce erau pt userul conectat
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db, "SELECT sender, msg FROM rmessage WHERE receiver=?1;", -1, &stmt1, NULL);
    sqlite3_bind_text(stmt1, 1, connected_username, -1, SQLITE_STATIC);

    int rc;
    while( (rc = sqlite3_step(stmt1)) == SQLITE_ROW)
    {
        //le mutam in tabela cu mesaj offline
        const char *s = sqlite3_column_text(stmt1, 0);
        const char *mesaj = sqlite3_column_text(stmt1, 1);

        sqlite3_stmt *g;
        sqlite3_prepare_v2(db, "INSERT INTO SMESSAGE VALUES(?1, ?2, ?3);", -1, &g, NULL);
        sqlite3_bind_text(g, 1, s, -1, SQLITE_STATIC);
        sqlite3_bind_text(g, 2, connected_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(g, 3, mesaj, -1, SQLITE_STATIC);
        sqlite3_step(g);
        sqlite3_finalize(g);
    }
    sqlite3_finalize(stmt1);

    //stergem mesajele din tabela online -> utilizatorul nu mai e conectat
    sqlite3_stmt *stmt3;
    sqlite3_prepare_v2(db, "DELETE FROM rmessage WHERE receiver=?1;", -1, &stmt3, NULL);
    sqlite3_bind_text(stmt3, 1, connected_username, -1, SQLITE_STATIC);
    sqlite3_step(stmt3);
    sqlite3_finalize(stmt3);
}

int main()
{
    struct sockaddr_in server;	// structura folosita de server
    struct sockaddr_in from;

    pthread_t th[100]; //Identificatorii thread-urilor care se vor crea
	int i = 0;
    int sd;

    dbproject = sqlite3_open("projectdb.db", &db);

    //create SQL statemnt -> creare tabel
    char *sql1;
    sql1 = "CREATE TABLE USERS(" \
        "USERNAME TEXT PRIMARY KEY NOT NULL, " \
        "PASSWORD TEXT             NOT NULL);";

    //execute SQL statement
    dbproject = sqlite3_exec(db, sql1, NULL, 0, &zErrMsg);

    /* crearea unui socket */
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }

    /* utilizarea optiunii SO_REUSEADDR */
    int on=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

     /* pregatirea structurilor de date */
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));

    server.sin_family = AF_INET;	
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

     /* atasam socketul */
    if(bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if(listen(sd, 2) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }

    /* servim in mod concurent clientii...folosind thread-uri */
    while(1)
    {
        int client;
        thData * td; //parametru functia executata de thread     
        int length = sizeof (from);

        printf ("[server]Asteptam la portul %d...\n",PORT);
        fflush (stdout);

        if( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
	    {
	        perror ("[server]Eroare la accept().\n");
	        continue;
	    }

	    td=(struct thData*)malloc(sizeof(struct thData));	
	    td->idThread=i++;
	    td->cl=client;

	    pthread_create(&th[i], NULL, &treat, td);	      		
	}
}

static void *treat(void * arg)
{
    struct thData tdL; 
    tdL= *((struct thData*)arg);	
    printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush (stdout);		 
    pthread_detach(pthread_self());		
    raspunde(tdL.cl, tdL.idThread);
    /* am terminat cu acest client, inchidem conexiunea */
    close ((intptr_t)arg);
    return(NULL);	
}

void creare_cont2(int cl, int idThread, char rasp[], int* connected)
{
    char username[20];
    char password[30];
    bzero(username, 20);
    bzero(password, 30);

    if(read(cl, username, 20) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        exit(1);
    }
    if(read(cl, password, 30) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        exit(1);
    }

    username[strlen(username) - 1] = '\0';
    password[strlen(password) - 1] = '\0';

    int rc;
    sqlite3_stmt *stmt1;
    sqlite3_prepare_v2(db, "SELECT username, password FROM users WHERE username = ?1;", -1, &stmt1, NULL);
    sqlite3_bind_text(stmt1, 1, username, -1, SQLITE_STATIC);

    if((rc = sqlite3_step(stmt1)) == SQLITE_ROW)
    {
        strcpy(rasp, "Acest utilizator exista deja!");
    }
    else
    {
        printf("Username: %s, Password: %s\n", username, password);
        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2(db, "INSERT INTO USERS VALUES(?1, ?2);", -1, &stmt2, NULL);
        sqlite3_bind_text(stmt2, 1, username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, password, -1, SQLITE_STATIC);
        sqlite3_step(stmt2);

        // //la creare unui cont esti si conectat cu acel cont in aplicatie
        // strcpy(connected_users[count_connected].username, username);
        // connected_users[count_connected++].descriptor = cl;
        // strcpy(connected_username, username);
        // //*connected = 1;

        strcpy(rasp, "Utilizator creat cu succes.");
    }
}

void log_in(int cl, int idThread, char rasp[], int* connected, char connected_username[])
{
    char username[20];
    char password[30];
    bzero(username, 20);
    bzero(password, 30);
    if(read(cl, username, 20) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        return;
    }
    if(read(cl, password, 30) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        return;
    }
    username[strlen(username) - 1] = '\0';
    password[strlen(password) - 1] = '\0';

    //printf("Username: %s, Password: %s\n", username, password); //un print
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT username, password FROM users WHERE username = ?1;", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int rc;
    int ok = 0;
    const char *right_pass;
    while( (rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const char *value = sqlite3_column_text(stmt, 0);
        right_pass = sqlite3_column_text(stmt, 1);
        if(strcmp(value, username) == 0)
        {
            ok = 1;
            break;
        }
    }
    
    if(write(cl, &ok, sizeof(ok)) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        return;
    }

    if(ok == 0)
    {
        strcpy(rasp, "Nu exista cont cu acest username!");
    }
    else
    {
        int verif = 0;
        if(strcmp(right_pass, password) == 0)
            verif = 1;

        if(write(cl, &verif, sizeof(verif)) <= 0)
        {
            perror("[server] Eroare scriere spre client.\n");
            return;
        }

        if(verif)
        {
            strcpy(rasp, "Te-ai conectat cu succes!");
            strcpy(connected_users[count_connected].username, username);
            connected_users[count_connected++].descriptor = cl;
            strcpy(connected_username, username);
            *connected = 1;
            printf("S-a conectat: %s\n", connected_username);

            sqlite3_stmt *msgs;
            sqlite3_prepare_v2(db, "SELECT * FROM smessage WHERE receiver = ?1", -1, &msgs, NULL);
            sqlite3_bind_text(msgs, 1, connected_username, -1, SQLITE_STATIC);

            int rd;
            char *oldmsg = "";
            while( (rd = sqlite3_step(msgs)) == SQLITE_ROW)
            {
                const char *msg = sqlite3_column_text(msgs, 2);
                const char *from = sqlite3_column_text(msgs, 0);
                char* newmsg = (char*)malloc(strlen(oldmsg) + strlen(msg) + strlen(from) + 3);
                strcpy(newmsg, oldmsg);
                strcat(newmsg, "\n");
                strcat(newmsg, from);
                strcat(newmsg, ": ");
                strcat(newmsg, msg);
                oldmsg = (char*)malloc(strlen(newmsg));
                strcpy(oldmsg, newmsg);
            }
            sqlite3_finalize(msgs);

            int dim_oldmsg = strlen(oldmsg);
            if(dim_oldmsg == 0)
            {
                oldmsg = (char*)malloc(strlen("Nu aveti mesaje noi!"));
                strcpy(oldmsg, "Nu aveti mesaje noi!");
                dim_oldmsg = strlen(oldmsg);
            }
            if(write(cl, &dim_oldmsg, sizeof(dim_oldmsg)) <= 0)
            {
                perror("[server]Eroare scriere spre client.");
                exit(2);
            }
            if(write(cl, oldmsg, dim_oldmsg) <= 0)
            {
                perror("[server]Eroare scriere spre client.");
                exit(2);
            }

            sqlite3_stmt *msgs2;
            sqlite3_prepare_v2(db, "DELETE FROM smessage WHERE receiver = ?1", -1, &msgs2, NULL);
            sqlite3_bind_text(msgs2, 1, connected_username, -1, SQLITE_STATIC);
            sqlite3_step(msgs2);
            ///
        }
        else
        {
            strcpy(rasp, "Parola introdusa gresit! Mai incercati!");
        }
    }
    sqlite3_finalize(stmt);
}

void trimite_mesaj(int cl, int idThread, char rasp[], char connected_username[])
{
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "CREATE TABLE SMESSAGE(SENDER TEXT NOT NULL, RECEIVER TEXT NOT NULL, MSG TEXT NOT NULL)", -1, &stmt, NULL);
    sqlite3_step(stmt);

    sqlite3_stmt *stmt8;
    sqlite3_prepare_v2(db, "CREATE TABLE RMESSAGE(SENDER TEXT NOT NULL, RECEIVER TEXT NOT NULL, MSG TEXT NOT NULL)", -1, &stmt8, NULL);
    sqlite3_step(stmt8);

    sqlite3_stmt *stmt3;
    sqlite3_prepare_v2(db, "CREATE TABLE CONVERSATII(USER1 TEXT, USER2 TEXT, COUNT INTEGER, CONV TEXT, primary key(USER1, USER2))", -1, &stmt3, NULL);
    sqlite3_step(stmt3);

    char receiver_username[20];
    bzero(receiver_username, 20);

    if(read(cl, receiver_username, 20) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        exit(2);
    }
  
    receiver_username[strlen(receiver_username) - 1] = '\0';
    int ok;
    ok = validare_username(receiver_username);

    if(write(cl, &ok, sizeof(ok)) <= 0)
    {
        perror("[server]Eroare scriere spre client.\n");
    }

    if(ok == 1)
    {
        int size;
        char mesaj[2024];
        if(read(cl, &size, sizeof(size)) <= 0)
        {
            perror("[server]Eroare citire de la client.\n");
        }
       
        if(read(cl, mesaj, size) <=0 )
        {
            perror("[server]Eroare citire de la client.\n");
        }
        
        int check = 0;
        for(int i = 0; i < count_connected && check == 0; i++)
        {
           if(strcmp(receiver_username, connected_users[i].username) == 0)
           {
                check = 1;
           }
        }
        //user-ul caruia vrem sa trimit mesajul nu e conectat
        if(check == 0)
        {
            sqlite3_stmt *stmt2;
        
            printf("%s to %s mesajul: %s\n", connected_username, receiver_username, mesaj);
            sqlite3_prepare_v2(db, "INSERT INTO SMESSAGE VALUES(?1, ?2, ?3);", -1, &stmt2, NULL);
            sqlite3_bind_text(stmt2, 1, connected_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt2, 2, receiver_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt2, 3, mesaj, -1, SQLITE_STATIC);
            sqlite3_step(stmt2);
        }
        else
        {
            //daca userul caruia vrem sa trimitem mesajul e conectat?
            //ar trebui pus in alt tabel -> RMESSAGE
            sqlite3_stmt *stmt9;
            sqlite3_prepare_v2(db, "INSERT INTO RMESSAGE VALUES(?1, ?2, ?3);", -1, &stmt9, NULL);
            sqlite3_bind_text(stmt9, 1, connected_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt9, 2, receiver_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt9, 3, mesaj, -1, SQLITE_STATIC);
            sqlite3_step(stmt9);
        }

        strcpy(rasp, "Mesaj transmis cu succes!");

        //adaugarea in tabela de conversatii a mesajului nou
        sqlite3_stmt *stmt4;
        sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt4, NULL);
        sqlite3_bind_text(stmt4, 1, connected_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt4, 2, receiver_username, -1, SQLITE_STATIC);

        int rc;
        if( (rc = sqlite3_step(stmt4)) == SQLITE_ROW)
        {
            //aici intra daca conversatia dintre cei doi exista deja
            char nr2[10];
            char* number;
            char* conv = (char*)sqlite3_column_text(stmt4, 0);
            int nr = sqlite3_column_int(stmt4, 1);

            //numarul de ordine al mesajului ce-l va preceda
            nr++;

            sprintf(nr2, "%d", nr);
            number = (char*)malloc(strlen(nr2 + 1));
            strcpy(number, nr2);

            char* newmesaj = (char*)malloc(strlen(connected_username) + strlen(mesaj) + strlen(number) + 2);
            strcpy(newmesaj, number);
            strcat(newmesaj, connected_username);
            strcat(newmesaj, ": ");
            strcat(newmesaj, mesaj);

            char* newconv = (char*)malloc(strlen(conv) + strlen(newmesaj) + 2);
            strcpy(newconv, conv);
            strcat(newconv, "\n");
            strcat(newconv, newmesaj);
            //printf("Noul mesaj de pus in CONVERSATII: %s\n", newconv);

            sqlite3_stmt *stmt5;
            sqlite3_prepare_v2(db, "UPDATE CONVERSATII SET CONV=?1, COUNT=?2 WHERE (USER1=?3 AND USER2=?4) OR (USER1=?4 AND USER2=?3);", -1, &stmt5, NULL);
            sqlite3_bind_text(stmt5, 1, newconv, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt5, 2, nr);
            sqlite3_bind_text(stmt5, 3, connected_username, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt5, 4, receiver_username, -1, SQLITE_STATIC);
            sqlite3_step(stmt5);
        }
        else
        {
            //aici intra daca mesajul e primului din conversatie, a.k.a cei doi nu au alte mesaje
            int nr = 1;
            sqlite3_stmt *stmt5;
            sqlite3_prepare_v2(db, "INSERT INTO CONVERSATII VALUES(?1, ?2, ?3, ?4);", -1, &stmt5, NULL);
            char *number = "1";
            char* newmesaj = (char*)malloc(strlen(mesaj) + strlen(connected_username) + strlen(number) + 2);
            strcpy(newmesaj, number);
            strcat(newmesaj, connected_username);
            strcat(newmesaj, ": ");
            strcat(newmesaj, mesaj);

            sqlite3_bind_text(stmt5, 1, connected_username, -1,SQLITE_STATIC);
            sqlite3_bind_text(stmt5, 2, receiver_username, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt5, 3, nr);
            sqlite3_bind_text(stmt5, 4, newmesaj, -1, SQLITE_STATIC);
            sqlite3_step(stmt5);
        }
    }
    else{
        strcpy(rasp, "Username-ul NU exista.");
    }
}

void vizualizare_conversatie(int cl, char rasp[], char connected_username[])
{
    char user_conv[20];
    bzero(user_conv, 20);

    if(read(cl, user_conv, 20) <= 0)
    {
        perror("[server] Eroare citire username de la client.\n");
        exit(2);
    }

    user_conv[strlen(user_conv) - 1] = '\0';
    int ok;
    ok = validare_username(user_conv);

    if(write(cl, &ok, sizeof(ok)) <= 0)
    {
        perror("[server]Eroare scriere spre client.\n");
        exit(2);
    }

    int rc;
    //daca usernamul exista, cautam conversatia
    if(ok == 1)
    {
        sqlite3_stmt *stmt2;
        sqlite3_prepare_v2(db, "SELECT conv FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt2, NULL);
        sqlite3_bind_text(stmt2, 1, connected_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt2, 2, user_conv, -1, SQLITE_STATIC);

        if( (rc = sqlite3_step(stmt2)) == SQLITE_ROW)
        {
            char* conv = (char*)sqlite3_column_text(stmt2, 0);
            //printf("%s de dim: %ld\n", conv, strlen(conv));

            char* conversatie = (char*)malloc(strlen(conv) + 1);
            strcpy(conversatie, conv);

            int size_conversatie = strlen(conv);

            if(write(cl, &size_conversatie, sizeof(size_conversatie)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }

            if(write(cl, conversatie, size_conversatie) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }
        }
        else
        {
            int size_conversatie = strlen("Nu exista conversatie cu user-ul introdus!");
            
            char* conversatie = (char*)malloc(size_conversatie);
            strcpy(conversatie, "Nu exista conversatie cu user-ul introdus!");

            if(write(cl, &size_conversatie, sizeof(size_conversatie)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }

            if(write(cl, conversatie, size_conversatie) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }
        }
        strcpy(rasp, "Ai primit conversatia!");
    }
    else
    {
        strcpy(rasp, "Username-ul introdus nu exista!");  
    }
}

void raspunde_mesaj(int cl, int idThread, char rasp[], int* connected, char connected_username[])
{    
    char user_conv[20];
    bzero(user_conv, 20);

    if(read(cl, user_conv, 20) <= 0)
    {
        perror("[server]Eroare citire username de la client.\n");
        exit(2);
    }

    user_conv[strlen(user_conv) - 1] = '\0';
    int ok = validare_username(user_conv);

    if(write(cl, &ok, sizeof(ok)) <= 0)
    {
        perror("[server]Eroare scriere spre client.\n");
        exit(2);
    }

    if(ok == 1) //daca username-ul exista
    {
        int rs;
        sqlite3_stmt *conv;
        sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &conv, NULL);
        sqlite3_bind_text(conv, 1, connected_username, -1, SQLITE_STATIC);
        sqlite3_bind_text(conv, 2, user_conv, -1, SQLITE_STATIC);

        rs = sqlite3_step(conv);

        if(write(cl, &rs, sizeof(rs)) <= 0)
        {
            perror("[server]Eroare scriere spre client.\n");
            exit(2);
        }

        if( rs == SQLITE_ROW ) //exista conversatia dintre cei doi useri, SQLITE_ROW=100
        {    
            char* mesaje = (char*)sqlite3_column_text(conv, 0);
            int nr_maxmsg = sqlite3_column_int(conv, 1);
            //printf("%s de dim: %ld\n", mesaje, strlen(mesaje));

            char* conversatie = (char*)malloc(strlen(mesaje) + 1);
            strcpy(conversatie, mesaje);

            int size_conversatie = strlen(mesaje);

            if(write(cl, &size_conversatie, sizeof(size_conversatie)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }

            if(write(cl, conversatie, size_conversatie) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }

            if(write(cl, &nr_maxmsg, sizeof(nr_maxmsg)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(2);
            }

            int nr_mesaj;
            if(read(cl, &nr_mesaj, sizeof(nr_mesaj)) <= 0)
            {
                perror("[server]Eroare citire de la client.\n");
                exit(2);
            }
            //printf("%d\n", nr_mesaj);

            if(nr_mesaj <= nr_maxmsg)
            {
                char reply[1024];
                bzero(reply, 1024);
                if(read(cl, reply, 1024) <= 0)
                {
                    perror("[server]Eroare citire de la client.\n");
                    exit(2);   
                }
                reply[strlen(reply) - 1] = '\0';
                printf("%s\n", reply);

                int check = 0;
                for(int i = 0; i < count_connected && check == 0; i++)
                {
                    if(strcmp(user_conv, connected_users[i].username) == 0)
                    {
                        check = 1;
                    }
                }

                char* number;
                char nr2[10];
                sprintf(nr2, "%d", nr_mesaj);
                number = (char*)malloc(strlen(nr2));
                strcpy(number, nr2);

                char* mesaj = malloc(strlen(reply) + strlen(number) + strlen("Raspuns la mesajul cu nr. ") + 2);
                strcpy(mesaj, "Raspuns la mesajul cu nr. ");
                strcat(mesaj, number);
                strcat(mesaj, ": ");
                strcat(mesaj, reply);

                //utilizatorul caruia ii trimitem mesajul nu e conectat -> SMESSAGE
                if(check == 0)
                {
                    sqlite3_stmt *stmt2;
                
                    sqlite3_prepare_v2(db, "INSERT INTO SMESSAGE VALUES(?1, ?2, ?3);", -1, &stmt2, NULL);
                    sqlite3_bind_text(stmt2, 1, connected_username, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt2, 2, user_conv, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt2, 3, mesaj, -1, SQLITE_STATIC);
                    sqlite3_step(stmt2);
                }
                else
                {
                    //daca userul caruia vrem sa trimitem mesajul e conectat?
                    //ar trebui pus in alt tabel -> RMESSAGE
                    sqlite3_stmt *stmt9;
                    sqlite3_prepare_v2(db, "INSERT INTO RMESSAGE VALUES(?1, ?2, ?3);", -1, &stmt9, NULL);
                    sqlite3_bind_text(stmt9, 1, connected_username, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt9, 2, user_conv, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt9, 3, mesaj, -1, SQLITE_STATIC);
                    sqlite3_step(stmt9);
                }

                /////
                sqlite3_stmt *stmt4;
                sqlite3_prepare_v2(db, "SELECT conv, count FROM CONVERSATII WHERE (USER1=?1 AND USER2=?2) OR (USER1=?2 AND USER2=?1);", -1, &stmt4, NULL);
                sqlite3_bind_text(stmt4, 1, connected_username, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt4, 2, user_conv, -1, SQLITE_STATIC);

                int rc;
                if( (rc = sqlite3_step(stmt4)) == SQLITE_ROW)
                {
                    char nr2[10];
                    char* number;
                    char* conv = (char*)sqlite3_column_text(stmt4, 0);
                    int nr = sqlite3_column_int(stmt4, 1);

                    nr++;

                    sprintf(nr2, "%d", nr);
                    number = (char*)malloc(strlen(nr2 + 1));
                    strcpy(number, nr2);

                    char* newmesaj = (char*)malloc(strlen(connected_username) + strlen(mesaj) + strlen(number) + 2);
                    strcpy(newmesaj, number);
                    strcat(newmesaj, connected_username);
                    strcat(newmesaj, ": ");
                    strcat(newmesaj, mesaj);

                    char* newconv = (char*)malloc(strlen(conv) + strlen(newmesaj) + 2);
                    strcpy(newconv, conv);
                    strcat(newconv, "\n");
                    strcat(newconv, newmesaj);
                    printf("Noul mesaj de pus in CONVERSATII: %s\n", newconv);

                    sqlite3_stmt *stmt5;
                    sqlite3_prepare_v2(db, "UPDATE CONVERSATII SET CONV=?1, COUNT=?2 WHERE (USER1=?3 AND USER2=?4) OR (USER1=?4 AND USER2=?3);", -1, &stmt5, NULL);
                    sqlite3_bind_text(stmt5, 1, newconv, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt5, 2, nr);
                    sqlite3_bind_text(stmt5, 3, connected_username, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt5, 4, user_conv, -1, SQLITE_STATIC);
                    sqlite3_step(stmt5);
                }
            }
        }
        else
        {
            strcpy(rasp, "Nu aveti inca conversatie cu utilizatorul ");
            strcat(rasp, user_conv);
        }
    }
    else
    {
        strcpy(rasp, "Username-ul introdus nu exista!");
    }

}

void raspunde(int cl, int idThread)
{
    char lista_comenzi[1024] = "Alege una dintre urmatoarele: \n" \
            "1. Log in \n" \
            "2. Log out \n" \
            "3. Creare cont \n" \
            "4. Trimite mesaj catre\n" \
            "5. Raspunde la mesaj...\n" \
            "6. Vizualizare conversatie \n" \
            "7. Refresh mesaje\n";

    //scriere comenzilor spre client
    if(write(cl, lista_comenzi, 1024) <= 0)
    {
        printf("[Thread %d] ", idThread);
        perror("[Thread]Eroare la write() catre client.\n");
    }	
    else
        printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", idThread);

    char comanda[100];
    char connected_username[20] = "";
    int connected = 0;
    bzero(comanda, 100);

    while(read(cl, comanda, 100))
    { 
        char rasp[100];  
        bzero(rasp, 100);  
        printf("[Thread %d] %s\n", idThread, comanda);;

        if(strcmp(comanda, "Log in") == 0)
        {
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(1);
            }
            if(connected == 0)
                log_in(cl, idThread, rasp, &connected, connected_username);
            else
            {
                strcpy(rasp, "Utilizatorul ");
                strcat(rasp, connected_username);
                strcat(rasp, " este conectat deja!");
            }
        }
        else if(strcmp(comanda, "Creare cont") == 0)
        {
            creare_cont2(cl, idThread, rasp, &connected);
        }
        else if(strcmp(comanda, "Trimite mesaj catre") == 0)
        {   
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
                exit(1);
            }
            if(connected == 0)
            {
                strcpy(rasp, "Nu esti conectat, nu poti folosi comanda.");
            }
            else
            {
                trimite_mesaj(cl, idThread, rasp, connected_username);
            }
        }
        else if(strcmp(comanda, "Vizualizare conversatie") == 0)
        {
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
            }
            if(connected == 0)
            {
                strcpy(rasp, "Nu esti conectat, nu poti folosi comanda.");
            }
            else
            {
                vizualizare_conversatie(cl, rasp, connected_username);
            }
        }
        else if(strcmp(comanda, "exit") == 0)
        {
            int poz = -1;
            for(int i = 0; i < count_connected; i++)
            {
                if(strcmp(connected_users[i].username, connected_username) == 0)
                {
                    poz = i;
                    break;
                }
            }
            if(poz != -1)
            {
                mutare_mesaje(connected_username);
                for(int i = poz; i < count_connected - 1; i++)
                {
                    strcpy(connected_users[i].username, connected_users[i + 1].username);
                    connected_users[i].descriptor = connected_users[i + 1].descriptor;
                }
                count_connected--;
            }
            strcpy(connected_username, " ");
            connected = 0;
        }
        else if(strcmp(comanda, "Log out") == 0)
        {
            if(connected == 1)
            {
                mutare_mesaje(connected_username);
                printf("S-A DECONECTAT %s\n", connected_username);
                int poz = -1;
                for(int i = 0; i < count_connected; i++)
                {
                    //printf("User %d: %s\n", i, connected_users[i].username);
                    if(strcmp(connected_users[i].username, connected_username) == 0)
                    {
                        poz = i;
                        break;
                    }
                }
                if(poz != -1)
                {
                    
                    for(int i = poz; i < count_connected - 1; i++)
                    {
                        strcpy(connected_users[i].username, connected_users[i + 1].username);
                        connected_users[i].descriptor = connected_users[i + 1].descriptor;
                    }
                    count_connected--;
                }
                strcpy(connected_username, " ");
                strcpy(rasp, "Te-ai deconectat!");
                connected = 0;
            }
            else
            {
                strcpy(rasp, "Nu e nimeni conectat!");
            }
            //toate mesajele din rmessage ar trb trecute in smessage??
        }
        else if(strcmp(comanda, "Raspunde la mesaj") == 0)
        {
            //strcpy(rasp, "Ai raspuns la un mesaj anume.");
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
            }
            if(connected == 0)
            {
                strcpy(rasp, "Nu esti conectat, nu poti folosi comanda.");
            }
            else
            {
                raspunde_mesaj(cl, idThread, rasp, &connected, connected_username);
            }
        }
        else if(strcmp(comanda, "Refresh mesaje") == 0)
        {
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere spre client.\n");
            }
            if(connected == 0)
            {
                strcpy(rasp, "Nu esti conectat, nu poti folosi comanda.");
            }
            else
            {
                strcpy(rasp, "Ai dai refresh! Poti vedea noile mesaje primite online!");
            }
        }
        else
        {
            strcpy(rasp, "Nu exista aceasta comanda! Alege una de mai sus!");
            int size = strlen(lista_comenzi);
            if(write(cl, &size, sizeof(size)) <= 0)
            {
                perror("[server]Eroare scriere catre client!\n");
                exit(1);
            }
            if(write(cl, lista_comenzi, size) <= 0)
            {
                perror("[server]Eroare scriere catre client!\n");
                exit(1);
            }
        }

        if(write(cl, rasp, 100) <= 0)
        {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
        }
        
        //verificam daca nu cumva are mesaje noi
        if(strcmp(comanda, "exit") != 0)
        {
            //printf("%d %s\n", connected, connected_username);
            int conn = 2;
            if(write(cl, &connected, sizeof(connected)) <= 0)
            {
                perror("[server]Eroare scriere catre client!\n");
                exit(1);
            }
            if(connected == 1)
            {
                sqlite3_stmt *rec1;
                sqlite3_prepare_v2(db, "SELECT * FROM rmessage WHERE receiver = ?1", -1, &rec1, NULL);
                sqlite3_bind_text(rec1, 1, connected_username, -1, SQLITE_STATIC);

                int rd;
                char *oldmsg = "";
                while( (rd = sqlite3_step(rec1)) == SQLITE_ROW)
                {
                    const char *msg = sqlite3_column_text(rec1, 2);
                    const char *from = sqlite3_column_text(rec1, 0);
                    char* newmsg = (char*)malloc(strlen(oldmsg) + strlen(msg) + strlen(from) + 3);
                    strcpy(newmsg, oldmsg);
                    strcat(newmsg, "\n");
                    strcat(newmsg, from);
                    strcat(newmsg, ": ");
                    strcat(newmsg, msg);
                    oldmsg = (char*)malloc(strlen(newmsg));
                    strcpy(oldmsg, newmsg);
                }
                sqlite3_finalize(rec1);

                int dim_oldmsg = strlen(oldmsg);
                if(dim_oldmsg == 0)
                {
                    oldmsg = (char*)malloc(strlen("Nu aveti mesaje noi!"));
                    strcpy(oldmsg, "Nu aveti mesaje noi!");
                    dim_oldmsg = strlen(oldmsg);
                }

                if(write(cl, &dim_oldmsg, sizeof(dim_oldmsg)) <= 0)
                {
                    perror("[server]Eroare scriere spre client.");
                    exit(2);
                }
                if(write(cl, oldmsg, dim_oldmsg) <= 0)
                {
                    perror("[server]Eroare scriere spre client.");
                    exit(2);
                }

                sqlite3_stmt *rec2;
                sqlite3_prepare_v2(db, "DELETE FROM rmessage WHERE receiver = ?1", -1, &rec2, NULL);
                sqlite3_bind_text(rec2, 1, connected_username, -1, SQLITE_STATIC);
                sqlite3_step(rec2);
            }
        }
        bzero(comanda, 100);
    }
}
