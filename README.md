// Copyright Spinochi Andreea 322CA

The code is designed to interact with a server API for performing various operations related to a library system.
The main program allows users to register, login, access the library, view books, add books, delete books, and logout.

The program supports the following commands:

```register```: Allows a user to register a new account in the library system.
```exit```: Logs out from the library system and terminates the program.
```login```: Logs in to the library system using existing account credentials.
```enter_library```: Grants access to the library system after successful login.
```get_books```: Retrieves and displays a list of books available in the library.
```get_book```: Retrieves and displays details of a specific book by providing its ID.
```add_book```: Adds a new book to the library system by providing details such as title, author, genre, publisher, and page count.
```delete_book```: Deletes a book from the library system by providing its ID.
```logout```: Logs out from the library system.

Implementation Details

The program uses multiple helper functions and follows a modular approach for handling different API requests and responses. It establishes a connection with the server, sends HTTP requests, receives responses, and performs appropriate actions based on the responses.

The main program utilizes multithreading to send dummy requests to the server at regular intervals to keep the connection alive.
For reading user input, the program uses a separate thread that waits for user input and sends the input to the main thread for processing.
Then the main thread sends the appropriate request to the server and receives a response. The response is then processed and displayed to the user.

The program uses the parson library for parsing JSON responses from the server. The library is included in the project as a header file.
It is a helper library for parsing JSON data from the server and extracting the required information and storing it in a JSON object.
It also provides functions for accessing the data stored in the JSON object.

Error handling is implemented for invalid commands, input validation, and server responses. Error messages are displayed when necessary.

Note:
The checker need to be run with the --debug option to see the titles of the books. I also implemented in the login and register function a limit of 
5 wrong attempts. If the user fails to login/register 5 times, the command needs to be re-entered.