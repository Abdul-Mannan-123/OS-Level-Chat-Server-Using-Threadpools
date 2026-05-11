#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include "data_structs.h"
#include "socket_utils.h"

#define WORKER_COUNT 4
#define MAX_CLIENTS 128
#define MAX_NAME_LENGTH 32
#define MAX_ROOM_LENGTH 32
#define MAX_MESSAGE_LENGTH BUFFER_SIZE
#define DEFAULT_ROOM "lobby"
#define RATE_LIMIT_WINDOW_SECONDS 5
#define RATE_LIMIT_MAX_MESSAGES 12

struct clientConnection
{
	int socket_fd;
	char name[MAX_NAME_LENGTH];
	char room[MAX_ROOM_LENGTH];
	bool authenticated;
	time_t window_start;
	int message_count;
	struct clientConnection* next;
};

struct chatRoom
{
	char name[MAX_ROOM_LENGTH];
	struct messageQueue history;
	pthread_mutex_t history_mutex;
	struct chatRoom* next;
};

static struct clientConnection* head;
static struct clientConnection* tail;
static struct chatRoom* room_head;
static const char* server_auth_token;
static bool authentication_required;
static FILE* log_file;

int client_task_queue[QUEUE_SIZE];
int task_front;
int task_rear;

sem_t queue_mutex;
sem_t empty;
sem_t full;

pthread_mutex_t clients_mutex;
pthread_mutex_t rooms_mutex;
pthread_mutex_t log_mutex;

static void log_event (const char* format, ...);
static int write_all (int socket_fd, const char* buffer);
static void send_client_message (int socket_fd, const char* message);
static void send_system_message (int socket_fd, const char* format, ...);
static void copy_text (char* destination, size_t destination_size, const char* source);
static void append_text (char* destination, size_t destination_size, const char* text);
static const char* skip_spaces (const char* text);
static void trim_newline (char* text);
static struct clientConnection* find_client_by_fd_locked (int socket_fd);
static struct clientConnection* find_client_by_name_locked (const char* name);
static struct chatRoom* find_room_locked (const char* room_name);
static struct chatRoom* get_or_create_room (const char* room_name);
static void add_message_to_room_history (const char* room_name, const char* message, int socket_fd);
static void send_room_history (int socket_fd, const char* room_name);
static void send_users_in_room (int socket_fd, const char* room_name);
static void send_room_list (int socket_fd);
static bool enforce_rate_limit_locked (int socket_fd);
static void broadcast_to_room (const char* room_name, const char* message);
static void send_private_message (const char* sender_name, int sender_socket_fd, const char* recipient_name, const char* message);
static void handle_client_line (int socket_fd, char* line);
static void process_client_socket (int client_socket_fd);

void program_variables_init();
void enqueue_client_socket (int client_socket_fd);
int dequeue_client_socket ();
void add_client_socket (int client_socket_fd);
void remove_client_socket (int client_socket_fd);
void* worker_thread (void* arg);

