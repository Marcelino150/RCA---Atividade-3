#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>		/* for wait() */
#include <sys/wait.h>		/* for wait() */
#include <errno.h>
#include <sys/shm.h>            /* for shmget(), shmat(), shmctl() */
#include <sys/sem.h>            /* for semget(), semop(), semctl() */
#include <signal.h>
#include <arpa/inet.h> 
/*
 * Servidor TCP
 */

#define SEM_KEY		0x1010
#define SHMNO_KEY	0x2020
#define SHMSGS_KEY	0x3030

struct mensagem{

	int validade; 
	char usuario[20];
	char mensagem[80];
};

int	g_sem_id;
int	g_shmno_id;
int	g_shmsgs_id;
int	*nmsgs;
struct mensagem *msgs;

struct sembuf	g_sem_op1[1];
struct sembuf	g_sem_op2[1];

void encerraServidor(){

        if( shmctl( g_shmno_id,IPC_RMID,NULL) != 0 ) {
                fprintf(stderr,"Impossivel remover o segmento de memoria compartilhada!\n");
                exit(1);
        }

        if( shmctl( g_shmsgs_id,IPC_RMID,NULL) != 0 ) {
                fprintf(stderr,"Impossivel remover o segmento de memoria compartilhada!\n");
                exit(1);
        }

        if( semctl( g_sem_id, 0, IPC_RMID, 0) != 0 ) {
                fprintf(stderr,"Impossivel remover o conjunto de semaforos!\n");
                exit(1);
        }

	printf("\nServidor encerrado.\n");

	exit(0);
}

