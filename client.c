#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

#define SERV_ADDR "34.254.242.81"
#define MAX_LEN 256
#define MAX_DATA 4096
#define LINELEN 1000

int sockfd;
pthread_mutex_t conn_mutex = PTHREAD_MUTEX_INITIALIZER;
char *token_jwt = NULL;
int got_token_jwt = 0;

bool has_spaces(char *str)
{
    // Iterate over each character in the string
    for (int i = 0; i < strlen(str); i++)
    {
        // Check if the current character is a space
        if (str[i] == ' ')
        {
            // If a space is found, return true
            return true;
        }
    }
    
    // If no spaces are found, return false
    return false;
}


bool is_logged_in(char *cookies)
{
    // Check if the provided 'cookies' string is equal to "connect.sid="
    if (strcmp(cookies, "connect.sid=") == 0)
    {
        // If the string is equal to "connect.sid=", return false
        return false;
    }
    
    // If the string is not equal to "connect.sid=", return true
    return true;
}


void *send_dummy_request(void *arg)
{
    // Cast the argument to an integer and assign it to sockfd
    int sockfd = *(int *)arg;
    
    // Declare variables for the message and response
    char *message;
    char *response;

    // Create the GET request message using compute_get_request function
    message = compute_get_request(SERV_ADDR, "/api/v1/dummy", NULL, NULL, 0, NULL);

    // Start an infinite loop
    while (1)
    {
        // Wait for 4 seconds
        sleep(4);

        // Lock the mutex before using the shared sockfd
        pthread_mutex_lock(&conn_mutex);

        // Send the message to the server using send_to_server function
        send_to_server(sockfd, message);
        
        // Receive the response from the server using receive_from_server function
        response = receive_from_server(sockfd);

        // Check if the response contains "HTTP/1.1 200 OK"
        if (strstr(response, "HTTP/1.1 200 OK") == NULL)
        {
            // Print a failure message if the response doesn't contain the expected string
            printf("Connection failed!\n");
        }

        // Unlock the mutex when we're done using the shared sockfd
        pthread_mutex_unlock(&conn_mutex);
    }

    return NULL;
}


void erase_newline_character(char *str, size_t len) {
    // Calculate the length of the string
    len = strlen(str);
    
    // Check if the length is greater than 0 and the last character is a newline character
    if (len > 0 && str[len - 1] == '\n')
    {
        str[len - 1] = '\0'; // Remove the trailing newline character by replacing it with the null terminator
    }
}


void parse_json_error(char *response) {
    // Take the error message from the response
    char *aux = strstr(response, "{");
    
    // Parse the JSON string using json_parse_string function
    JSON_Value *root_value = json_parse_string(aux);
    
    // Get the root object from the parsed JSON value
    JSON_Object *root_object = json_value_get_object(root_value);
    
    // Get the error message from the root object
    const char *error_message = json_object_get_string(root_object, "error");

    // Print the error message
    printf("Error: %s\n", error_message);
}


void *register_req(char *message, char *response, char **cookies, int max_attempts, int attempt_count, bool retry_command) {
    // Declare variables
    bool check_spaces = true;
    char username[LINELEN];
    char password[LINELEN];

    // Initialize variables
    attempt_count = 0;
    retry_command = false;

    // Loop for username input
    while (attempt_count < max_attempts)
    {
        printf("username=");
        fflush(stdout);
        if (fgets(username, sizeof(username), stdin) == NULL)
        {
            break; // Exit if there is no input
        }

        erase_newline_character(username, strlen(username));

        check_spaces = has_spaces(username);
        if (check_spaces)
        {
            fprintf(stderr, "Username cannot contain spaces!\n");
            attempt_count++;
            continue;
        }

        break; // Exit the loop if username is valid
    }

    if (attempt_count >= max_attempts)
    {
        fprintf(stderr, "Exceeded maximum attempts for username input! You must re-enter the command!\n");
        retry_command = true;
    }

    // Continue only if the command does not need to be retried
    if (!retry_command)
    {
        attempt_count = 0;

        // Loop for password input
        while (attempt_count < max_attempts)
        {
            printf("password=");
            fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(password, strlen(password));

            check_spaces = has_spaces(password);
            if (check_spaces)
            {
                fprintf(stderr, "Password cannot contain spaces!\n");
                attempt_count++;
                continue;
            }
            break; // Exit the loop if password is valid
        }

        if (attempt_count >= max_attempts)
        {
            fprintf(stderr, "Exceeded maximum attempts for password input! You must re-enter the command!\n");
            retry_command = true;
        }
    }

    // Check if the user is already logged in
    if (is_logged_in(*cookies))
    {
        fprintf(stderr, "Please log out before registering a new account!\n");
        retry_command = true;
    }

    // Proceed with registration if no retry is needed and the user is not logged in
    if (!retry_command && !is_logged_in(*cookies))
    {
        // Create a JSON object and set the username and password
        JSON_Value *root_value = json_value_init_object();
        JSON_Object *root_object = json_value_get_object(root_value);
        json_object_set_string(root_object, "username", username);
        json_object_set_string(root_object, "password", password);
        char *serialized_string = json_serialize_to_string_pretty(root_value);

        // Create the POST request message for registration
        message = compute_post_request(SERV_ADDR, "/api/v1/tema/auth/register", "application/json", &serialized_string, 1, NULL, 0, NULL);
        
        // Lock the mutex before sending the request
        pthread_mutex_lock(&conn_mutex);
        send_to_server(sockfd, message);
        response = receive_from_server(sockfd);
        pthread_mutex_unlock(&conn_mutex);

        // Check the response for successful registration
        if (strstr(response, "HTTP/1.1 201 Created") != NULL)
        {
            printf("Successfully registered!\n");
        }
        else
        {
            parse_json_error(response);
        }

        // Free memory and resources
        free(message);
        free(response);
        json_free_serialized_string(serialized_string);
        json_value_free(root_value);
    }
}