int main()
{
	program_variables_init();
	server_auth_token = getenv("CHAT_SERVER_TOKEN");
	authentication_required = server_auth_token != NULL && server_auth_token[0] != '\0';
	log_file = fopen("chat_server.log", "a");
	get_or_create_room(DEFAULT_ROOM);

	int socket_fd = createTCPIpv4Socket();
	struct sockaddr* address = createTCPIpv4SocketAddress("", 2000);

	if (bind(socket_fd, address, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind failed");
		exit(1);
	}
	printf("Socket Bound Successfully\n");

	if (listen(socket_fd, 10) < 0)
	{
		perror("listen failed");
		exit(1);
	}
	log_event("Server listening on port 2000");

	pthread_t workers[WORKER_COUNT];
	for (int i = 0; i < WORKER_COUNT; i++)
		pthread_create(&workers[i], NULL, worker_thread, NULL);

	while (true)
	{
		struct sockaddr clientAddress;
		socklen_t clientAddressSize = sizeof(clientAddress);

		int client_socket_fd = accept(socket_fd, &clientAddress, &clientAddressSize);
		if (client_socket_fd < 0)
			continue;

		add_client_socket(client_socket_fd);
		enqueue_client_socket(client_socket_fd);
	}

	shutdown(socket_fd, SHUT_RDWR);
	sem_destroy(&queue_mutex);
	sem_destroy(&empty);
	sem_destroy(&full);
	pthread_mutex_destroy(&clients_mutex);
	pthread_mutex_destroy(&rooms_mutex);
	pthread_mutex_destroy(&log_mutex);
	if (log_file != NULL)
		fclose(log_file);

	return 0;
}

void program_variables_init()
{
	head = NULL;
	tail = NULL;
	room_head = NULL;
	task_front = 0;
	task_rear = 0;

	sem_init(&queue_mutex, 0, 1);
	sem_init(&empty, 0, QUEUE_SIZE);
	sem_init(&full, 0, 0);
	pthread_mutex_init(&clients_mutex, NULL);
	pthread_mutex_init(&rooms_mutex, NULL);
	pthread_mutex_init(&log_mutex, NULL);
	log_file = NULL;
}

void enqueue_client_socket (int client_socket_fd)
{
	sem_wait(&empty);
	sem_wait(&queue_mutex);

	client_task_queue[task_rear] = client_socket_fd;
	task_rear = (task_rear + 1) % QUEUE_SIZE;

	sem_post(&queue_mutex);
	sem_post(&full);
}

int dequeue_client_socket ()
{
	int client_socket_fd;

	sem_wait(&full);
	sem_wait(&queue_mutex);

	client_socket_fd = client_task_queue[task_front];
	task_front = (task_front + 1) % QUEUE_SIZE;

	sem_post(&queue_mutex);
	sem_post(&empty);

	return client_socket_fd;
}

void add_client_socket (int client_socket_fd)
{
	struct clientConnection* node = (struct clientConnection*) malloc(sizeof(struct clientConnection));
	node->socket_fd = client_socket_fd;
	snprintf(node->name, sizeof(node->name), "guest-%d", client_socket_fd);
	strncpy(node->room, DEFAULT_ROOM, sizeof(node->room) - 1);
	node->room[sizeof(node->room) - 1] = '\0';
	node->authenticated = !authentication_required;
	node->window_start = time(NULL);
	node->message_count = 0;
	node->next = NULL;

	pthread_mutex_lock(&clients_mutex);

	if (head == NULL)
	{
		head = node;
		tail = node;
	}
	else
	{
		tail->next = node;
		tail = node;
	}

	pthread_mutex_unlock(&clients_mutex);

	if (authentication_required)
		send_system_message(client_socket_fd, "Connected. Authenticate with /auth <token>, then set /nick <name> and /join <room>");
	else
		send_system_message(client_socket_fd, "Connected. Use /nick <name> and /join <room>");

	log_event("client connected fd=%d", client_socket_fd);
}

void remove_client_socket (int client_socket_fd)
{
	struct clientConnection* current;
	struct clientConnection* previous;

	pthread_mutex_lock(&clients_mutex);
	current = head;
	previous = NULL;

	while (current != NULL)
	{
		if (current->socket_fd == client_socket_fd)
		{
			if (previous == NULL)
				head = current->next;
			else
				previous->next = current->next;

			if (tail == current)
				tail = previous;

			free(current);
			break;
		}

		previous = current;
		current = current->next;
	}

	pthread_mutex_unlock(&clients_mutex);
}

static void log_event (const char* format, ...)
{
	char timestamp[32];
	time_t now = time(NULL);
	struct tm tm_info;
	struct tm* tm_result = localtime_r(&now, &tm_info);
	if (tm_result != NULL)
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);
	else
		strncpy(timestamp, "unknown-time", sizeof(timestamp) - 1);
	timestamp[sizeof(timestamp) - 1] = '\0';

	pthread_mutex_lock(&log_mutex);
	fprintf(stdout, "[%s] ", timestamp);

	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);

	fprintf(stdout, "\n");
	fflush(stdout);

	if (log_file != NULL)
	{
		fprintf(log_file, "[%s] ", timestamp);
		va_start(args, format);
		vfprintf(log_file, format, args);
		va_end(args);
		fprintf(log_file, "\n");
		fflush(log_file);
	}

	pthread_mutex_unlock(&log_mutex);
}