int cadastraMensagem(int ns, struct sockaddr_in client){

	int full = 0;
	struct mensagem recvmsg;

	if (recv(ns, &recvmsg, sizeof(recvmsg), 0) == -1){
		perror("Recv()");
		exit(7);
	}

	if( semop( g_sem_id, g_sem_op2, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel bloquear o recurso!");
               	exit(1);
       	}

	if(*nmsgs == 10){
		full = 1;
	}

	if (send(ns, &full, sizeof(full), 0) < 0){
		perror("Send()");
		exit(8);
	}

	for(int i = 0; i < 10; i++){
		if(msgs[i].validade == 0){
			msgs[i] = recvmsg;
			*nmsgs = *nmsgs + 1;
			printf("Mensagem de %s recebida pela porta %d e cadastrada com usuario %s.\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port),recvmsg.usuario);
			break;
	  	}
	}

	if( semop( g_sem_id, g_sem_op1, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel liberar o recurso!");
               	exit(1);
       	}
}

int removeMensagem(int ns, struct sockaddr_in client){

	int n = 0;
	char recvbuf[512];

	if (recv(ns, recvbuf, sizeof(recvbuf), 0) == -1){
		perror("Recv()");
		exit(11);
	}
	
	if( semop( g_sem_id, g_sem_op2, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel bloquear o recurso!");
               	exit(1);
       	}

	for(int i = 0; i < 10; i++){
		if(msgs[i].validade == 1 && strcmp(msgs[i].usuario,recvbuf) == 0){
			n++;
		}
	}	

	if (send(ns, &n, sizeof(n), 0) < 0){
		perror("Send()");
		exit(12);
	}

  	for(int i = 0; i < 10; i++){
		if(msgs[i].validade == 1 && strcmp(msgs[i].usuario,recvbuf) == 0){
			if (send(ns, &msgs[i], sizeof(msgs[i]), 0) < 0){
				perror("Send()");
				exit(13);
	 		}

			msgs[i].validade = 0;
			*nmsgs = *nmsgs - 1;
		}
	}

	if( semop( g_sem_id, g_sem_op1, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel liberar o recurso!");
               	exit(1);
       	}
	
	if(n != 0){
		printf("%d mensagenm do usuario %s foram removidas a pedido de %s pela porta %d.\n",n,recvbuf,inet_ntoa(client.sin_addr),ntohs(client.sin_port));
	}
}

int enviaMensagem(int ns, struct sockaddr_in client){

	if( semop( g_sem_id, g_sem_op2, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel bloquear o recurso!");
               	exit(1);
       	}

	if(send(ns, nmsgs, sizeof(*nmsgs), 0) < 0){
		perror("Send()");
		exit(9);
	}	
			
	for(int i = 0; i < 10; i++){
		if(msgs[i].validade != 0){
			if (send(ns, &msgs[i], sizeof(msgs[i]), 0) < 0){
				perror("Send()");
				exit(10);
	 		}
		}
	}

	if( semop( g_sem_id, g_sem_op1, 1 ) == -1 ) {      		
                fprintf(stderr,"chamada semop() falhou, impossivel liberar o recurso!");
               	exit(1);
       	}

	printf("Todas as mensagens foram enviadas ao cliente %s pela porta %d.\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));
}

int inicializaMemSem(){

	g_sem_op1[0].sem_num   =  0;
	g_sem_op1[0].sem_op    = 1;
	g_sem_op1[0].sem_flg   =  0;

	g_sem_op2[0].sem_num =  0;
	g_sem_op2[0].sem_op  =  -1;
	g_sem_op2[0].sem_flg =  0;
	
	if( ( g_sem_id = semget( SEM_KEY, 1, IPC_CREAT | 0666 ) ) == -1 ) {
		fprintf(stderr,"chamada a semget() falhou, impossivel criar o conjunto de semaforos!");
		exit(1);
	}
	
	if( semop( g_sem_id, g_sem_op1, 1 ) == -1 ) {
		fprintf(stderr,"chamada semop() falhou, impossivel inicializar o semaforo!");
		exit(1);
	}

	if( (g_shmno_id = shmget( SHMNO_KEY, sizeof(int), IPC_CREAT | 0666)) == -1 ) {
		fprintf(stderr,"Impossivel criar o segmento de memoria compartilhada! errno = %d  \n", errno);
		exit(1);
	}

	if( (nmsgs = (int *)shmat(g_shmno_id, NULL, 0)) == (int *)-1 ) {
		fprintf(stderr,"Impossivel associar o segmento de memoria compartilhada! errno = %d \n", errno);
		exit(1);
	}

	if( (g_shmsgs_id = shmget( SHMSGS_KEY, 10*sizeof(struct mensagem), IPC_CREAT | 0666)) == -1 ) {
		fprintf(stderr,"Impossivel criar o segmento de memoria compartilhada! errno = %d \n", errno);
		exit(1);
	}

	msgs = shmat(g_shmsgs_id, NULL, 0);

	*nmsgs = 0;

	for(int i = 0; i < 10; i++){
		msgs[i].validade = 0;
	}
}

int main(int argc, char **argv)
{
    unsigned short port;                       
    struct sockaddr_in client; 
    struct sockaddr_in server; 
    int s;                     /* Socket para aceitar conexoes       */
    int ns;                    /* Socket conectado ao cliente        */
    int namelen;
    int op;
    pid_t pid, fid;               

    signal(SIGINT, encerraServidor); 
    inicializaMemSem();

    /*
     * O primeiro argumento (argv[1]) e a porta
     * onde o servidor aguardara por conexoes
     */
    if (argc != 2)
    {
        fprintf(stderr, "Use: %s porta\n", argv[0]);
        exit(1);
    }

    port = (unsigned short) atoi(argv[1]);

    /*
     * Cria um socket TCP (stream) para aguardar conexoes
     */
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket()");
        exit(2);
    }

   /*
    * Define a qual endereco IP e porta o servidor estara ligado.
    * IP = INADDDR_ANY -> faz com que o servidor se ligue em todos
    * os enderecos IP
    */
    server.sin_family = AF_INET;   
    server.sin_port   = htons(port);       
    server.sin_addr.s_addr = INADDR_ANY;

    /*
     * Liga o servidor a porta definida anteriormente.
     */
    if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
       perror("Bind()");
       exit(3);
    }

    /*
     * Prepara o socket para aguardar por conexoes e
     * cria uma fila de conexoes pendentes.
     */
    if (listen(s, 1) != 0)
    {
        perror("Listen()");
        exit(4);
    }

    /*
     * Aceita uma conexao e cria um novo socket atraves do qual
     * ocorrera a comunicacao com o cliente.
     */

	signal(SIGCHLD, SIG_IGN);

	while(1){

		namelen = sizeof(client);
		if ((ns = accept(s, (struct sockaddr *)&client, (socklen_t *)&namelen)) == -1)
		{
			perror("Accept()");
			exit(5);
		}

		if ((pid = fork()) == 0) {

			printf("Conexao estabelecida com o IP %s na porta %d.\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));

			while(1){
				/*
				 * Processo filho 
				 */
			      
				/* Fecha o socket aguardando por conexoes */
				close(s);

				/* Processo filho obtem seu proprio pid */
				fid = getpid();

				/* Recebe a operação do cliente*/
				if (recv(ns, &op, sizeof(op), 0) == -1)
				{
				    perror("Recv()");
				    exit(6);
				}

				switch(op){

					/* Cadastra nova mensagem */
					case 1:
						cadastraMensagem(ns,client);
						break;

					/* Envia todos as mensasgens cadastradas */
					case 2:					 
						enviaMensagem(ns,client);
						break;

					/* Remove mensagens cadastrada*/
					case 3:	
						removeMensagem(ns,client);
						break;
				
					/* Aceita a/aguarda pela proxima conexão da fila se o cliente desconectar*/
					case 4:
						printf("Conexao com o IP %s (porta %d) encerrada.\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));

						close(ns);
						exit(0);

						break;
				}
			}
		}
		else{  
			/*
			* Processo pai 
			*/

			if (pid > 0)
			{
				/* Fecha o socket conectado ao cliente */
				close(ns);
			}
			else
			{
				perror("Fork()");
				exit(7);	      
			}
		}
	};

    /* Fecha o socket conectado ao cliente */
    close(ns);

    /* Fecha o socket aguardando por conexões */
    close(s);

    printf("Servidor terminou com sucesso.\n");
    exit(0);
}


