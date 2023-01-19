/* IMPORTANT LINKS

    For SQLite Databases: https://www.includehelp.com/c-programs/get-records-from-a-database-table-in-sqlite.aspx
    For Synchronization: https://github.com/angrave/SystemProgramming/wiki
        (especially: https://github.com/angrave/SystemProgramming/wiki/Synchronization%2C-Part-6%3A-Implementing-a-barrier)
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <sqlite3.h> 
#include <time.h>
#include <stdbool.h>

//portul folosit
#define PORT 3456

//codul de eroare returnat la anumite apeluri
extern int errno;
//linia corespunzatoare din baza de date
int ID;
int jucatoriInregistrati,jucatoriFinal,jucatoriExit;
char clasament[2000];

int nrTh;
int check;
int sesiune;
int onGoing;
int nrTh2;

//variabile utilizate pentru bariere
pthread_mutex_t m;
pthread_cond_t cv;

typedef struct thData
{
    int idThread; //id-ul thread-ului tinut in evidenta de acest program
    int cl; //descriptorul intors de accept
}thData;

struct player
{
    char nickname[100];
    int points;
    struct thData tdL;
    bool isLogged;
    bool isExit;
    int session;
}player[1000];

struct rank
{
    char nickname[100];
    int points;
    bool isLogged;
    bool isExit;
    int session;
};

//functii utilizate in aceasta aplicatie
static void *treat(void *);//functia executata de fiecare thread
void inregistrareClient(void *); //functia prin care primim nickname-ul de la client
void Joc(void *,int,int,int); //functia unde are loc logica jocului
void raspunde(int,void *); //functia prin care se trimit intrebarile+variantele de raspuns catre client
void getQuestion(int,char*); //functia prin care extragem intrebarile+variantele de raspuns din baza de date 
void checkAnswer(int,void *); //functia prin care primim raspunsul de la client si il verificam
void score(void *); //functie de calcul pentru scor
void rank(int,int); //functie de calcul al clasamentului
void sendRank(void *,int); //functie prin care trimitem clasamentul fiecarui client
void deleteInfo(void *,int,int);



int main()
{
    struct sockaddr_in server; //structura folosita de server
    struct sockaddr_in from;
    int sd; //descriptorul de socket
    pthread_t th[100]; //identificatorii thread-urilor care se vor crea
    int i=0;

    //crearea unui socket
    if((sd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        perror("[server]Eroare la socket!\n");
        return errno;
    }
    //utilizarea optiunii SO_REUSEADDR
    int on=1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

    //pregatrirea stucturilor de date
    bzero(&server,sizeof(server));
    bzero(&from,sizeof(from));

    //umplem structura folosita de server
    server.sin_family=AF_INET;//stabilirea familiei de scoket-uri
    server.sin_addr.s_addr=htonl(INADDR_ANY);//acceptam orice adresa
    server.sin_port=htons(PORT);//utilizam un port utilizator

    //atasam socket-ul
    if(bind(sd,(struct sockaddr *) &server,sizeof(struct sockaddr))==-1)
    {
        perror("[server]Eroare la bind!\n");
        return errno;
    }

    //punem serverul sa asculte daca vin clienti sa se conecteze
    if(listen(sd,2)==-1)
    {
        perror("[server]Eroare la listen!\n");
        return errno;
    }

    //servim in mod CONCURENT clientii folosind thread-uri
    int client;
    thData * td;//parametru functia executat de thread
    int lenght=sizeof(from);
    while(1)
    {
        pthread_mutex_init(&m,NULL);
        pthread_cond_init(&cv,NULL);

        printf("[server]Asteptam la portul %d\n",PORT);
        fflush(stdout);

        //acceptam un client (stare blocanta pana la realizarea conexiunii)
        if((client=accept(sd,(struct sockaddr *) &from,&lenght))<0)
        {
            perror("[server]Eroare la accept!\n");
            continue; //***se reintoarce in starea de listen
        }

        //s-a realizat conexiunea si se asteapta mesajul
        td=(struct thData*)malloc(sizeof(struct thData));
        td->idThread=i++;
        td->cl=client;
        
        if(pthread_create(&th[i],NULL,&treat,td)!=0)
        {
            perror("pthread_create error!\n");
        }
        else
        {
            nrTh++;
        }

        pthread_detach(th[i]);
    }
}

static void *treat(void *arg)
{
    struct thData tdL;
    tdL=*((struct thData*)arg);
    
    printf("[thread]-%d - Asteptam mesajul\n",tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());
    inregistrareClient((struct thData*)arg);

    while(true)
    {
        if(player[tdL.idThread].isExit==true)
        {
            nrTh--;
            pthread_testcancel();
            jucatoriExit++;check=0;
            printf("A player disconnected!\n");
            pthread_exit(NULL);
        }
        
        time_t time1,time2;
        time(&time1);
        for(int sec=1;sec<=10;sec++)
            sleep(1);    
        time(&time2);

        if(difftime(time2,time1)==10)
        {
            pthread_mutex_lock(&m);
            nrTh--;
            if(nrTh==0)
            {
                pthread_cond_broadcast(&cv);
            }
            else
            {
                while(nrTh!=0)
                {
                    pthread_cond_wait(&cv,&m);
                }
            }
            pthread_mutex_unlock(&m);
        }
        
        if(jucatoriInregistrati!=0)
        {
            jucatoriInregistrati=jucatoriInregistrati-nrTh2;
            nrTh2=0;
        }

        printf("GAME BEGINS!\n");
        Joc(arg,sesiune,jucatoriInregistrati,jucatoriFinal);
        printf("GAME ENDS!\n\n\n");
    }
}

void inregistrareClient(void *arg)
{
    int nr,len;
    char buf[1024];
    struct thData tdL;
    tdL=*((struct thData*)arg);
    if((nr=(read(tdL.cl,&len,sizeof(len))))<0)
    {
        printf("[Thread %d]\n",tdL.idThread);
        perror("Eroare la read de la client!\n");
    }
    else if(nr==0)
    {
        pthread_cancel(player[tdL.idThread].tdL.cl);
    }
    else
    {
        if(read(tdL.cl,buf,len)<=0)
        {
            printf("[Thread %d]\n",tdL.idThread);
            perror("Eroare la read de la client!\n");
        }
        buf[len]='\0';
        printf("[Thread %d]Mesajul a fost receptionat: '%s'\n",tdL.idThread,buf);

        //initializarea structurii player
        strcpy(player[tdL.idThread].nickname,buf);
        player[tdL.idThread].isLogged=true;
        player[tdL.idThread].isExit=false;
        player[tdL.idThread].points=0;
        if(onGoing!=0)
        {   
            nrTh2=jucatoriInregistrati;
            sesiune++;
        }
        jucatoriInregistrati++;
        player[tdL.idThread].session=sesiune;
    }
}

void Joc(void *arg,int sesiune,int jucatoriInregistrati,int jucatoriFinal)
{

    int Inregistrati=jucatoriInregistrati;
    int Final=jucatoriFinal;
    int Sesiune=sesiune;

    onGoing++;
    jucatoriInregistrati=0;
    jucatoriFinal=0;    

    struct thData tdL;
    tdL=*((struct thData*)arg);
    int i;

    if(player[tdL.idThread].session==Sesiune)
        for(i=1;i<=20;i++)
        {
            raspunde(i,(struct thData*)arg);
            checkAnswer(i,(struct thData*)arg);
            if(player[tdL.idThread].isLogged==false)
            {
                i=21;//jucatorul a raspuns prea tarziu, nu mai are drept sa participe si este descalificat
            }
        }

    Final++;
    printf("WAIT FOR ALL PLAYERS TO FINISH!\n");
    
    if(Final<=Inregistrati)
    {
        pthread_mutex_lock(&m);
        nrTh++;
        if(nrTh==Inregistrati)
        {
            pthread_cond_broadcast(&cv);
        }
        else
        {
            while(nrTh!=Inregistrati)
            {
                pthread_cond_wait(&cv,&m);
            }
        }
        pthread_mutex_unlock(&m);
    }
    printf("PLAYERS DONE!\n");
   

    if(check==0)
    {
        rank(Sesiune,Inregistrati);
        check=1;
    }

    if(player[tdL.idThread].isExit==false)
    {
        sendRank(arg,Sesiune);
    }      
    
    printf("GAME DONE!\n");
    deleteInfo(arg,Inregistrati,Final);
}

void raspunde(int ID,void *arg)
{   
    struct thData tdL;
    tdL=*((struct thData*)arg);

    //pregatim mesajul de raspuns
    char question[1024];
    int len;
    
    getQuestion(ID,question);

    len=strlen(question);
    question[len]='\0';

    //prefixarea lungimii
    if((write(tdL.cl,&len,sizeof(len)))==-1)
    {
        printf("[Thread %d]Eroare la write catre client!\n",tdL.idThread);
    }
    //trimitem intrebarea clientului
    if(write(tdL.cl,question,len)<=0)
    {
        printf("[Thread %d]Eroare la write catre client!\n",tdL.idThread);
    }
    else
    {
        printf("[Thread %d]Mesajul a fost transmis cu succes\n",tdL.idThread);
    }
             
}

void getQuestion(int ID,char* question)
{
    sqlite3* db_ptr;
    sqlite3_stmt* stmt;
       

    if ((sqlite3_open("trivia.db", &db_ptr)) != SQLITE_OK) 
    {
        printf("Eroare la deschiderea bazei de date!\n");
    }

    char* sql_stmt = "SELECT * FROM trivia";


    if ((sqlite3_prepare_v2(db_ptr, sql_stmt, -1, &stmt, 0)) != SQLITE_OK) 
    {
        printf("Eroare la preluarea datelor din baza de date!\n");
        sqlite3_close(db_ptr);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if(ID==sqlite3_column_int(stmt,0))
        {
            strcpy(question,sqlite3_column_text(stmt,1));
            strcat(question,"\n");
            strcat(question,sqlite3_column_text(stmt,2));
            strcat(question," ");
            strcat(question,sqlite3_column_text(stmt,3));
            strcat(question," ");
            strcat(question,sqlite3_column_text(stmt,4));
            strcat(question," ");
            strcat(question,sqlite3_column_text(stmt,5));
                     
            printf("Trimitem mesajul inapoi: '%s'\n",question);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db_ptr);
}

void checkAnswer(int ID,void *arg)
{
    sqlite3* db_ptr;
    sqlite3_stmt* stmt;
    struct thData tdL;
    tdL=*((struct thData*)arg);

    if ((sqlite3_open("trivia.db", &db_ptr)) != SQLITE_OK) 
    {
        printf("Eroare la deschiderea bazei de date!\n");
    }

    char* sql_stmt = "SELECT * FROM trivia";


    if ((sqlite3_prepare_v2(db_ptr, sql_stmt, -1, &stmt, 0)) != SQLITE_OK) 
    {
        printf("Eroare la preluarea datelor din baza de date!\n");
        sqlite3_close(db_ptr);
        return;
    }

    int len;
    char buf[10];
    
    if((read(tdL.cl,&len,sizeof(len)))<0)
    {
        printf("[Thread %d]\n",tdL.idThread);
        perror("Eroare la read de la client!\n");
    }
    if(read(tdL.cl,buf,len)<0)
    {
        printf("[Thread %d]\n",tdL.idThread);
        perror("Eroare la read de la client!\n");
    }
    buf[len]='\0';

    printf("Am primit raspunsul: '%s' cu len='%d'\n",buf,len);
    if(strcmp(buf,"X")==0)
    {
        printf("[thread - %d]PLAYER-UL %s A RASPUNS PREA TARZIU! VA FI DESCALIFICAT.\n",tdL.idThread,player[tdL.idThread].nickname);
        player[tdL.idThread].isLogged=false;
    }
    else
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            if(ID==sqlite3_column_int(stmt,0))
            {
                if(strcmp(sqlite3_column_text(stmt,6),buf)==0)
                {
                    printf("[Thread %d]AI RASPUNS CORECT\n",tdL.idThread);
                    score((struct thData*)arg);
                }
                else
                {
                    printf("[Thread %d]GRESIT\n",tdL.idThread);
                }
            }
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db_ptr);
}

void score(void *arg)
{
    struct thData tdL;
    tdL=*((struct thData*)arg);
    
    player[tdL.idThread].points+=5;
}

void rank(int Sesiune,int Inregistrati)
{
    int i,j;
    int INR=Inregistrati;
    int SES=Sesiune;
    struct rank rank[1000];
    struct rank aux;
    char top_line[200];

    memset(clasament,0,sizeof(clasament));

    for(i=0;i<INR+jucatoriExit;i++)
    {
        strcpy(rank[i].nickname,player[i].nickname);
        rank[i].points=player[i].points;
        rank[i].isLogged=player[i].isLogged;
        rank[i].session=player[i].session;
        rank[i].isExit=player[i].isExit;
    }

    for(i=0;i<INR+jucatoriExit-1;i++)
        for(j=i+1;j<INR+jucatoriExit;j++)
        {
            if(rank[i].isLogged==false)
            {
                rank[i].points=-1;
            }
            if((rank[i].points<rank[j].points || (rank[j].isExit==false && rank[i].isExit==true)))
            {
                aux=rank[i];
                rank[i]=rank[j];
                rank[j]=aux;
            }
        }
    
    j=0;
    for(i=0;i<INR;i++)
    {   
        if(rank[i].session==SES)
        {
            if(rank[i].isLogged==false)
                sprintf(top_line,"[DESCALIFICAT]: Jucatorul %s, punctaj %d, sesiune: %d\n",
                    rank[i].nickname,rank[i].points,rank[i].session);
            else
            {
                sprintf(top_line,"LOCUL %d: Jucatorul %s are %d puncte, si activ=%d, iar sesiune: %d\n",
                    j+1,rank[i].nickname,rank[i].points,rank[i].isLogged,rank[i].session);
                j++;
            }
            strcat(clasament,top_line);
            memset(top_line,0,sizeof(top_line));
        }
    }

    int len=strlen(clasament);
    clasament[len-1]='\0';
}

void sendRank(void *arg,int Sesiune)
{
    struct thData tdL;
    tdL=*((struct thData*)arg);
    int len;
    int SES=Sesiune;
    len=strlen(clasament);
    clasament[len]='\0';

    if(player[tdL.idThread].session==SES)
    {
        //prefixarea lungimii
        if((write(tdL.cl,&len,sizeof(len)))==-1)
        {
            printf("[Thread %d]Eroare la write catre client!\n",tdL.idThread);
        }

        //trimitem intrebarea clientului
        if(write(tdL.cl,clasament,len)<=0)
        {
            printf("[Thread %d]Eroare la write catre client!\n",tdL.idThread);
        }
        else
        {
            printf("[Thread %d]Mesajul a fost transmis cu succes\n",tdL.idThread);
        }
    }
}

void deleteInfo(void *arg,int Inregistrati,int Final)
{
    int INR=Inregistrati;
    int FIN=Final;

    struct thData tdL;
    tdL=*((struct thData*)arg);
   
    player[tdL.idThread].isLogged=false;
    player[tdL.idThread].isExit=true;
    player[tdL.idThread].points=0;
    
    INR--;
    jucatoriInregistrati--;
    onGoing--;
    
    if(INR==0)
    {    
        FIN=0;  
    }
}