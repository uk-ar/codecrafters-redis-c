#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

typedef struct Node
{
	struct Node **array;
	char kind;
	char *simple;
	char *bulk;
	int size;
} Node;

enum kind
{
	KIND_SIMPLE = '+',
	KIND_BULK = '$',
	KIND_ARRAY = '*'
};

// Node *parse(char *buf, char *end)
Node *parse(FILE *stream)
{
	printf("start\n");
	char kind;
	if (fread(&kind, sizeof(char), 1, stream) <= 0)
		return NULL;
	printf("kind:\"%c\"\n", kind);

	Node *ans = malloc(sizeof(Node));
	ans->kind = kind;
	char buf[1024];
	if (kind == KIND_ARRAY)
	{
		printf("array\n");
		if (fgets(buf, 1024, stream) <= 0)
			return NULL;
		int n = strtol(buf, NULL, 10);
		printf("array:%d\n", n);
		ans->array = malloc(sizeof(Node) * n);
		// fread(buf, sizeof(char), 1, stream);
		// if (fgets(buf, 1, stream) <= 0)
		// return NULL;
		// printf("array:%d:%d\n", n, ftell(stream));
		for (int i = 0; i < n; i++)
		{
			printf("array:%d/%d\n", i, n);
			ans->array[i] = parse(stream);
		}
		/*Node dummy, *cur = &dummy;
		for (int i = 0; i < n; i++)
		{
			cur->next = parse(fd);
			cur = cur->next;
		}
		return dummy.next;*/
	}
	else if (kind == KIND_SIMPLE)
	{
		printf("simple\n");
		if (!fgets(buf, 1024, stream))
			return NULL;
		ans->simple = strdup(buf);
		printf("simple:\"%s\"", ans->simple);
	}
	else if (kind == KIND_BULK)
	{
		printf("bulk\n");
		if (!fgets(buf, 1024, stream))
			return NULL;
		int n = strtol(buf, NULL, 10);
		printf("bulk:%d\n", n);
		ans->bulk = malloc(sizeof(char) * n);
		fread(ans->bulk, sizeof(char), n, stream);
		printf("bulk:\"%.*s\"\n", n, ans->bulk);
		fgets(buf, 1024, stream);
		ans->size = n;
		// memcpy(ans->bulk, buf, n);
	}
	else
	{
		fread(buf, sizeof(char), 3, stream);
		// buf[3] = '\0';
		printf("error:\"%3s\"\n", buf);
	}
	return ans;
}

void bulk(char *dest, char *src, int n)
{
	int offset = snprintf(dest, 1024, "$%d\r\n", n);
	memcpy(dest + offset, src, n);
	snprintf(dest + offset + n, 3, "\r\n");
	return;
}

typedef struct ListNode
{
	struct ListNode *next;
	char *key, *value;
} ListNode;

ListNode *head = NULL;
ListNode *new_listNode(ListNode *next, char *key, char *value)
{
	ListNode *ans = malloc(sizeof(ListNode));
	ans->next = next;
	ans->key = key;
	ans->value = value;
	return ans;
}

char *get(char *key)
{
	ListNode *cur = head;
	while (cur)
	{
		if (strcmp(cur->key, key) == 0)
			return cur->value;
		cur = cur->next;
	}
	return NULL;
}

void set(char *key, char *value)
{
	printf("SET[%s]", value);
	ListNode *cur = head;
	while (cur)
	{
		if (strcmp(cur->key, key) == 0)
		{
			cur->value = value;
			return;
		}
		cur = cur->next;
	}

	head = new_listNode(head, key, value);
	return;
}

char *str(char *byte, int n)
{
	char *ans = malloc(sizeof(char) * (n + 1));
	memcpy(ans, byte, n);
	ans[n] = '\0';
	return ans;
}
/*void test(){

	exit(0);
}*/

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
				// int n = read(events[i].data.fd, buf, 1024);
				// Node *node = parse(buf, buf + n);
				FILE *stream = fdopen(events[i].data.fd, "r");
				if (!stream)
					exit(0);
				Node *node = parse(stream);
				// fclose(stream);
				if (node == NULL)
				{
					epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
					close(events[i].data.fd);
					break;
				}

				char buf[1024];
				if (memcmp(node->array[0]->bulk, "ping", 4) == 0)
				{
					printf("COM:PING\n");
					strncpy(buf, "+PONG\r\n", 1024);
				}
				else if (memcmp(node->array[0]->bulk, "echo", 4) == 0)
				{
					printf("COM:ECHO\n");
					Node *arg = node->array[1];
					bulk(buf, arg->bulk, arg->size);
				}
				else if (memcmp(node->array[0]->bulk, "set", 3) == 0)
				{
					printf("COM:SET\n");
					set(str(node->array[1]->bulk, node->array[1]->size),
						str(node->array[2]->bulk, node->array[2]->size));
					strncpy(buf, "+OK\r\n", 1024);
				}
				else if (memcmp(node->array[0]->bulk, "get", 3) == 0)
				{
					printf("COM:GET\n");
					char *resp = get(str(node->array[1]->bulk, node->array[1]->size));
					if (resp == NULL)
					{
						strncpy(buf, "$-1\r\n", 1024);
					}
					else
					{
						printf("GET[%s]", resp);
						bulk(buf, resp, strlen(resp));
						// strncpy(buf, "$-1\r\n", 1024);
					}
				}
				write(events[i].data.fd, buf, strlen(buf));
			}
		}
	}
	close(server_fd);
	// TODO close all connections

	return 0;
}
