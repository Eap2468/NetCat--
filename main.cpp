#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <string>

#define POLL_STDIN  0
#define POLL_NETOUT 1
#define POLL_NETIN  2
#define POLL_STDOUT 3

#define BUFFERSIZE 1024

ssize_t fillBuffer(int fd, unsigned char *buffer, size_t *bufferPos);
ssize_t drainBuffer(int fd, unsigned char *buffer, size_t *bufferPos);

int main(int argc, char* argv[])
{
    int stdin_fd = STDIN_FILENO;
    int stdout_fd = STDOUT_FILENO;
    
    int clientfd = -1, serverfd = -1;
    if(argc < 2)
    {
        serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(4444);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        socklen_t sockLen = sizeof(serverfd);
        int enable = 1;
        setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &enable, sockLen);
        
        bind(serverfd, (sockaddr*)&addr, sizeof(addr));
        listen(serverfd, 1);
        
        std::cout << "[+] Server Started on 127.0.0.1:4444" << std::endl;
        
        clientfd = accept(serverfd, 0, 0);
        
        std::cout << "[+] Client Connected" << std::endl;
    }
    else
    {
        if(argc < 3)
        {
            std::cout << "Usage .\\ncTest <IP> <PORT> or just .\\ncTest for server mode (localhost on port 4444)" << std::endl;
            return 0;
        }
        
        int port = -1;
        try
        {
            port = std::stoi(argv[2]);
        } catch (...)
        {
            std::cout << "[-] Port error" << std::endl;
            std::cout << "Usage .\\ncTest <IP> <PORT> or just .\\ncTest for server mode (localhost on port 4444" << std::endl;
            return 0;
        }
        
        std::cout << "[*] Attempting to connect to server" << std::endl;
        
        clientfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        
        if(connect(clientfd, (sockaddr*)&addr, sizeof(addr)) != 0)
        {
            std::cout << "[-] Connection error" << std::endl;
            return 0;
        }
        std::cout << "[+] Connected!" << std::endl;
    }
    
    unsigned char stdinBuff[BUFFERSIZE];
    size_t stdinBuffpos = 0;
    unsigned char netinBuff[BUFFERSIZE];
    size_t netinBuffpos = 0;
    int pollError = 0;
    
    struct pollfd fds[4];
    
    fds[POLL_STDIN].fd = stdin_fd;
    fds[POLL_STDIN].events = POLLIN;
    
    fds[POLL_NETOUT].fd = clientfd;
    fds[POLL_NETOUT].events = 0;
    
    fds[POLL_NETIN].fd = clientfd;
    fds[POLL_NETIN].events = POLLIN;
    
    fds[POLL_STDOUT].fd = stdout_fd;
    fds[POLL_STDOUT].events = 0;
    
    ssize_t code = 0;
    while(true)
    {
        pollError = poll(fds, 4, 10);
        
        if(pollError == -1)
        {
            std::cout << "[-] Poll error" << std::endl;
            break;
        }
        if(fds[POLL_STDIN].fd == -1 || fds[POLL_NETIN].fd == -1 || fds[POLL_NETOUT].fd == -1 || fds[POLL_STDOUT].fd == -1)
        {
            std::cout << "[*] Connection Closed" << std::endl;
            break;
        }
        if(pollError == 0){continue;}
        
        if(fds[POLL_STDIN].revents & POLLIN && stdinBuffpos < BUFFERSIZE)
        {
            std::cout << "[*] stdin" << std::endl;
            code = fillBuffer(fds[POLL_STDIN].fd, stdinBuff, &stdinBuffpos);
            
            if(code == -1)
            {
                fds[POLL_STDIN].fd = -1;
            }
            if(stdinBuffpos > 0)
            {
                fds[POLL_NETOUT].events = POLLOUT;
            }
            if(stdinBuffpos == BUFFERSIZE)
            {
                fds[POLL_STDIN].events = 0;
            }
        }
        if(fds[POLL_NETOUT].revents & POLLOUT && stdinBuffpos > 0)
        {
            std::cout << "[*] netout" << std::endl << std::endl;
            code = drainBuffer(fds[POLL_NETOUT].fd, stdinBuff, &stdinBuffpos);
            
            if(code == -1)
            {
                fds[POLL_NETOUT].fd = -1;
            }
            if(stdinBuffpos < BUFFERSIZE)
            {
                fds[POLL_STDIN].events = POLLIN;
            }
            if(stdinBuffpos == 0)
            {
                fds[POLL_NETOUT].events = 0;
            }
        }
        if(fds[POLL_NETIN].revents & POLLIN && netinBuffpos < BUFFERSIZE)
        {
            std::cout << "[*] netin" << std::endl;
            code = fillBuffer(fds[POLL_NETIN].fd, netinBuff, &netinBuffpos);
            
            if(code == -1)
            {
                fds[POLL_NETIN].fd = -1;
            }
            if(netinBuffpos > 0)
            {
                fds[POLL_STDOUT].events = POLLOUT;
            }
            if(netinBuffpos == BUFFERSIZE)
            {
                fds[POLL_NETIN].events = 0;
            }
        }
        if(fds[POLL_STDOUT].revents & POLLOUT && netinBuffpos > 0)
        {
            std::cout << "[*] stdout" << std::endl;
            code = drainBuffer(fds[POLL_STDOUT].fd, netinBuff, &netinBuffpos);
            
            if(code == -1)
            {
                fds[POLL_STDOUT].fd = -1;
            }
            if(netinBuffpos < BUFFERSIZE)
            {
                fds[POLL_NETIN].events = POLLIN;
            }
            if(netinBuffpos == 0)
            {
                fds[POLL_STDOUT].events = 0;
            }
        }
    }
    
    close(clientfd);
    close(serverfd);
    return 0;
}

ssize_t fillBuffer(int fd, unsigned char *buffer, size_t *bufferPos)
{
    ssize_t recieved = 0, num = BUFFERSIZE - *bufferPos;
    recieved = read(fd, buffer + *bufferPos, num);
    
    if(recieved <= 0)
    {
        std::cout << "[-] Recv Error" << std::endl;
        return -1;
    }
    
    *bufferPos += recieved;
    return recieved;
}

ssize_t drainBuffer(int fd, unsigned char *buffer, size_t *bufferPos)
{
    ssize_t sent = 0, adjust = 0;
    sent = write(fd, buffer, *bufferPos);
    
    if(sent <= 0)
    {
        std::cout << "[+] Send Error" << std::endl;
        return -1;
    }
    
    adjust = *bufferPos - sent;
    if(adjust > 0)
    {
        memmove(buffer, buffer + sent, adjust);
    }
    *bufferPos -= sent;
    return sent;
}