void *exit_req(char** cookies) {
    // Remove the trailing part of the cookie value by adding a null terminator
    cookies[0][strlen("connect.sid=")] = '\0';
    
    // Free the memory allocated for the cookie value
    free(cookies[0]);

    // Reset the token and flag variables
    token_jwt = NULL;
    got_token_jwt = 0;
    
    // Close the connection
    close_connection(sockfd);
    
    // Exit the program
    exit(0);
}


void *login_req(char *message, char *response, char **cookies, int max_attempts, int attempt_count, bool retry_command){
    // Declare variables
    bool check_spaces = true;
    char username[LINELEN];
    char password[LINELEN];

    // Initialize variables
    attempt_count = 0;
    retry_command = false;

    // Loop for username input
    while (attempt_count < max_attempts)
    {
        printf("username=");
        fflush(stdout);
        if (fgets(username, sizeof(username), stdin) == NULL)
        {
            break; // Exit if there is no input
        }

        erase_newline_character(username, strlen(username));

        check_spaces = has_spaces(username);
        if (check_spaces)
        {
            fprintf(stderr, "Username cannot contain spaces!\n");
            attempt_count++;
            continue;
        }

        break; // Exit the loop if username is valid
    }

    if (attempt_count >= max_attempts)
    {
        fprintf(stderr, "Exceeded maximum attempts for username input! You must re-enter the command!\n");
        retry_command = true;
    }

    // Continue only if the command does not need to be retried
    if (!retry_command)
    {
        attempt_count = 0;

        // Loop for password input
        while (attempt_count < max_attempts)
        {
            printf("password=");
            fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(password, strlen(password));

            check_spaces = has_spaces(password);
            if (check_spaces)
            {
                fprintf(stderr, "Password cannot contain spaces!\n");
                attempt_count++;
                continue;
            }
            break; // Exit the loop if password is valid
        }

        if (attempt_count >= max_attempts)
        {
            fprintf(stderr, "Exceeded maximum attempts for password input! You must re-enter the command!\n");
            retry_command = true;
        }
    }

    // Check if the user is already logged in
    if (is_logged_in(*cookies))
    {
        fprintf(stderr, "You are already logged in!\n");
        retry_command = true;
    }

    // Proceed with login if no retry is needed and the user is not logged in
    if (!retry_command)
    {
        // Create a JSON object and set the username and password
        JSON_Value *root_value = json_value_init_object();
        JSON_Object *root_object = json_value_get_object(root_value);
        json_object_set_string(root_object, "username", username);
        json_object_set_string(root_object, "password", password);
        char *serialized_string = json_serialize_to_string_pretty(root_value);

        // Create the POST request message for login
        message = compute_post_request(SERV_ADDR, "/api/v1/tema/auth/login", "application/json", &serialized_string, 1, NULL, 0, NULL);

        // Lock the mutex before sending the request
        pthread_mutex_lock(&conn_mutex);
        send_to_server(sockfd, message);
        response = receive_from_server(sockfd);
        pthread_mutex_unlock(&conn_mutex);

        // Check the response for successful login
        if (strstr(response, "HTTP/1.1 200 OK") != NULL)
        {
            printf("Successfully logged in!\n");

            // Extract the cookie value from the response
            char *cookie = strstr(response, "connect.sid=");
            char *cookie_end = strstr(cookie, "; Path");

            // Update the cookie value in the cookies array
            snprintf(cookies[0] + strlen("connect.sid="),
                        strlen(cookie) - strlen(cookie_end) - strlen("connect.sid=") + 1, "%s", cookie + strlen("connect.sid="));
        }
        else
        {
            // Take the error message from the response
            parse_json_error(response);
        }

        // Free memory and resources
        free(message);
        free(response);
        json_free_serialized_string(serialized_string);
        json_value_free(root_value);
    }
}


