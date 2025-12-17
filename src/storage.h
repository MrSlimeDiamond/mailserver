#pragma once

#include "message.h"

/**
 * Open the database
*/
int storage_open(void);

/**
 * Close the database
*/
void storage_close(void);

/**
 * Get the ID of a user, or insert a user for that ID
 * if it doesn't exist
 * 
 * @return User ID, or -1 if unavailable
*/
int get_user_id(char *name);

/**
 * Get a user's name from their user ID
 * 
 * @return User name from ID, or NULL if not found.
*/
char *get_user_name(int id);

/**
 * Submit a message into the database
 * 
 * @return The generated ID of the message, or -1 if it was not successful
*/
int submit_message(message message);

/**
 * Get all of the messages that a user has
 * 
 * @return Array of messages
*/
message *get_messages(user user, bool read_messages_only, int *out_count);

/**
 * Get the next unread message sent to a user
 * 
 * @return A pointer to a message
*/
message *get_next(user user, bool mark_read);

/**
 * Mark a message as read
 * 
 * @return Whether it was successful
*/
int mark_read(int message_id);