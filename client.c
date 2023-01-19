#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

//timpul de raspuns oferit
#define TIME_TO_ANSWER 10

//codul de eroare returrnat la anumite apeluri
extern int errno;
//portul de conectare la server
int port;



int main(int argc,char *argv[])
{
    int sd; //descriptorul de socket
    struct sockaddr_in server; //structura folosita panetru conectare
    //mesajul trimis:
    char buf[1024];
    int len;

    //exista toate argumentele la linia de comanda?
    if(argc!=3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n",argv[0]);
        return -1;
    }

    //stabilim portul
    port=atoi(argv[2]);

    //cream socket-ul
    if((sd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        perror("[client]Eroare la scoket!\n");
        return errno;
    }

    //umplem sturctura folosita pentru realizarea conexiunii cu serverul
    server.sin_family=AF_INET;//familia socket-ului
    server.sin_addr.s_addr=inet_addr(argv[1]);//adresa IP a serverului
    server.sin_port=htons(port);//portul de conectare

    //ne conectam la server
    if(connect(sd,(struct sockaddr *) &server,sizeof(struct sockaddr))==-1)
    {
        perror("[client]Eroare la connect()!\n");
        return errno;
    }

    printf("Bine ati venit la QUIZZGAME, jocul de cultura generala! Esti gata sa iti testezi cunostiintele?\nTot ce trebuie sa faci este sa te inregistrezi cu un nume iar apoi sa raspunzi la intrebari folosind una dintre cele 4 variante de raspuns (A, B, C sau D). Ai grija insa, ai acordate doar %d secunde sa raspunzi, altfel esti descalificat.\nMULT SUCCES!\n\n\n",TIME_TO_ANSWER);
    

    //inregistrare client
    printf("[client]Introduceti un nume: ");
    fflush(stdout);
    fgets(buf,1024,stdin);
    len=strlen(buf)-1;
    
    buf[len]='\0';

    //inregistrarea clientului
    if((write(sd,&len,sizeof(len)))==-1)
    {
        perror("[client]Eroare la write spre server!\n");
        return errno;
    }
    if(write(sd,buf,len)<=0)
    {
        perror("[client]Eroare la write spre server!\n");
        return errno;
    }


    //clientul raspunde celor x intrebari transmise de catre server
    int i;
    for(i=1;i<=20;i++)
    {
        printf("\n");
        //citirea intrebarii si a variantelor de raspuns de la server
        if((read(sd,&len,sizeof(len)))<0)
        {
            perror("[client]Eroare la read de la server!\n");
            return errno;
        }
        if(read(sd,buf,len)<0)
        {
            perror("[client]Eroare la read de la server!\n");
            return errno;
        }

        buf[len]='\0';
        //afisam mesajul primit
        printf("%d: %s\n",i,buf);

        time_t t1,t2;

        time(&t1);
        fgets(buf,1024,stdin);
        len=strlen(buf)-1;
        buf[len]='\0';
        time(&t2);

        //daca jucatorul raspunde prea tarziu, nu mai are drept sa raspunda la nicio intrebare si este descalificat
        if(difftime(t2,t1) > TIME_TO_ANSWER)
        {
            printf("Ai raspuns prea tarziu: %.2lf. Nu vei mai putea raspunde mai departe!\n",difftime(t2,t1));
            buf[0]='X';
            len=1;
            buf[len]='\0';

            i=30; //nu i=21 deoarece trebuie sa stim in final in ce caz ne aflam
        }
        
        //trimiterea raspunsului
        if((write(sd,&len,sizeof(len)))==-1)
        {
            perror("[client]Eroare la write spre server!\n");
            return errno;
        }
        if(write(sd,buf,len)<=0)
        {
            perror("[client]Eroare la write spre server!\n");
            return errno;
        }
        
    }

    
    printf("\n\nCLASAMENT:\n");

    if((read(sd,&len,sizeof(len)))<0)
    {
        perror("[client]Eroare la read de la server!\n");
        return errno;
    }
    if(read(sd,buf,len)<0)
    {
        perror("[client]Eroare la read de la server!\n");
        return errno;
    }
        
    buf[len]='\0';
    printf("%s\n\nFELICITARI TUTUROR!\n",buf);

    //inchidem conexiunea
    close(sd);
}