#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int sockfd;
    int len;
    struct sockaddr_in address;
    int result;
    char ch = 'A';

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;   //AF_INET与PF_INET相同
    /* Convert Internet host address from numbers-and-dots notation in CP
   into binary data in network byte order.  */
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    /* host to network short*/
    address.sin_port = htons(4000);
    len = sizeof(address);

    //连接
    result = connect(sockfd, (struct sockaddr *)&address, len);

    if (result == -1)
    {
        perror("oops: client1");
        exit(1);
    }
    printf("write ok\n");
    write(sockfd, &ch, 1);
    printf("read ok\n");
    read(sockfd, &ch, 1);
    printf("char from server = %c\n", ch);
    close(sockfd);
    exit(0);
}
