#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <signal.h>
#include <time.h>

// ������ ����ü ����
struct card {// ī�� ������ ����
	int value; // ī�� ���� (1~13)
	char suit; // ī�� ����
};
struct gameInfo{ // manager�� player�� �ְ� �޴� ������ ���� 
    struct card cards[53]; // player�� ���� ������ ī�� ����
    int num_cards; // player�� ���� ������ ī�� ����
    struct card open_card; // ���� open card ����
};
struct socket_msg{
    char text[1024]; // �ؽ�Ʈ �޼���
    int flag; // ���� flag
};

// �������� ����
struct card cards[53]; // ī�� ���� (parent ���μ����� ���)
struct card open_card; // ���� open card ���� (parent ���μ����� ���)
int top_card = 0; // ������ ���� ���� �ִ� ī���� ���� (parent ���μ����� ���)
int child_client_sock; // �ñ׳� ����� ���� client ���� ���� (child ���μ����� ���)
int g_count = 0;

//���� ������ ���� �ʿ��� �Լ� ����
struct socket_msg receive_sock(int sock){ // ���� ���� �Լ�
    struct socket_msg msg;
    recv(sock, (struct socket_msg*)&msg, sizeof(msg),0);
    return msg;
}
void send_sock(int sock, struct socket_msg msg){ // ���� �۽� �Լ�
    send(sock, (struct socket_msg*)&msg, sizeof(msg), 0);
}
void make_cards(){ // ī�� ���� ����
    // make cards
    printf("Make cards");
	for (int i=0;i<13;i++) {
		cards[i].value=i%13+1;
		cards[i].suit='c';
	}
	for (int i=0;i<13;i++) {
		cards[i+13].value=i%13+1;
		cards[i+13].suit='d';
	}
	for (int i=0;i<13;i++) {
		cards[i+26].value=i%13+1;
		cards[i+26].suit='h';
	}
	for (int i=0;i<13;i++) {
		cards[i+39].value=i%13+1;
		cards[i+39].suit='s';
	}
    cards[52].value = 20;
    cards[52].suit = 'j';
    for (int i=0;i<53;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);
	}
    printf("\n\n");
}
void shuffle(struct card *cards, int num){ // ī�� ���� ����
    srand(time(NULL));
    struct card temp;
    int randInt;
    printf("Shuffling the cards\n");
    for (int i=0; i<num-1; i++){
        randInt = rand() % (num-i) + i;
        temp = cards[i];
        cards[i] = cards[randInt];
        cards[randInt] = temp;
    }
    for (int i=0;i<53;i++) {
		printf("(%c,%d) ", cards[i].suit,cards[i].value);
	}
    printf("\n");
}
void child_proc(int sock, int client_id, int msqid_p2c, int msqid_c2p); // �ڽ� ���μ����� ����, �� �ڿ��� ����

//Signal handling �Լ� ����
void my_turn(int sig){
    struct socket_msg msg;
    msg.flag = 0;
    sprintf(msg.text, "********************\n*******Its your turn\n********************\n");
    send_sock(child_client_sock, msg); // �ȳ����� ����
}
void win_sig(int sig){
    struct socket_msg msg;
    msg.flag = -99;
    sprintf(msg.text, "You are the winner!\n");
    send_sock(child_client_sock, msg); // ���� ��� ����
    close(child_client_sock); // socket close
    exit(0);
}
void lose_sig(int sig){
    struct socket_msg msg;
    msg.flag = -99;
    sprintf(msg.text, "You are the loser.\n");
    send_sock(child_client_sock, msg); // ���� ��� ����
    close(child_client_sock); // socket close
    exit(0);
}
void tie_sig(int sig){
    struct socket_msg msg;
    msg.flag = -99;
    sprintf(msg.text, "Its tie....\n");
    send_sock(child_client_sock, msg); // ���� ��� ����
    close(child_client_sock); // socket close
    exit(0);
}