void *enter_library_req(char **cookies, char *message, char *response) {
    // Create the GET request message for accessing the library
    message = compute_get_request(SERV_ADDR, "/api/v1/tema/library/access", NULL, cookies, 1, NULL);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);
    // printf("%s\n", response);

    // Check the response for successful access
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);

        // Free memory and return NULL
        free(message);
        free(response);
        
        return NULL;
    }

    // Extract the token from the response and store it
    char *aux = strstr(response, "{");
    JSON_Value *root_value = json_parse_string(aux);
    JSON_Object *root_object = json_value_get_object(root_value);
    token_jwt = (char *)json_object_get_string(root_object, "token");
    got_token_jwt = 1; // If the response contains a JWT token, store it for later use

    // Print success message
    printf("Successfully entered the library!\n");

    // Free memory
    free(message);
    free(response);
}

void *get_books_req(char **cookies, char *message, char *response) {
    // Create the GET request message for retrieving books from the library
    message = compute_get_request(SERV_ADDR, "/api/v1/tema/library/books", NULL, cookies, 1, token_jwt);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);

    // Check the response for successful retrieval of books
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);

        // Free memory and return NULL
        free(message);
        free(response);
        
        return NULL;
    }

    // Parse the response to get the books
    int req_id;
    sscanf(response, "%*s %d", &req_id);

    if (req_id / 100 == 2)
    {
        // Find the starting point of the body of the response
        char *aux = strstr(response, "[");
        JSON_Value *recv_body = json_parse_string(aux);

        // Parse the JSON array received from the server
        JSON_Array *recv_array = json_value_get_array(recv_body);
        int array_count = json_array_get_count(recv_array);

        if (array_count < 1)
        {
            // If there are no books in the library, print an error message
            printf("There are no books in the library!\n");
        }
        else
        {
            // Otherwise, print the books retrieved from the JSON array
            printf("These are the books in the library: \n");

            for (int i = 0; i < array_count; i++)
            {
                JSON_Object *book = json_array_get_object(recv_array, i);
                double id = json_object_get_number(book, "id");
                const char *title = json_object_get_string(book, "title");

                printf("{\n \"id\": %0.0f\n \"title\": %s}\n", id, title);
            }
        }

        // Free memory
        json_value_free(recv_body);
        free(message);
        free(response);
    }
}

