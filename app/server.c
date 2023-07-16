#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

int main()
{
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(6379),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");

	int epollfd = epoll_create1(0);
	{
		struct epoll_event ev;
		ev.data.fd = server_fd;
		ev.events = EPOLLIN;
		// registered
		epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &ev);
	}
	struct epoll_event events[1024];

	while (1)
	{
		int n = epoll_wait(epollfd, events, 1024, -1);
		for (int i = 0; i < n; i++)
		{
			if (events[i].data.fd == server_fd)
			{
				client_addr_len = sizeof(client_addr);
				int client = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

				printf("Client connected\n");
				struct epoll_event ev;
				ev.data.fd = client;
				ev.events = EPOLLIN;
				epoll_ctl(epollfd, EPOLL_CTL_ADD, client, &ev);
			}
			else if (events[i].events == EPOLLIN)
			{
				char buf[1024];
				int n = read(events[i].data.fd, buf, 1024);
				if (n <= 0)
				{					
					epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
					close(events[i].data.fd);
					break;
				}
				printf("read %d\n", n);
				if (n <= 0)
					break;
				strncpy(buf, "+PONG\r\n", 1024);
				write(events[i].data.fd, buf, strlen(buf));
			}
		}
	}
	close(server_fd);
	// TODO close all connections

	return 0;
}