static int write_all (int socket_fd, const char* buffer)
{
	size_t total_written = 0;
	size_t buffer_length = strlen(buffer);

	while (total_written < buffer_length)
	{
		ssize_t written = write(socket_fd, buffer + total_written, buffer_length - total_written);
		if (written <= 0)
			return -1;
		total_written += (size_t) written;
	}

	return 0;
}

static void send_client_message (int socket_fd, const char* message)
{
	if (message == NULL)
		return;

	write_all(socket_fd, message);
	size_t message_length = strlen(message);
	if (message_length == 0 || message[message_length - 1] != '\n')
		write_all(socket_fd, "\n");
}

static void send_system_message (int socket_fd, const char* format, ...)
{
	char message[MAX_MESSAGE_LENGTH];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	send_client_message(socket_fd, message);
}

static void copy_text (char* destination, size_t destination_size, const char* source)
{
	if (destination == NULL || destination_size == 0)
		return;

	if (source == NULL)
		source = "";

	snprintf(destination, destination_size, "%s", source);
}

static void append_text (char* destination, size_t destination_size, const char* text)
{
	if (destination == NULL || text == NULL || destination_size == 0)
		return;

	size_t used_length = strlen(destination);
	if (used_length >= destination_size - 1)
		return;

	size_t available = destination_size - used_length - 1;
	size_t text_length = strlen(text);
	if (text_length > available)
		text_length = available;

	memcpy(destination + used_length, text, text_length);
	destination[used_length + text_length] = '\0';
}

static const char* skip_spaces (const char* text)
{
	while (*text == ' ' || *text == '\t')
		text++;

	return text;
}

static void trim_newline (char* text)
{
	if (text == NULL)
		return;

	size_t length = strlen(text);
	while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r'))
	{
		text[length - 1] = '\0';
		length--;
	}
}

static struct clientConnection* find_client_by_fd_locked (int socket_fd)
{
	struct clientConnection* current = head;
	while (current != NULL)
	{
		if (current->socket_fd == socket_fd)
			return current;
		current = current->next;
	}

	return NULL;
}

static struct clientConnection* find_client_by_name_locked (const char* name)
{
	struct clientConnection* current = head;
	while (current != NULL)
	{
		if (strcmp(current->name, name) == 0)
			return current;
		current = current->next;
	}

	return NULL;
}

static struct chatRoom* find_room_locked (const char* room_name)
{
	struct chatRoom* current = room_head;
	while (current != NULL)
	{
		if (strcmp(current->name, room_name) == 0)
			return current;
		current = current->next;
	}

	return NULL;
}

static void persist_message_to_file (const char* room_name, const char* message)
{
	char filename[MAX_ROOM_LENGTH + 10];
	snprintf(filename, sizeof(filename), "room_%s.txt", room_name);

	FILE* file = fopen(filename, "a");
	if (file != NULL)
	{
		fprintf(file, "%s\n", message);
		fflush(file);
		fclose(file);
	}
}

static void load_room_history_from_file (const char* room_name, struct messageQueue* queue)
{
	char filename[MAX_ROOM_LENGTH + 10];
	char line[BUFFER_SIZE];
	snprintf(filename, sizeof(filename), "room_%s.txt", room_name);

	FILE* file = fopen(filename, "r");
	if (file == NULL)
		return;

	while (fgets(line, sizeof(line), file) != NULL)
	{
		trim_newline(line);
		if (line[0] != '\0')
			enqueue(queue, line, -1);
	}

	fclose(file);
}

