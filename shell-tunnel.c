#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libgen.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SERVER_PATH "/tmp/shell-tunnel-socket"

enum {MODE_UNDEF, MODE_DAEMON, MODE_CLIENT};

static void server_mode(void);
static void echo(int sockfd);
static void client_mode(void);
static void client_test(int sockfd);
static void print_usage(const char *name);

int main(int argc, char *argv[])
{
	int i;
	int mode = MODE_UNDEF;

	/* parse command line */
	for (i = 1; i < argc; ++i) {
		if (!strcmp("--daemon", argv[i]))
			mode = MODE_DAEMON;
		else if (!strcmp("--client", argv[i]))
			mode = MODE_CLIENT;
	}

	switch (mode)
	{
	case MODE_DAEMON:
		server_mode();
		break;
	case MODE_CLIENT:
		client_mode();
		break;
	default:
		print_usage(basename(argv[0]));
	}

	return 0;
}

static void print_usage(const char *name)
{
	printf("Usage: \n");
	printf("%s --daemon \n", name);
	printf("%s --client \n", name);
	exit(1);
}

/*
 * Server side
 */
void server_mode(void)
{
	int err;
	int sockfd;
	int new_sockfd;
	struct sockaddr_un serveraddr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("could not open socket");
		return;
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, SERVER_PATH);

	err = bind(sockfd, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr));
	if (err < 0) {
		perror("could not bind to socket");
		goto out_err_1;
	}

	err = listen(sockfd, 1);
	if (err < 0) {
		perror("could not listen to socket");
		goto out_err_1;
	}

	/* --- fork() --- */
	new_sockfd = accept(sockfd, NULL, NULL);
	if (new_sockfd < 0) {
		perror("could not accept connection");
		goto out_err_1;
	}

	/* business logic */
	echo(new_sockfd);

	close(new_sockfd);

out_err_1:
	close(sockfd);
	unlink(SERVER_PATH);
}

static void echo(int sockfd)
{
	char buff;
	int len;

	while (true) {
		len = recv(sockfd, &buff, 1, 0);
		if (len == 0)
			break;

		/* processing */
		if ((buff >= 'a') && (buff <= 'z'))
			buff += 'A' - 'a';

		send(sockfd, &buff, 1, 0);
	}
}

/*
 * Client side
 */
void client_mode(void)
{
	int err;
	int sockfd;
	struct sockaddr_un serveraddr;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("could not open socket");
		goto out_err_1;
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strcpy(serveraddr.sun_path, SERVER_PATH);

	err = connect(sockfd, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr));
	if (err < 0) {
		perror("could not connect to socket");
		goto out_err_2;
	}

	client_test(sockfd);
	shutdown(sockfd, SHUT_RDWR);

out_err_2:
	close(sockfd);

out_err_1:
	putchar('\n');
}

static void client_test(int sockfd)
{
	int i;
	const char outbuff[] = "hello world";
	char inbuff;

	for (i = 0; i < strlen(outbuff); ++i) {
		send(sockfd, &outbuff[i], 1, 0);
		recv(sockfd, &inbuff, 1, 0);
		putchar(inbuff);
	}
}