void main(){
    /*********************************************************
     1. ���� ����� ���� ���� *********************************
    **********************************************************/
    int s_sock_fd, new_sock_fd;
    struct sockaddr_in s_address, c_address;
    int addrlen = sizeof(s_address);
    int check;
    int child_process_num = 0; // ������ ������ Ŭ���̾�Ʈ�� ��
    
    s_sock_fd = socket(AF_INET, SOCK_STREAM, 0);// ���� ���� ���� (AF_INET: IPv4 ���ͳ� ��������, SOCK_STREAM: TCP/IP)
    if (s_sock_fd == -1){
        perror("socket failed: ");
        exit(1);
    }
        
    memset(&s_address, '0', addrlen); // s_address�� �޸� ������ 0���� ���� / memset(�޸� ���� ������, ä����� �ϴ� ��, �޸� ����)
    s_address.sin_family = AF_INET; // IPv4 ���ͳ� �������� ����� ���
    s_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // ���� ������ �ּ� ���
    s_address.sin_port = htons(8080); // ���� ������ ��Ʈ��ȣ ���
    check = bind(s_sock_fd, (struct sockaddr *)&s_address, sizeof(s_address)); // ������ ������ ���� ������ ���� ���� Bind
    if(check == -1){
        perror("bind error: ");
        exit(1);
    }

    /*********************************************************
     2. IPC�� ���� ���� *************************************
    **********************************************************/
    pid_t pid_0, pid_1; // player 0�� palyer1�� pid�� ������ ����
    key_t key_p2c_0 = 11111, key_c2p_0 = 22222;
    key_t key_p2c_1 = 33333, key_c2p_1 = 44444;
    int msqid_p2c_0, msqid_c2p_0, msqid_p2c_1, msqid_c2p_1;
    //Message queue �ʱ�ȭ (clear up)
    if((msqid_p2c_0=msgget(key_p2c_0, IPC_CREAT|0666))==-1){exit(-1);}
    msgctl(msqid_p2c_0, IPC_RMID, NULL);
    if((msqid_c2p_0=msgget(key_c2p_0, IPC_CREAT|0666))==-1){exit(-1);}
    msgctl(msqid_c2p_0, IPC_RMID, NULL);
    if((msqid_p2c_1=msgget(key_p2c_1, IPC_CREAT|0666))==-1){exit(-1);}
    msgctl(msqid_p2c_1, IPC_RMID, NULL);
    if((msqid_c2p_1=msgget(key_c2p_1, IPC_CREAT|0666))==-1){exit(-1);}
    msgctl(msqid_c2p_1, IPC_RMID, NULL);
    
    // Message queue ����
    if((msqid_p2c_0=msgget(key_p2c_0, IPC_CREAT|0666))==-1){exit(-1);}
    if((msqid_c2p_0=msgget(key_c2p_0, IPC_CREAT|0666))==-1){exit(-1);}
    if((msqid_p2c_1=msgget(key_p2c_1, IPC_CREAT|0666))==-1){exit(-1);}
    if((msqid_c2p_1=msgget(key_c2p_1, IPC_CREAT|0666))==-1){exit(-1);}

    struct gameInfo sending_info, receiving_info; // ������ �ְ� �ޱ� ���� ����ü ���� (�θ� ���μ����� ���)
    // �� �÷��̾�� ������ �ʱ� ���� ���� ���� �� ����
    printf("Set game info\n");
    make_cards(); // ī�� ����
    shuffle(cards, 53); // ī�� ����
    printf("\n@@ cards setting---------------------------\n");
    top_card = 0; //  �� ���� ���� �ִ� ī���� �ε��� ǥ��

    //player 0���� �� ù 6�� ī�� ���� �� ����
    sending_info.num_cards = 0;
    for (int i=0; i<6; i++){ 
        sending_info.cards[i]= cards[i];
        sending_info.num_cards ++;
        top_card += 1;
    }
    sending_info.open_card = cards[top_card+6];
    msgsnd(msqid_p2c_0, &sending_info, sizeof(struct gameInfo),0); // ù 6���� ī�� ����
    //player 1���� �� ù 6�� ī�� ���� �� ����
    sending_info.num_cards = 0;
    int i = 0;
    for (i=0; i < 6; i++){
        sending_info.cards[i]= cards[top_card];
        sending_info.num_cards ++;
        top_card += 1;
    }
    sending_info.open_card = cards[top_card];
    open_card = cards[top_card];
    
    top_card++;
    msgsnd(msqid_p2c_1, &sending_info, sizeof(struct gameInfo), 0); // ù 6���� ī�� ����

    while(1){
    /*********************************************************
     4. ���� �÷��̾� ���� ��� ********************************
    **********************************************************/
        // Ŭ���̾�Ʈ�� ������ ��ٸ�
        check = listen(s_sock_fd, 16);
        if(check==-1){
            perror("listen failed: ");
            exit(1);
        }

        // Ŭ���̾�Ʈ�� ������ �㰡�� -> ���ӿ� ������ Ŭ���̾�Ʈ���� ����� ���� ���ο� ������ ������
        new_sock_fd = accept(s_sock_fd, (struct sockaddr *) &c_address, (socklen_t*)&addrlen);
        if(new_sock_fd<0){
            perror("accept failed: ");
            exit(1);
        }

        //���� ������ ���� �ڽ� ���μ��� ����
        int c_pid = fork();
        if(c_pid==0){ // �ڽ� ���μ���
            if(0==child_process_num){
                child_proc(new_sock_fd,  child_process_num, msqid_p2c_0, msqid_c2p_0); // player 0�� ����� �ڽ� ���μ���
                
            }
            else if(1==child_process_num){
                child_proc(new_sock_fd,  child_process_num,  msqid_p2c_1, msqid_c2p_1); // player 1�� ����� �ڽ� ���μ���
            }

            
        }
        else{ // �θ� ���μ���
            if (child_process_num==0){
                pid_0 = c_pid; // ù��° �ڽ� ���μ����� pid ����
            }
            else if (child_process_num==1){
                pid_1 = c_pid; // �ι�° �ڽ� ���μ����� pid ����
            }
            child_process_num ++;
            close(new_sock_fd);// �θ� ���μ����� ���ο� ������ ������ �ʿ� ����.
    /*********************************************************
     5. ���� ���� ********************************************
    **********************************************************/
            if (child_process_num >=2){// 2���� Client�� �����ϸ� ���� ����
                
                while(1){//�ݺ��Ǵ� ���� ����
                    ////////////////////////////////////////////////////////////////////////
                    // Player 0�� ����//////////////////////////////////////////////////////
                    //////////////////////////////////////////////////////////////////////
                    kill(pid_0, SIGUSR1);
                    printf("Player 0's turn\n");
                    // 1. ���� ����ī�� ���� ����
                    sending_info.open_card = open_card;
                    msgsnd(msqid_p2c_0, &sending_info, sizeof(struct gameInfo),0);
                    // 2. �÷��̾� 0�� ���� ���� ����
                    msgrcv(msqid_c2p_0, &receiving_info, sizeof(struct gameInfo), 0 ,0);
                    // 3. �÷��̾� 0�� ���� ������ ���� �ൿ
                    if ((receiving_info.open_card.value == open_card.value && receiving_info.open_card.suit == open_card.suit)){
                        //3-1. open ī�� ������ ������, �÷��̾ ī�带 �������� ���� ��. --> ���ο� ī�� ����.                    
                        if(receiving_info.open_card.suit == 'j' && g_count == 0){
                            for(int i = 0; i < 5; i++){
                                sending_info.cards[receiving_info.num_cards + i] = cards[top_card];
                                top_card++;
                            }
                            sending_info.num_cards = receiving_info.num_cards+5;
                            g_count = 1; 
                            msgsnd(msqid_p2c_0, &sending_info, sizeof(struct gameInfo),0);
                                                       
                        }
                        else{
                            for(int i=0; i<receiving_info.num_cards; i++){
                                sending_info.cards[i] = receiving_info.cards[i];
                            }
                            sending_info.cards[receiving_info.num_cards] = cards[top_card];
                            sending_info.num_cards = receiving_info.num_cards+1;
                            top_card ++;
                            msgsnd(msqid_p2c_0, &sending_info, sizeof(struct gameInfo),0); // ù 6���� ī�� ����
                        }
                    }
                    else{
                        //3-2. open ī�� ������ �ٸ���, �÷��̾ ī�带 �������� ��. --> ���� open ī�� ���� ������Ʈ
                        open_card = receiving_info.open_card;
                        g_count = 0;
                     }
                    // 4. ���� ���� �Ǵ�
                    if (receiving_info.num_cards == 0){
                        kill(pid_0, SIGINT); // �¸�
                        kill(pid_1, SIGQUIT); // �й�
                        printf("Player 1 Win\n");
                    }
                    if (receiving_info.num_cards > 20){
                        kill(pid_0, SIGQUIT); // �й�
                        kill(pid_1, SIGINT); // �¸�
                        printf("Player 2 Win\n");
                    }
                    if (top_card >= 52){
                        //ī�� �پ�, ���º�
                        kill(pid_0, SIGILL);
                        kill(pid_1, SIGILL); 
                    }
                    ////////////////////////////////////////////////////////////////////////
                    // Player 1�� ����//////////////////////////////////////////////////////
                    ///////////////////////////////////////////////////////////////////////
                    kill(pid_1, SIGUSR1);
                    printf("Player 1's turn\n");
                    // 1. ���� ����ī�� ���� ����
                    sending_info.open_card = open_card;
                    msgsnd(msqid_p2c_1, &sending_info, sizeof(struct gameInfo),0);
                    // 2. �÷��̾� 1�� ���� ���� ����
                    msgrcv(msqid_c2p_1, &receiving_info, sizeof(struct gameInfo), 0 ,0);
                    // 3. �÷��̾� 1�� ���� ������ ���� �ൿ
                    if ((receiving_info.open_card.value == open_card.value && receiving_info.open_card.suit == open_card.suit)){
                        //3-1. open ī�� ������ ������, �÷��̾ ī�带 �������� ���� ��. --> ���ο� ī�� ����.
                        if(receiving_info.open_card.suit == 'j' && g_count == 0){
                            for(int i = 0; i < 5; i++){
                                sending_info.cards[receiving_info.num_cards + i] = cards[top_card];
                                top_card++;
                            }
                            sending_info.num_cards = receiving_info.num_cards+5;
                            g_count = 1;
                            msgsnd(msqid_p2c_1, &sending_info, sizeof(struct gameInfo),0);
                            
                        }
                        else{
                            for(int i=0; i<receiving_info.num_cards; i++){
                                sending_info.cards[i] = receiving_info.cards[i];
                            }
                            sending_info.cards[receiving_info.num_cards] = cards[top_card];
                            sending_info.num_cards = receiving_info.num_cards+1;
                            top_card ++;
                            msgsnd(msqid_p2c_1, &sending_info, sizeof(struct gameInfo),0); // ù 6���� ī�� ����
                        }
                    }

                    else{
                        //3-2. open ī�� ������ �ٸ���, �÷��̾ ī�带 �������� ��. --> ���� open ī�� ���� ������Ʈ
                        open_card = receiving_info.open_card;
                        g_count = 0;
                    }
                    // 4. ���� ���� �Ǵ�
                    if (receiving_info.num_cards == 0){
                        kill(pid_1, SIGINT); // �¸�
                        kill(pid_0, SIGQUIT); // �й�
                        printf("Player 1 Win\n");
                    }
                    if (receiving_info.num_cards > 20){
                        kill(pid_1, SIGQUIT); // �й�
                        kill(pid_0, SIGINT); // �¸�
                        printf("Player 0 Win\n");
                    }
                    if (top_card >= 52){
                        //ī�� �پ�, ���º�
                        kill(pid_0, SIGILL);
                        kill(pid_1, SIGILL); 
                    }
                }
                exit(0); // ������ ������ ���μ��� ����
            }
        }
    }
}

