#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct item
{
    int fd;
    char ipaddress[50];
    int connected;
    int current;
    int whatplayer; // 0 if x, 1 if o, 2 if spectator
    struct item *next;
};

struct item *head = NULL;
char board[9];

int main(int argc, char **argv)
{
    extern void setboard();
    extern void showboard(int fd);
    extern int game_is_over();

    static char x_msg[] = "You now get to play!  You are now x.\r\n";
    static char o_msg[] = "You now get to play!  You are now o.\r\n";
    static char xturn_msg[] = "It is x's turn.\r\n";
    static char oturn_msg[] = "It is o's turn.\r\n";
    static char yourturn_msg[] = "It is your turn.\r\n";
    static char notnow_msg[] = "It is not your turn\r\n";
    static char notplaying_msg[] = "You can't make moves; you can only watch the game\r\n";
    static char taken_msg[] = "That spot is already taken\r\n";
    static char over_msg[] = "Game over!\r\n";
    static char again_msg[] = "Let's play again!\r\n";
    static char switchx_msg[] = "You are now x.\r\n";
    static char switcho_msg[] = "You are now o.\r\n";
    char win_msg[20];
    char movemsg[20];
    char newpmsg[100];


    setboard(board);
    

    int have_x = 0;
    int have_o = 0;
    int spot;

    struct item *lp;
    struct item *lp2;
    struct item *lp3;
    int c, port = 3000, status = 0;
    int fd, clientfd;
    socklen_t qsize = sizeof(struct sockaddr_in);
    struct sockaddr_in r, q;
    fd_set fds;
    int numfd;
    int bytes_in_buf, len;
    int received = 0;
    char buf[1010];
    int turn = 0; // 0 is x, 1 is o

    while ((c = getopt(argc, argv, "p:")) != EOF)
    {
        switch (c)
        {
        case 'p':
            port = atoi(optarg);
            break;
        default:
            status = 1;
        }
    }
    if (status || optind < argc)
    {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return (1);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return (1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&r, sizeof r) < 0)
    {
        perror("bind");
        return (1);
    }
    if (listen(fd, 5))
    {
        perror("listen");
        return (1);
    }

    while (1)
    {

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        numfd = fd + 1;

        for (lp = head; lp; lp = lp->next)
        {
            if (lp->connected == 1)
            {
                FD_SET(lp->fd, &fds);
            }
            if ((lp->fd) >= numfd)
            {
                numfd = lp->fd + 1;
            }
        }

        if (select(numfd, &fds, NULL, NULL, NULL) != 1)
        {
            perror("select");
            exit(1);
        }

        if (FD_ISSET(fd, &fds))
        {
            if ((clientfd = accept(fd, (struct sockaddr *)&q, &qsize)) < 0)
            {
                perror("accept");
                return (1);
            }
            // A NEW CLIENT HAS JOINED
            printf("new connection from %s\n", inet_ntoa(q.sin_addr));



            // Send the board

            showboard(clientfd);
            if (turn == 0) {
                write(clientfd, xturn_msg, sizeof xturn_msg - 1);
            } else {
                write(clientfd, oturn_msg, sizeof oturn_msg - 1);
            }




            struct item *p;
            if ((p = malloc(sizeof(struct item))) == NULL)
            {
                fprintf(stderr, "error: Out of memory\n");
                return (1);
            }
            p->fd = clientfd;
            strncpy(p->ipaddress, inet_ntoa(q.sin_addr), 49);
            (p->ipaddress)[49] = '\0';
            p->connected = 1;
            p->current = 0;
            


            if (have_x == 0) {
                // let them be x
                p->whatplayer = 0;
                have_x = 1;

                printf("client from %s is now x\n", p->ipaddress);
                // broadcast to new x
                write(p->fd, x_msg, sizeof x_msg - 1);
                for (lp3 = head; lp3; lp3 = lp3->next)
                {
                    if (lp3->connected == 1 && lp3->whatplayer != 0)
                    {
                        strncpy(newpmsg, p->ipaddress, 64);
                        strcat(newpmsg, " is now playing 'x'.\r\n");
                        write(lp3->fd, newpmsg, strlen(newpmsg) + 1);
                    }
                }
            } else if (have_o == 0) {
                // let them be o
                p->whatplayer = 1;
                have_o = 1;

                printf("client from %s is now o\n", p->ipaddress);
                // broadcast to new x
                write(p->fd, o_msg, sizeof o_msg - 1);
                for (lp3 = head; lp3; lp3 = lp3->next)
                {
                    if (lp3->connected == 1 && lp3->whatplayer != 1)
                    {
                        strncpy(newpmsg, p->ipaddress, 64);
                        strcat(newpmsg, " is now playing 'o'.\r\n");
                        write(lp3->fd, newpmsg, strlen(newpmsg) + 1);
                    }
                }
            } else {
                // they're a spectator
                p->whatplayer = 2;
            }
            
            // add this client to the linked list
            p->next = head;
            head = p;
        }
        else
        {
            for (lp = head; lp; lp = lp->next)
            {
                if (lp->connected == 1 && FD_ISSET(lp->fd, &fds))
                {
                    lp->current = 1;
                    bytes_in_buf = 0;
                    received = 1;
                    // READ A WHOLE LINE FROM THE CLIENT:
                    while (buf[bytes_in_buf - 1] != '\n')
                    {
                        if (bytes_in_buf > 1000)
                        {
                            fprintf(stderr, "error: Buffer limit exceeded (1000)\n");
                            return (1);
                        }
                        len = read(lp->fd, buf + bytes_in_buf, 1);
                        if (len < 0)
                        {
                            perror("read");
                            return (1);
                        }
                        else if (len == 0)
                        {
                            // THE CLIENT HAS DISCONNECTED!
                            printf("disconnecting client %s\n", lp->ipaddress);
                            close(lp->fd);
                            lp->connected = 0;
                            received = 0;

                            lp->current = 0;

                            if (lp->whatplayer == 0) {
                                // find new x:
                                lp->whatplayer = 2;
                                have_x = 0;
                                for (lp2 = head; lp2; lp2 = lp2->next) {
                                    if (lp2->connected == 1 && lp2->whatplayer == 2) {
                                        lp2->whatplayer = 0;
                                        have_x = 1;

                                        printf("client from %s now x\n", lp2->ipaddress);
                                        // broadcast to new x
                                        write(lp2->fd, x_msg, sizeof x_msg - 1);
                                        // broadcast to everyone else
                                        for (lp3 = head; lp3; lp3 = lp3->next) {
                                            if (lp3->connected == 1 && lp3->whatplayer != 0) {
                                                strncpy(newpmsg, lp->ipaddress, 64);
                                                strcat(newpmsg, " is now playing 'x'.\r\n");
                                                write(lp3->fd, newpmsg, strlen(newpmsg) + 1);
                                            }
                                        }
                                        break;
                                    }
                                }
                            }

                            if (lp->whatplayer == 1) {
                                // find new o:
                                lp->whatplayer = 2;
                                have_o = 0;
                                for (lp2 = head; lp2; lp2 = lp2->next) {
                                    if (lp2->connected == 1 && lp2->whatplayer == 2) {
                                        lp2->whatplayer = 1;
                                        have_o = 1;

                                        printf("client from %s now o\n", lp2->ipaddress);
                                        // broadcast to new o
                                        write(lp2->fd, o_msg, sizeof o_msg - 1);
                                        
                                        // broadcast to everyone else
                                        for (lp3 = head; lp3; lp3 = lp3->next) {
                                            if (lp3->connected == 1 && lp3->whatplayer != 1) {
                                                strncpy(newpmsg, lp->ipaddress, 64);
                                                strcat(newpmsg, " is now playing 'o'.\r\n");
                                                write(lp3->fd, newpmsg, strlen(newpmsg) + 1);
                                            }
                                        }
                                        break;
                                    }
                                }
                            }

                            break;
                        }
                        else
                        {
                            bytes_in_buf++;
                        }
                    }

                    if (received)
                    {
                        // we have the client's input buf

                        buf[bytes_in_buf - 1] = '\0';

                        

                        // check if its a move or a chat message
                        if (strlen(buf) == 1 && (spot = atoi(buf)) > 0) {
                            if (lp->whatplayer == turn && board[spot - 1] != 'x' && board[spot - 1] != 'o') {
                                // do turn
                                if (turn == 0) {
                                    printf("%s (x) makes move %d\n", lp->ipaddress, spot);
                                    // update board
                                    board[spot - 1] = 'x';
                                    // broadcast x makes move # (including to player)
                                    strcpy(movemsg, "x makes move ");
                                    strcat(movemsg, buf);
                                    strcat(movemsg, "\r\n");
                                
                                } else {
                                    printf("%s (o) makes move %d\n", lp->ipaddress, spot);
                                    // update board
                                    board[spot - 1] = 'o';
                                    // broadcast o makes move # (including to player)
                                    strcpy(movemsg, "o makes move ");
                                    strcat(movemsg, buf);
                                    strcat(movemsg, "\r\n");
                                    
                                }
                                turn = 1 - turn;
                                for (lp2 = head; lp2; lp2 = lp2->next) {
                                    if (lp2->connected == 1) {
                                        write(lp2->fd, movemsg, strlen(movemsg) + 1);
                                        showboard(lp2->fd);
                                        if (lp2->whatplayer == turn) {
                                            write(lp2->fd, yourturn_msg, sizeof yourturn_msg);
                                        } else if (turn == 0)
                                        {
                                            write(lp2->fd, xturn_msg, sizeof xturn_msg - 1);
                                        }
                                        else
                                        {
                                            write(lp2->fd, oturn_msg, sizeof oturn_msg - 1);
                                        }

                                    }
                                }
                                
                                // check for win
                                if (game_is_over() != 0) {

                                    if (game_is_over() == 'x') {
                                        strcpy(win_msg, "x wins.\r\n");
                                    } else if (game_is_over() == 'o') {
                                        // o wins
                                        strcpy(win_msg, "o wins.\r\n");
                                    } else {
                                        strcpy(win_msg, "It is a draw.\r\n");
                                    }

                                    for (lp2 = head; lp2; lp2 = lp2->next) {
                                        if (lp2->connected == 1) {
                                            write(lp2->fd, over_msg, sizeof over_msg - 1);
                                            showboard(lp2->fd);
                                            write(lp2->fd, win_msg, strlen(win_msg) + 1);
                                            write(lp2->fd, again_msg, sizeof again_msg - 1);
                                            
                                            if (lp2->whatplayer == 0) {
                                                lp2->whatplayer = 1;
                                                write(lp2->fd, switcho_msg, sizeof switcho_msg - 1);
                                            } else if (lp2->whatplayer == 1) {
                                                lp2->whatplayer = 0;
                                                write(lp2->fd, switchx_msg, sizeof switchx_msg - 1);
                                            }
                                        }
                                    }
                                    setboard();
                                    turn = 0;
                                    

                                }
                                
                                

                            } else if (lp->whatplayer == turn) {
                                write(lp->fd, taken_msg, sizeof taken_msg - 1);
                                printf("%s tries to make move %d, but the spot is already taken\n", lp->ipaddress, spot);
                            } else if (lp->whatplayer == 0 || lp->whatplayer == 1) {
                                write(lp->fd, notnow_msg, sizeof notnow_msg - 1);
                                printf("%s tries to make move %d, but it's not their turn\n", lp->ipaddress, spot);
                            } else {
                                write(lp->fd, notplaying_msg, sizeof notplaying_msg - 1);
                                printf("%s tries to make move %d, but they aren't playing\n", lp->ipaddress, spot);
                            }
                            
                        } else {
                            printf("%s said: %s\n", lp->ipaddress, buf);
                            char chatmsg[bytes_in_buf + 100];
                            strncpy(chatmsg, lp->ipaddress, 64);
                            strcat(chatmsg, " said: ");
                            strcat(chatmsg, buf);
                            strcat(chatmsg, "\r\n");
                            for (lp2 = head; lp2; lp2 = lp2->next)
                            {
                                if (lp2->connected == 1 && lp2->current == 0)
                                {
                                    write(lp2->fd, chatmsg, strlen(chatmsg) + 1);
                                }
                            }
                        }


                        
                    }
                    

                    lp->current = 0;
                    break;
                }
            }
        }
    }
    return (0);
}

void setboard() {
    int i;
    for (i = 1; i < 10; i++) {
        board[i-1] = i + '0';
    }
}

void showboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;

    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
        perror("write");
}

int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'a')
            return(0);
    return(1);
}