void *get_book_req(char **cookies, char *message, char *response) {
    // Read the book ID from the user
    printf("id=");
    fflush(stdout);
    char id[LINELEN];
    if (fgets(id, sizeof(id), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }

    erase_newline_character(id, strlen(id));

    // Check if the ID is a number and if it contains spaces
    while (1)
    {
        if (atoi(id) == 0 || id == '\0')
        {
            fprintf(stderr, "Id must be a number!\n");
            printf("id=");
            fflush(stdout);
            if (fgets(id, sizeof(id), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(id, strlen(id));
        }
        else if (has_spaces(id))
        {
            fprintf(stderr, "Id cannot contain spaces!\n");
            printf("id=");
            fflush(stdout);
            if (fgets(id, sizeof(id), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(id, strlen(id));
        }
        else
        {
            break;
        }
    }

    // Create the URL for the specific book based on the ID
    char url[LINELEN];
    sprintf(url, "/api/v1/tema/library/books/%s", id);

    // Create the GET request message for retrieving the specific book
    message = compute_get_request(SERV_ADDR, url, NULL, cookies, 1, token_jwt);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);
    // printf("%s\n", response);

    // Check the response for successful retrieval of the book
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);
    }
    else
    {
        // Find the starting point of the body of the response
        char *aux = strstr(response, "{");
        JSON_Value *recv_body = json_parse_string(aux);

        // Get the JSON object representing the book
        JSON_Object *book = json_value_get_object(recv_body);
        int book_count = json_object_get_count(book);

        // Print the book information
        printf("{\n");
        for (int i = 0; i < book_count; i++)
        {
            const char *key = json_object_get_name(book, i);
            JSON_Value *value = json_object_get_value(book, key);

            if (json_value_get_type(value) == JSONString)
            {
                printf(" \"%s\": \"%s\"\n", key, json_value_get_string(value));
            }
            else if (json_value_get_type(value) == JSONNumber)
            {
                printf(" \"%s\": %0.0f\n", key, json_value_get_number(value));
            }
        }

        printf("}\n");

        // Free memory
        json_value_free(recv_body);
    }

    // Free memory
    free(message);
    free(response);
}


void *delete_book(char **cookies, char *message, char *response) {
    // Read the book ID from the user
    printf("id=");
    fflush(stdout);
    char id[LINELEN];
    if (fgets(id, sizeof(id), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }

    erase_newline_character(id, strlen(id));

    // Check if the ID is a number and if it contains spaces
    while (1)
    {
        if (atoi(id) == 0 || id == '\0')
        {
            fprintf(stderr, "Id must be a number!\n");
            printf("id=");
            fflush(stdout);
            if (fgets(id, sizeof(id), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(id, strlen(id));
        }
        else if (has_spaces(id))
        {
            fprintf(stderr, "Id cannot contain spaces!\n");
            printf("id=");
            fflush(stdout);
            if (fgets(id, sizeof(id), stdin) == NULL)
            {
                break; // Exit if there is no input
            }

            erase_newline_character(id, strlen(id));
        }
        else
        {
            break;
        }
    }

    // Create the URL for the specific book based on the ID
    char url[LINELEN];
    sprintf(url, "/api/v1/tema/library/books/%s", id);

    // Create the DELETE request message for deleting the specific book
    message = compute_delete_request(SERV_ADDR, url, NULL, cookies, 1, token_jwt);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);

    // Check the response for successful deletion of the book
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);
    }
    else
    {
        printf("Successfully deleted book!\n");
    }

    // Free memory
    free(message);
    free(response);
}


void *add_book(char **cookies, char *message, char *response) {
    // Create a JSON object for the new book
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root_object = json_value_get_object(root_value);

    // Read the title from the user
    char title[LINELEN];
    printf("title=");
    fflush(stdout);
    if (fgets(title, sizeof(title), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }
    erase_newline_character(title, strlen(title));
    json_object_set_string(root_object, "title", title);

    // Read the author from the user
    char author[LINELEN];
    printf("author=");
    fflush(stdout);
    if (fgets(author, sizeof(author), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }
    erase_newline_character(author, strlen(author));
    json_object_set_string(root_object, "author", author);

    // Read the genre from the user
    char genre[LINELEN];
    printf("genre=");
    fflush(stdout);
    if (fgets(genre, sizeof(genre), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }
    erase_newline_character(genre, strlen(genre));
    // Check if it contains digits
    while (1)
    {
        if (atoi(genre) != 0)
        {
            fprintf(stderr, "Genre cannot contain digits!\n");
            printf("genre=");
            fflush(stdout);
            if (fgets(genre, sizeof(genre), stdin) == NULL)
            {
                break; // Exit if there is no input
            }
            erase_newline_character(genre, strlen(genre));
        }
        else
        {
            break;
        }
    }
    json_object_set_string(root_object, "genre", genre);

    // Read the publisher from the user
    char publisher[LINELEN];
    printf("publisher=");
    fflush(stdout);
    if (fgets(publisher, sizeof(publisher), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }
    erase_newline_character(publisher, strlen(publisher));
    json_object_set_string(root_object, "publisher", publisher);

    // Read the page count from the user
    char page_count[LINELEN];
    printf("page_count=");
    fflush(stdout);
    if (fgets(page_count, sizeof(page_count), stdin) == NULL)
    {
        return NULL; // Exit if there is no input
    }
    erase_newline_character(page_count, strlen(page_count));
    // Check if it is a valid number and doesn't contain spaces
    while (1)
    {
        if (!atoi(page_count) || page_count[0] == '\0')
        {
            fprintf(stderr, "Page count must be a number!\n");
            printf("page_count=");
            fflush(stdout);
            if (fgets(page_count, sizeof(page_count), stdin) == NULL)
            {
                break; // Exit if there is no input
            }
            erase_newline_character(page_count, strlen(page_count));
        }
        else if (has_spaces(page_count))
        {
            fprintf(stderr, "Page count cannot contain spaces!\n");
            printf("page_count=");
            fflush(stdout);
            if (fgets(page_count, sizeof(page_count), stdin) == NULL)
            {
                break; // Exit if there is no input
            }
            erase_newline_character(page_count, strlen(page_count));
        }
        else
        {
            break;
        }
    }
    json_object_set_string(root_object, "page_count", page_count);

    // Create the URL for the POST request to add the new book
    char url[LINELEN];
    sprintf(url, "/api/v1/tema/library/books");

    // Serialize the JSON object to a string
    char *serialized_string = json_serialize_to_string_pretty(root_value);

    // Create the POST request message to add the new book
    message = compute_post_request(SERV_ADDR, url, "application/json", &serialized_string, 1, cookies, 1, token_jwt);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);

    // Check the response for successful addition of the book
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);
    }
    else
    {
        printf("Successfully added book!\n");
    }

    // Free memory
    free(message);
    free(response);
    json_free_serialized_string(serialized_string);
    json_value_free(root_value);
}

void *logout(char **cookies, char *message, char *response) {
    // Create the GET request message for logging out
    message = compute_get_request(SERV_ADDR, "/api/v1/tema/auth/logout", NULL, cookies, 1, NULL);

    // Lock the mutex before sending the request
    pthread_mutex_lock(&conn_mutex);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    pthread_mutex_unlock(&conn_mutex);

    // Check the response for successful logout
    if (strstr(response, "HTTP/1.1 200 OK") == NULL)
    {
        // Take the error message from the response
        parse_json_error(response);
    }
    else
    {
        printf("Successfully logged out!\n");
    }

    // Reset the cookie to clear the session
    sprintf(cookies[0], "%s", "connect.sid=");
    cookies[0][strlen("connect.sid=")] = '\0';

    // Reset token
    got_token_jwt = 0;
    token_jwt = NULL;

    // Free memory
    free(message);
    free(response);
}

int main(int argc, char *argv[])
{
    char *message;  // Variable to store request message
    char *response;  // Variable to store response message
    char *cookies[1];  // Array to store cookies
    cookies[0] = (char *)calloc(256, sizeof(char));  // Allocate memory for the cookie
    sprintf(cookies[0], "%s", "connect.sid=");  // Initialize cookie with a default value
    pthread_t tid;
    size_t len;

    int attempt_count = 0;  // Counter for retry attempts
    int max_attempts = 5;  // Maximum number of retry attempts
    bool retry_command = false;  // Flag to indicate if a command should be retried

    sockfd = open_connection(SERV_ADDR, 8080, AF_INET, SOCK_STREAM, 0);  // Open connection to server

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, send_dummy_request, &sockfd) != 0)  // Create a thread to send dummy requests
    {
        fprintf(stderr, "Could not create thread to send dummy requests\n");
        return 1;
    }

    while (1)
    {
        char line[LINELEN];

        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;  // Exit if there is no input
        }

        erase_newline_character(line, strlen(line));  // Remove newline character from input

        // Check the user input and execute the corresponding command
        if (strcmp(line, "register") == 0)
        {
            register_req(message, response, cookies, max_attempts, attempt_count, retry_command);  // Send register request
        }
        else if (strcmp(line, "exit") == 0)
        {
            exit_req(cookies);  // Send exit request
        }
        else if (strcmp(line, "login") == 0)
        {
            login_req(message, response, cookies, max_attempts, attempt_count, retry_command);  // Send login request
        }
        else if (strcmp(line, "enter_library") == 0)
        {
            enter_library_req(cookies, message, response);  // Send enter_library request
        }
        else if (strcmp(line, "get_books") == 0)
        {
            get_books_req(cookies, message, response);  // Send get_books request
        }
        else if (strcmp(line, "get_book") == 0)
        {
            get_book_req(cookies, message, response);  // Send get_book request
        }
        else if (strcmp(line, "add_book") == 0)
        {
            add_book(cookies, message, response);  // Send add_book request
        }
        else if (strcmp(line, "delete_book") == 0)
        {
            delete_book(cookies, message, response);  // Send delete_book request
        }
        else if (strcmp(line, "logout") == 0)
        {
            logout(cookies, message, response);  // Send logout request
        }
        else
        {
            fprintf(stderr, "Invalid command!\n");  // Print error for invalid command
        }

        retry_command = false;  // Reset retry flag
    }

    pthread_mutex_destroy(&conn_mutex);  // Destroy the connection mutex
    return 0;
}