void child_proc(int sock, int client_id, int msqid_p2c, int msqid_c2p){
    /* �Ű�����
        0. ����fd
        1. client ��ȣ (Ȥ�� player ��ȣ)
        2. parent to child �޼���ť id
        3. child to parent �޼���ť id
    */
    printf("child(%d) joined.\n", client_id);
    struct socket_msg msg; // client�� ������ �ְ� ���� ����ü
    struct gameInfo player_info; // player�� ��������
    struct gameInfo receiving_info; // �θ� ���μ����ο��� ���޹��� ���� ����
    child_client_sock = sock; // �ñ׳� ����� ���� sock ���������� ����
    int count = 0; // joker count    

    signal(SIGINT, win_sig); // �¸� �ñ׳�
    signal(SIGQUIT, lose_sig); //�й� �ñ׳�
    signal(SIGILL, tie_sig); //���º� �ñ׳�

    // �ʱ� ���� ����
    msgrcv(msqid_p2c, &player_info, sizeof(struct gameInfo), 0 , 0);
    // msg.flag=0 -> �ȳ� ����
    // msg.flag=1 -> �ȳ����� + �Է¿��
    msg.flag = 0;
    sprintf(msg.text,"You got six cards.\n");
    send(sock, (struct socket_msg*)&msg, sizeof(msg), 0);
    for (int i=0; i<player_info.num_cards; i++) {
        sprintf(msg.text, "%d:(%c,%d), ", i, player_info.cards[i].suit, player_info.cards[i].value);
        send_sock(sock, msg);
    }
    sprintf(msg.text, "\nGame will starts soon----------------.\n");
    send_sock(sock, msg);
    
    while(1){
        // �ñ׳� �� ������ ���
        signal(SIGUSR1, my_turn);
        pause();

        // �θ� ���μ����κ��� ���� ���� �� ������Ʈ
        msgrcv(msqid_p2c, &receiving_info, sizeof(struct gameInfo), 0 ,0);
        player_info.open_card = receiving_info.open_card;

        // ���� ���� ī�� ���� Client���� �۽�
        sprintf(msg.text, "--------------------------------------------------\n");
        send_sock(sock, msg);
        msg.flag = 0;
        sprintf(msg.text, "Current Open Card: (%c,%d)\n", player_info.open_card.suit, player_info.open_card.value);
        send_sock(sock, msg);
       
        // ���� �� ī�� ���� Client���� �۽�
        sprintf(msg.text, "Your Cards List\n");
        send_sock(sock, msg);
        for (int i=0; i<player_info.num_cards; i++) {
            sprintf(msg.text, "%d:(%c,%d), ", i, player_info.cards[i].suit, player_info.cards[i].value);
            send_sock(sock, msg);
        }
        sprintf(msg.text, "\n");
        send_sock(sock, msg);
        sprintf(msg.text, "--------------------------------------------------\n");
        send_sock(sock, msg);

        // ����� ī�� ���� ��û (client�� ���� �Է¹޾ƾ� ��)
        int user_input;
        msg.flag = 1; // input flag
        sprintf(msg.text, "Select card index:");
        send_sock(sock, msg);

        // *client�� �Է°�� ����
        // *���ŵ� ���� �θ� ���μ������� ����
        // *�θ� ���μ������� �������� ��� ���޹޾Ƽ�, player info ����
        msg = receive_sock(sock); // Ŭ���̾�Ʈ �Է°�� ����
        user_input = msg.flag;
        if (player_info.cards[user_input].suit==player_info.open_card.suit || player_info.cards[user_input].value==player_info.open_card.value){
            //ī�� ���
            if (player_info.num_cards == 1){
                // user_input 0, num cards�� 1�϶� ���� �߻��ϴ°� ����. (my_info.cards �����ϸ� ���� �߻�.)
                player_info.num_cards=0;
            }
            
            else{
                struct card temp = player_info.cards[msg.flag];
                for (int i=user_input; i < player_info.num_cards; i++){ // ī�� ����Ʈ���� ����� ī�� ����
                    player_info.cards[i].value = player_info.cards[i+1].value;
                    player_info.cards[i].suit = player_info.cards[i+1].suit;
                }
                player_info.num_cards -= 1;
                player_info.open_card = temp;
                msg.flag = 0;
                sprintf(msg.text, "Card (%c,%d) is dropped\n", temp.suit, temp.value);
                send_sock(sock, msg); // ������Ʈ �� ���� ����
            }
            msgsnd(msqid_c2p, &player_info, sizeof(struct gameInfo),0);
        }
        else if((player_info.cards[user_input].suit == 'j' || player_info.cards[user_input].value == 20)){
            count++;
            if (player_info.num_cards == 1){
                // user_input 0, num cards�� 1�϶� ���� �߻��ϴ°� ����. (my_info.cards �����ϸ� ���� �߻�.)
                player_info.num_cards=0;
            }
            else{
                struct card temp = player_info.cards[msg.flag];
                for (int i=user_input; i < player_info.num_cards; i++){ // ī�� ����Ʈ���� ����� ī�� ����
                    player_info.cards[i].value = player_info.cards[i+1].value;
                    player_info.cards[i].suit = player_info.cards[i+1].suit;
                }
                player_info.num_cards -= 1;
                player_info.open_card = temp;
                msg.flag = 0;
                sprintf(msg.text, "Card (%c,%d) is dropped\n", temp.suit, temp.value);
                send_sock(sock, msg); // ������Ʈ �� ���� ����
            }
            msgsnd(msqid_c2p, &player_info, sizeof(struct gameInfo),0);
        }

        else if(count == 1 && player_info.open_card.suit=='j'){
            //ī�� ���
            if (player_info.num_cards == 1){
                // user_input 0, num cards�� 1�϶� ���� �߻��ϴ°� ����. (my_info.cards �����ϸ� ���� �߻�.)
                player_info.num_cards=0;
            }
            
            else{
                struct card temp = player_info.cards[msg.flag];
                for (int i=user_input; i < player_info.num_cards; i++){ // ī�� ����Ʈ���� ����� ī�� ����
                    player_info.cards[i].value = player_info.cards[i+1].value;
                    player_info.cards[i].suit = player_info.cards[i+1].suit;
                }
                player_info.num_cards -= 1;
                player_info.open_card = temp;
                msg.flag = 0;
                sprintf(msg.text, "Card (%c,%d) is dropped\n", temp.suit, temp.value);
                send_sock(sock, msg); // ������Ʈ �� ���� ����
            }
            msgsnd(msqid_c2p, &player_info, sizeof(struct gameInfo),0);
        }
        
        else{
            //ī�� ��� X
            msg.flag = 0;
            sprintf(msg.text, "You cannot drop this card. You will have another card.\n");
            send_sock(sock, msg); // ������Ʈ �� ���� ����
            // ������Ʈ �� ���� ����
            msgsnd(msqid_c2p, &player_info, sizeof(struct gameInfo),0);
            // ���ο� ī�尡 ���Ե� ���� ����.
            msgrcv(msqid_p2c, &receiving_info, sizeof(struct gameInfo), 0 ,0);
            int num = receiving_info.num_cards - player_info.num_cards;
            player_info.num_cards = receiving_info.num_cards;
            for (int i=0; i < player_info.num_cards; i++){ // ī������ ����
                player_info.cards[i].value = receiving_info.cards[i].value;
                player_info.cards[i].suit = receiving_info.cards[i].suit;
                if(i==player_info.num_cards-1){
                    for(int j = i-num+1; j < player_info.num_cards; j++){
                        sprintf(msg.text, "Card (%c,%d) is added\n", player_info.cards[j].suit, player_info.cards[j].value);
                        send_sock(sock, msg); // ������Ʈ �� ���� ����
                    }                    
                }
            }
        }
        msg.flag = 0;
        sprintf(msg.text, "Your turn is over. Waiting for the next turn.\n--------------------------------------------------\n");
        send_sock(sock, msg); // �� ���� �� ���
    }
}