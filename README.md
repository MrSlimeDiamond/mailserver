# mailserver
A basic mail server written in C.

I'm not particularly good at C, this was mostly done as an excercise to make something which pretends
that it's useful for me to learn it (:

With that said, here is the explaination:

Mailserver uses a Postgres database to store its messages and uses a TCP socket for connectivity.
JSON is sent over the socket and then processed.

Sending messages will broadcast to all other clients connected to the server so they can process them.

## JSON Envolope
Each packet sent to the server should contain a `request_type`.

```json
{
  "request_type": "status"
}
```

If extra data is needed (in most cases!), then use a `data` object
```json
{
  "request_type": "user_id",
  "data": {
    "name": "test"
  }
}
```

## Example Requests
### Message Submit
Sent by the client to submit a message to the database.
```json
{
  "request_type": "submit_message",
  "data": {
    "from": {
      "name": "Findlay"
    },
    "to": {
      "name": "Findlay2"
    },
    "content": "Hello World"
  }
}
```

### Get Next
Used by the client to get the next message in someone's inbox
```json
{
  "request_type": "get_next",
  "data": {
    "user": {
      "name": "Findlay2"
    }
  },
  "mark_read": true
}
```

### Get Messages
Used by the client to receive a list of messages in a user's inbox.
```json
{
  "request_type": "get_messages",
  "data": {
    "user": {
      "name": "Findlay2"
    }
  }
}
```

## Example Responses
Responses will contain the `requst_type` and the relevant user so clients can track them.

### Message Submit
This is broadcast to all connected clients when a message is submitted.
```json
{
  "request_type": "submit_message",
  "data": {
    "from": {
      "name": "Findlay",
      "id": 9
    },
    "to": {
      "name": "Findlay2",
      "id": 13
    },
    "content": "Hello World",
    "timestamp": 1765967769,
    "id": 4,
    "read": false
  },
  "status": 200
}
```

### Get Messages
```json
{
  "request_type": "get_messages",
  "data": {
    "user": {
      "name": "Findlay2"
    },
    "messages": [
      {
        "id": 1,
        "timestamp": 0,
        "from": {
          "id": 9,
          "name": "Findlay"
        },
        "to": {
          "id": 13,
          "name": "Findlay2"
        },
        "content": "Hello World",
        "read": false
      },
      {
        "id": 2,
        "timestamp": 0,
        "from": {
          "id": 9,
          "name": "Findlay"
        },
        "to": {
          "id": 13,
          "name": "Findlay2"
        },
        "content": "Hello World",
        "read": false
      },
      {
        "id": 3,
        "timestamp": 0,
        "from": {
          "id": 9,
          "name": "Findlay"
        },
        "to": {
          "id": 13,
          "name": "Findlay2"
        },
        "content": "Hello World",
        "read": false
      }
    ]
  },
  "status": 200
}
```