static struct chatRoom* get_or_create_room (const char* room_name)
{
	pthread_mutex_lock(&rooms_mutex);
	struct chatRoom* room = find_room_locked(room_name);
	if (room == NULL)
	{
		room = (struct chatRoom*) malloc(sizeof(struct chatRoom));
		strncpy(room->name, room_name, sizeof(room->name) - 1);
		room->name[sizeof(room->name) - 1] = '\0';
		messageQueue_init(&room->history);
		pthread_mutex_init(&room->history_mutex, NULL);
		room->next = room_head;
		room_head = room;
		
		load_room_history_from_file(room_name, &room->history);
	}
	pthread_mutex_unlock(&rooms_mutex);

	return room;
}

static void add_message_to_room_history (const char* room_name, const char* message, int socket_fd)
{
	struct chatRoom* room = get_or_create_room(room_name);
	pthread_mutex_lock(&room->history_mutex);
	if (enqueue(&room->history, message, socket_fd) != 0) {
		if (room->history.numMessages == QUEUE_SIZE) {
			room->history.front = (room->history.front + 1) % QUEUE_SIZE;
			room->history.numMessages--;
		}
		(void) enqueue(&room->history, message, socket_fd);
	}
	pthread_mutex_unlock(&room->history_mutex);
	persist_message_to_file(room_name, message);
}

static void send_room_history (int socket_fd, const char* room_name)
{
	struct chatRoom* room = get_or_create_room(room_name);
	char history_copy[QUEUE_SIZE][BUFFER_SIZE];
	int history_count = 0;

	pthread_mutex_lock(&room->history_mutex);
	for (int i = 0; i < room->history.numMessages && i < QUEUE_SIZE; i++)
	{
		int index = (room->history.front + i) % QUEUE_SIZE;
		copy_text(history_copy[history_count], BUFFER_SIZE, room->history.messages[index]);
		history_count++;
	}
	pthread_mutex_unlock(&room->history_mutex);

	for (int i = 0; i < history_count; i++)
		send_client_message(socket_fd, history_copy[i]);
}

static void send_users_in_room (int socket_fd, const char* room_name)
{
	char names[MAX_CLIENTS][MAX_NAME_LENGTH];
	int count = 0;

	pthread_mutex_lock(&clients_mutex);
	struct clientConnection* current = head;
	while (current != NULL && count < MAX_CLIENTS)
	{
		if (current->authenticated && strcmp(current->room, room_name) == 0)
		{
			copy_text(names[count], MAX_NAME_LENGTH, current->name);
			count++;
		}
		current = current->next;
	}
	pthread_mutex_unlock(&clients_mutex);

	if (count == 0)
	{
		send_system_message(socket_fd, "Users in %s: none", room_name);
		return;
	}

	char response[MAX_MESSAGE_LENGTH];
	response[0] = '\0';
	append_text(response, sizeof(response), "Users in ");
	append_text(response, sizeof(response), room_name);
	append_text(response, sizeof(response), ": ");
	for (int i = 0; i < count; i++)
	{
		append_text(response, sizeof(response), names[i]);
		if (i + 1 < count)
			append_text(response, sizeof(response), ", ");
	}

	send_client_message(socket_fd, response);
}

