#include "query_language.pb-c.h"
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

struct rethinkdb_connection {
        char *addr;
        unsigned short port;
	int timeout;
        // the socket
        int s;
        // version sent ?
        int version_sent;
        // buffer for the query
        void *query_buf;
        // buffer for the response
        void *response_buf;
        // token
        int64_t token;
};


int rethinkdb_send(struct rethinkdb_connection *, void *, size_t);
int rethinkdb_connect(struct rethinkdb_connection *);
int rethinkdb_send_version(struct rethinkdb_connection *);
int rethinkdb_check(struct rethinkdb_connection *);
int rethinkdb_send_protobuf(struct rethinkdb_connection *, size_t);
int rethinkdb_send_query(struct rethinkdb_connection *, Query *);
int rethinkdb_recv(struct rethinkdb_connection *, void *, size_t);
int32_t rethinkdb_response_size(struct rethinkdb_connection *);
Response *rethinkdb_response(struct rethinkdb_connection *);
int rethinkdb_response_check(struct rethinkdb_connection *, int);
char **rethinkdb_response_list(struct rethinkdb_connection *, unsigned int *);
char *rethinkdb_response_json(struct rethinkdb_connection *);
int rethinkdb_create_db(struct rethinkdb_connection *, char *);
int rethinkdb_create_table(struct rethinkdb_connection *, char *, char *, char *, char *, int);
char **rethinkdb_list_db(struct rethinkdb_connection *, unsigned int *);
char **rethinkdb_table(struct rethinkdb_connection *, char *, char *, int, unsigned int *);
struct rethinkdb_connection *rethinkdb_init(char *, unsigned short, int);
char *rethinkdb_get(struct rethinkdb_connection *, char *, char *, char *, char *, int);
char **rethinkdb_filter(struct rethinkdb_connection *, char *, char *, char *, unsigned int *);
char *rethinkdb_insert(struct rethinkdb_connection *, char *, char *, char **, unsigned int, int);