static void send_room_list (int socket_fd)
{
	char rooms[MAX_CLIENTS][MAX_ROOM_LENGTH];
	int room_count = 0;

	pthread_mutex_lock(&clients_mutex);
	struct clientConnection* current = head;
	while (current != NULL && room_count < MAX_CLIENTS)
	{
		bool room_exists = false;
		for (int i = 0; i < room_count; i++)
		{
			if (strcmp(rooms[i], current->room) == 0)
			{
				room_exists = true;
				break;
			}
		}

		if (!room_exists)
		{
			copy_text(rooms[room_count], MAX_ROOM_LENGTH, current->room);
			room_count++;
		}
		current = current->next;
	}
	pthread_mutex_unlock(&clients_mutex);

	if (room_count == 0)
	{
		send_system_message(socket_fd, "Rooms: none");
		return;
	}

	char response[MAX_MESSAGE_LENGTH];
	response[0] = '\0';
	append_text(response, sizeof(response), "Rooms: ");
	for (int i = 0; i < room_count; i++)
	{
		append_text(response, sizeof(response), rooms[i]);
		if (i + 1 < room_count)
			append_text(response, sizeof(response), ", ");
	}

	send_client_message(socket_fd, response);
}

static bool enforce_rate_limit_locked (int socket_fd)
{
	struct clientConnection* client = find_client_by_fd_locked(socket_fd);
	if (client == NULL)
		return false;

	time_t now = time(NULL);
	if (client->window_start == 0 || difftime(now, client->window_start) >= RATE_LIMIT_WINDOW_SECONDS)
	{
		client->window_start = now;
		client->message_count = 1;
		return true;
	}

	if (client->message_count >= RATE_LIMIT_MAX_MESSAGES)
		return false;

	client->message_count++;
	return true;
}

static void broadcast_to_room (const char* room_name, const char* message)
{
	int* recipients = NULL;
	size_t recipient_count = 0;

	pthread_mutex_lock(&clients_mutex);
	struct clientConnection* current = head;
	while (current != NULL)
	{
		if (current->authenticated && strcmp(current->room, room_name) == 0)
			recipient_count++;
		current = current->next;
	}

	if (recipient_count > 0)
	{
		recipients = (int*) malloc(sizeof(int) * recipient_count);
		if (recipients != NULL)
		{
			size_t index = 0;
			current = head;
			while (current != NULL)
			{
				if (current->authenticated && strcmp(current->room, room_name) == 0)
				{
					recipients[index] = current->socket_fd;
					index++;
				}
				current = current->next;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);

	if (recipients == NULL)
		return;

	for (size_t i = 0; i < recipient_count; i++)
		send_client_message(recipients[i], message);

	free(recipients);
}

static void send_private_message (const char* sender_name, int sender_socket_fd, const char* recipient_name, const char* message)
{
	int recipient_socket_fd = -1;
	char formatted[MAX_MESSAGE_LENGTH];
	char sender_copy[MAX_NAME_LENGTH];
	char recipient_copy[MAX_NAME_LENGTH];
	char message_copy[MAX_MESSAGE_LENGTH];

	copy_text(sender_copy, sizeof(sender_copy), sender_name);
	copy_text(recipient_copy, sizeof(recipient_copy), recipient_name);
	copy_text(message_copy, sizeof(message_copy), message);

	pthread_mutex_lock(&clients_mutex);
	struct clientConnection* recipient = find_client_by_name_locked(recipient_name);
	if (recipient != NULL)
		recipient_socket_fd = recipient->socket_fd;
	pthread_mutex_unlock(&clients_mutex);

	if (recipient_socket_fd < 0)
	{
		send_system_message(sender_socket_fd, "User not found: %s", recipient_name);
		return;
	}

	formatted[0] = '\0';
	append_text(formatted, sizeof(formatted), "[pm][");
	append_text(formatted, sizeof(formatted), sender_copy);
	append_text(formatted, sizeof(formatted), " -> ");
	append_text(formatted, sizeof(formatted), recipient_copy);
	append_text(formatted, sizeof(formatted), "] ");
	append_text(formatted, sizeof(formatted), message_copy);
	send_client_message(sender_socket_fd, formatted);
	if (recipient_socket_fd != sender_socket_fd)
		send_client_message(recipient_socket_fd, formatted);
	log_event("pm from=%s to=%s message=%s", sender_copy, recipient_copy, message_copy);
}

static void handle_client_line (int socket_fd, char* line)
{
	char sender_name[MAX_NAME_LENGTH];
	char sender_room[MAX_ROOM_LENGTH];
	bool sender_authenticated = false;

	pthread_mutex_lock(&clients_mutex);
	struct clientConnection* client = find_client_by_fd_locked(socket_fd);
	if (client != NULL)
	{
		copy_text(sender_name, sizeof(sender_name), client->name);
		copy_text(sender_room, sizeof(sender_room), client->room);
		sender_authenticated = client->authenticated;
	}
	else
	{
		copy_text(sender_name, sizeof(sender_name), "unknown");
		copy_text(sender_room, sizeof(sender_room), DEFAULT_ROOM);
	}
	pthread_mutex_unlock(&clients_mutex);

	trim_newline(line);
	if (line[0] == '\0')
		return;

	if (strncmp(line, "/help", 5) == 0)
	{
		send_system_message(socket_fd, "Commands: /auth <token>, /nick <name>, /join <room>, /msg <user> <text>, /users, /rooms, /help, /quit");
		return;
	}

	if (strncmp(line, "/auth ", 6) == 0)
	{
		const char* token = skip_spaces(line + 6);
		if (!authentication_required || strcmp(token, server_auth_token) == 0)
		{
			pthread_mutex_lock(&clients_mutex);
			struct clientConnection* current = find_client_by_fd_locked(socket_fd);
			if (current != NULL)
				current->authenticated = true;
			pthread_mutex_unlock(&clients_mutex);

			send_system_message(socket_fd, "Authentication successful");
			log_event("authenticated fd=%d user=%s", socket_fd, sender_name);
		}
		else
		{
			send_system_message(socket_fd, "Authentication failed");
			log_event("authentication failed fd=%d user=%s", socket_fd, sender_name);
		}
		return;
	}

	if (strncmp(line, "/nick ", 6) == 0)
	{
		const char* requested_name = skip_spaces(line + 6);
		bool success = false;
		if (requested_name[0] == '\0')
		{
			send_system_message(socket_fd, "Usage: /nick <name>");
			return;
		}

		pthread_mutex_lock(&clients_mutex);
		struct clientConnection* existing = find_client_by_name_locked(requested_name);
		struct clientConnection* current = find_client_by_fd_locked(socket_fd);
		if (current != NULL && (existing == NULL || existing->socket_fd == socket_fd))
		{
			copy_text(current->name, sizeof(current->name), requested_name);
			success = true;
		}
		pthread_mutex_unlock(&clients_mutex);

		if (success)
		{
			send_system_message(socket_fd, "Nickname set to %s", requested_name);
			log_event("nickname set fd=%d name=%s", socket_fd, requested_name);
		}
		else
		{
			send_system_message(socket_fd, "Nickname unavailable: %s", requested_name);
		}
		return;
	}

	if (strncmp(line, "/join ", 6) == 0)
	{
		const char* requested_room = skip_spaces(line + 6);
		if (requested_room[0] == '\0')
		{
			send_system_message(socket_fd, "Usage: /join <room>");
			return;
		}

		get_or_create_room(requested_room);

		pthread_mutex_lock(&clients_mutex);
		struct clientConnection* current = find_client_by_fd_locked(socket_fd);
		if (current != NULL)
		{
			copy_text(current->room, sizeof(current->room), requested_room);
			copy_text(sender_name, sizeof(sender_name), current->name);
			copy_text(sender_room, sizeof(sender_room), current->room);
		}
		pthread_mutex_unlock(&clients_mutex);

		send_system_message(socket_fd, "Joined room %s", requested_room);
		send_room_history(socket_fd, requested_room);

		char room_notice[MAX_MESSAGE_LENGTH];
		snprintf(room_notice, sizeof(room_notice), "[system][%s] joined %s", sender_name, requested_room);
		broadcast_to_room(requested_room, room_notice);
		log_event("room change fd=%d user=%s to=%s", socket_fd, sender_name, requested_room);
		return;
	}

	if (strcmp(line, "/users") == 0)
	{
		send_users_in_room(socket_fd, sender_room);
		return;
	}

	if (strcmp(line, "/rooms") == 0)
	{
		send_room_list(socket_fd);
		return;
	}

	if (strncmp(line, "/msg ", 5) == 0)
	{
		const char* payload = skip_spaces(line + 5);
		const char* separator = strchr(payload, ' ');
		char recipient[MAX_NAME_LENGTH];
		char message[MAX_MESSAGE_LENGTH];

		if (separator == NULL)
		{
			send_system_message(socket_fd, "Usage: /msg <user> <message>");
			return;
		}

		size_t recipient_length = (size_t) (separator - payload);
		if (recipient_length == 0 || recipient_length >= sizeof(recipient))
		{
			send_system_message(socket_fd, "Usage: /msg <user> <message>");
			return;
		}

		memcpy(recipient, payload, recipient_length);
		recipient[recipient_length] = '\0';
		snprintf(message, sizeof(message), "%s", skip_spaces(separator + 1));
		if (message[0] == '\0')
		{
			send_system_message(socket_fd, "Usage: /msg <user> <message>");
			return;
		}

		send_private_message(sender_name, socket_fd, recipient, message);
		return;
	}

	if (strncmp(line, "/quit", 5) == 0)
	{
		send_system_message(socket_fd, "Goodbye");
		shutdown(socket_fd, SHUT_RDWR);
		return;
	}

	if (authentication_required && !sender_authenticated)
	{
		send_system_message(socket_fd, "Authenticate first with /auth <token>");
		return;
	}

	pthread_mutex_lock(&clients_mutex);
	bool allowed = enforce_rate_limit_locked(socket_fd);
	pthread_mutex_unlock(&clients_mutex);

	if (!allowed)
	{
		send_system_message(socket_fd, "Rate limit exceeded. Please slow down.");
		return;
	}

	char formatted[MAX_MESSAGE_LENGTH];
	snprintf(formatted, sizeof(formatted), "[%s][%s] %s", sender_room, sender_name, line);
	add_message_to_room_history(sender_room, formatted, socket_fd);
	broadcast_to_room(sender_room, formatted);
	log_event("message room=%s user=%s text=%s", sender_room, sender_name, line);
}

static void process_client_socket (int client_socket_fd)
{
	char read_buffer[BUFFER_SIZE];
	char pending[BUFFER_SIZE * 4];
	size_t pending_length = 0;
	pending[0] = '\0';

	while (true)
	{
		memset(read_buffer, 0, sizeof(read_buffer));
		ssize_t bytes_read = read(client_socket_fd, read_buffer, sizeof(read_buffer) - 1);
		if (bytes_read <= 0)
			break;

		read_buffer[bytes_read] = '\0';

		size_t chunk_length = strlen(read_buffer);
		if (pending_length + chunk_length >= sizeof(pending))
		{
			pending_length = 0;
			pending[0] = '\0';
		}

		memcpy(pending + pending_length, read_buffer, chunk_length);
		pending_length += chunk_length;
		pending[pending_length] = '\0';

		char* line_start = pending;
		char* newline = NULL;
		while ((newline = strchr(line_start, '\n')) != NULL)
		{
			*newline = '\0';
			handle_client_line(client_socket_fd, line_start);
			line_start = newline + 1;
		}

		if (line_start != pending)
		{
			size_t remaining = strlen(line_start);
			memmove(pending, line_start, remaining + 1);
			pending_length = remaining;
		}
	}
}

void* worker_thread (void* arg)
{
	(void)arg;

	while (true)
	{
		int client_socket_fd = dequeue_client_socket();
		process_client_socket(client_socket_fd);
		remove_client_socket(client_socket_fd);
		close(client_socket_fd);
		log_event("client disconnected fd=%d", client_socket_fd);
	}

	return NULL;
}
