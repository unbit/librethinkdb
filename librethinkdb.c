#include "librethinkdb.h"

/*
	send the specified amount of datas to the socket
*/
int rethinkdb_send(struct rethinkdb_connection *rdb_conn, void *buf, size_t len) {
	size_t remains = len;
	void *ptr = buf;
	while(remains) {
		ssize_t rlen = write(rdb_conn->s, ptr, remains);
		if (rlen <= 0) {
			return -1;
		}
		ptr += rlen;
		remains -= rlen;
	}
	return 0;
}

/*
	connect to a server
*/
int rethinkdb_connect(struct rethinkdb_connection *rdb_conn) {

	if (rdb_conn->s >= 0) return 0;

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	rdb_conn->s = socket(AF_INET, SOCK_STREAM, 0);
	if (rdb_conn->s < 0) {
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(rdb_conn->addr);
	sin.sin_port = htons(rdb_conn->port);

	if (connect(rdb_conn->s, (struct sockaddr *) &sin, sizeof(sin))) {
		close(rdb_conn->s);
		rdb_conn->s = -1;
		return -1;
	}

	return 0;

}

/*
	send the version string
*/
int rethinkdb_send_version(struct rethinkdb_connection *rdb_conn) {
	if (rdb_conn->version_sent) return 0;
	int ret = rethinkdb_send(rdb_conn, "\x35\xba\x61\xaf", 4);
	if (ret == 0) {
		rdb_conn->version_sent = 1;
		return 0;
	}
	return -1;
}

/*
	check if the connection is valid
*/
int rethinkdb_check(struct rethinkdb_connection *rdb_conn) {
	if (rethinkdb_connect(rdb_conn)) {
		return -1;
	}
	return rethinkdb_send_version(rdb_conn);
}

/*
	send a protobuf packet (prefixed with the size)
*/
int rethinkdb_send_protobuf(struct rethinkdb_connection *rdb_conn, size_t len) {
	char *ptr = rdb_conn->query_buf;
	ptr[0] = (char) (len & 0xff);
	ptr[1] = (char) (len  >> 8 ) & 0xff;
	ptr[2] = (char) (len  >> 16 ) & 0xff;
	ptr[3] = (char) (len  >> 24 ) & 0xff;
	return rethinkdb_send(rdb_conn, rdb_conn->query_buf, len + 4);
}

/*
	send a query
*/
int rethinkdb_send_query(struct rethinkdb_connection *rdb_conn, Query *q) {
	size_t len = query__get_packed_size(q);
	rdb_conn->query_buf = malloc(len + 4);
	query__pack(q, rdb_conn->query_buf + 4);
	int ret = rethinkdb_send_protobuf(rdb_conn, len);
	free(rdb_conn->query_buf);
	rdb_conn->query_buf = NULL;
	return ret;
}

/*
	receive the specified amount of datas
*/
int rethinkdb_recv(struct rethinkdb_connection *rdb_conn, void *buf, size_t len) {
	size_t remains = len;
	void *ptr = buf;
	while(remains) {
		ssize_t rlen = read(rdb_conn->s, ptr, remains);
		if (rlen <= 0) {
			return -1;
		}
		ptr += rlen;
		remains -=rlen;
	}
	return 0;
}

/*
	get the size of a response
*/
int32_t rethinkdb_response_size(struct rethinkdb_connection *rdb_conn) {
	int32_t len = 0;
	if (rethinkdb_recv(rdb_conn, &len, 4)) {
		return 0;
	}
	char *ptr = (char *) &len;
	ptr[0] = (char) (len & 0xff);
	ptr[1] = (char) (len  >> 8 ) & 0xff;
	ptr[2] = (char) (len  >> 16 ) & 0xff;
	ptr[3] = (char) (len  >> 24 ) & 0xff;
	return len;
}

/*
	parse a response
*/
Response *rethinkdb_response(struct rethinkdb_connection *rdb_conn) {
	int32_t len = rethinkdb_response_size(rdb_conn);
	if (len == 0) return NULL;
	rdb_conn->response_buf = malloc(len);
	if (rethinkdb_recv(rdb_conn, rdb_conn->response_buf, len)) {
		free(rdb_conn->response_buf);
		rdb_conn->response_buf = NULL;
		return NULL;
	}
	return response__unpack(NULL, len, rdb_conn->response_buf);
}

/*
	check for a specific response code
*/
int rethinkdb_response_check(struct rethinkdb_connection *rdb_conn, int val) {
	Response *r = rethinkdb_response(rdb_conn);
	int ret = r->status_code; 
	free(rdb_conn->response_buf);
	rdb_conn->response_buf = NULL;

	if (ret == val) {
		return 0;
	}
	return -1;	
}

/*
	get the list of response strings
*/
char **rethinkdb_response_list(struct rethinkdb_connection *rdb_conn, unsigned int *len) {
	Response *r = rethinkdb_response(rdb_conn);
	if (r->status_code == RESPONSE__STATUS_CODE__SUCCESS_STREAM) {
		*len = r->n_response;	
		return r->response;
	}
	return NULL;
}

/*
	get a JSON response
*/
char *rethinkdb_response_json(struct rethinkdb_connection *rdb_conn) {
        Response *r = rethinkdb_response(rdb_conn);
        if (r->status_code == RESPONSE__STATUS_CODE__SUCCESS_JSON) {
		if (r->n_response > 0) {
                	return r->response[0];
		}
        }
        return NULL;
}


/*
	create a db
*/
int rethinkdb_create_db(struct rethinkdb_connection *rdb_conn, char *dbname) {
	if (rethinkdb_check(rdb_conn)) return -1;

	Query q = QUERY__INIT;
	MetaQuery mq = META_QUERY__INIT;
	q.type = QUERY__QUERY_TYPE__META;
	q.token = rdb_conn->token;
	q.meta_query = &mq;
	mq.type = META_QUERY__META_QUERY_TYPE__CREATE_DB;
	mq.db_name = dbname;

	if (rethinkdb_send_query(rdb_conn, &q)) {
		return -1;
	}

	return rethinkdb_response_check(rdb_conn, RESPONSE__STATUS_CODE__SUCCESS_EMPTY);
}

/*
	create a table
*/
int rethinkdb_create_table(struct rethinkdb_connection *rdb_conn, char *dbname, char *tablename, char *datacenter, char *pkey, int use_outdated) {
        if (rethinkdb_check(rdb_conn)) return -1;

        Query q = QUERY__INIT;
        MetaQuery mq = META_QUERY__INIT;
	MetaQuery__CreateTable ct = META_QUERY__CREATE_TABLE__INIT;
	TableRef tr = TABLE_REF__INIT;

        q.type = QUERY__QUERY_TYPE__META;
        q.token = rdb_conn->token;
        q.meta_query = &mq;

        mq.type = META_QUERY__META_QUERY_TYPE__CREATE_TABLE;
	mq.create_table = &ct;

	ct.datacenter = datacenter;
	ct.primary_key = pkey;
	ct.table_ref = &tr;

	tr.db_name = dbname;
	tr.table_name = tablename;
	tr.use_outdated = use_outdated;

        if (rethinkdb_send_query(rdb_conn, &q)) {
                return -1;
        }

        return rethinkdb_response_check(rdb_conn, RESPONSE__STATUS_CODE__SUCCESS_EMPTY);
}

/*
	list dbs
*/
char **rethinkdb_list_db(struct rethinkdb_connection *rdb_conn, unsigned int *len) {

	if (rethinkdb_check(rdb_conn)) return NULL;

        Query q = QUERY__INIT;
        MetaQuery mq = META_QUERY__INIT;
        q.type = QUERY__QUERY_TYPE__META;
        q.token = rdb_conn->token;
        q.meta_query = &mq;
        mq.type = META_QUERY__META_QUERY_TYPE__LIST_DBS;

        if (rethinkdb_send_query(rdb_conn, &q)) {
                return NULL;
        }

        return rethinkdb_response_list(rdb_conn, len);
	
}

/*
	r.table('table')
*/
char **rethinkdb_table(struct rethinkdb_connection *rdb_conn, char *dbname, char *tablename, int use_outdated, unsigned int *len) {
	if (rethinkdb_check(rdb_conn)) return NULL;

        Query q = QUERY__INIT;
        ReadQuery rq = READ_QUERY__INIT;
	Term t = TERM__INIT;
	Term__Table tb = TERM__TABLE__INIT;
	TableRef tr = TABLE_REF__INIT;

        q.type = QUERY__QUERY_TYPE__READ;
        q.token = rdb_conn->token;
        q.read_query = &rq;

        rq.term = &t;

	t.type = TERM__TERM_TYPE__TABLE;
	t.table = &tb;

	tb.table_ref = &tr;

	tr.db_name = dbname;
        tr.table_name = tablename;
        tr.use_outdated = use_outdated;
	

        if (rethinkdb_send_query(rdb_conn, &q)) {
                return NULL;
        }

        return rethinkdb_response_list(rdb_conn, len);
}

/*
	init a rethinkdb connection
*/
struct rethinkdb_connection *rethinkdb_init(char *address, unsigned short port, int timeout) {
	struct rethinkdb_connection *rdb_conn = calloc(1, sizeof(struct rethinkdb_connection));
	rdb_conn->s = -1;
	rdb_conn->addr = address;
	rdb_conn->port = port;
	rdb_conn->timeout = timeout;
	return rdb_conn;
}

/*
	r.table('table').get('id')
*/
char *rethinkdb_get(struct rethinkdb_connection *rdb_conn, char *dbname, char *tablename, char *pk, char *attr, int use_outdated) {

	if (rethinkdb_check(rdb_conn)) return NULL;

        Query q = QUERY__INIT;
        ReadQuery rq = READ_QUERY__INIT;
        Term t = TERM__INIT;
	Term tk = TERM__INIT;
        Term__GetByKey gbk = TERM__GET_BY_KEY__INIT;
        TableRef tr = TABLE_REF__INIT;

        q.type = QUERY__QUERY_TYPE__READ;
        q.token = rdb_conn->token;
        q.read_query = &rq;

        rq.term = &t;

        t.type = TERM__TERM_TYPE__GETBYKEY;
        t.get_by_key = &gbk;

        gbk.table_ref = &tr;
	if (!attr) {
		gbk.attrname = "id";
	}
	else {
		gbk.attrname = attr;
	}
	gbk.key = &tk;

        tr.db_name = dbname;
        tr.table_name = tablename;
        tr.use_outdated = use_outdated;

	tk.type = TERM__TERM_TYPE__STRING;
	tk.valuestring = pk;


        if (rethinkdb_send_query(rdb_conn, &q)) {
                return NULL;
        }

        return rethinkdb_response_json(rdb_conn);
}

/*
FIXME
	r.table('table').filter(...)
*/
char **rethinkdb_filter(struct rethinkdb_connection *rdb_conn, char *dbname, char *tablename, char *filter, unsigned int *len) {

	if (rethinkdb_check(rdb_conn)) return NULL;

        Query q = QUERY__INIT;
        ReadQuery rq = READ_QUERY__INIT;
        Term t = TERM__INIT;
        //Term j = TERM__INIT;
	Term__Call tc = TERM__CALL__INIT;
	Builtin b = BUILTIN__INIT;
	Builtin__Filter f = BUILTIN__FILTER__INIT;
	Predicate p = PREDICATE__INIT;

        q.type = QUERY__QUERY_TYPE__READ;
        q.token = rdb_conn->token;
        q.read_query = &rq;

        rq.term = &t;

        t.type = TERM__TERM_TYPE__CALL;
        t.call = &tc;

	tc.builtin = &b;

	b.type = BUILTIN__BUILTIN_TYPE__FILTER;
	b.filter = &f;

	f.predicate = &p;
	//p. = &j;

	//j.type = TERM__TERM_TYPE__JSON;
	//j.jsonstring = filter;

        if (rethinkdb_send_query(rdb_conn, &q)) {
                return NULL;
        }

        return rethinkdb_response_list(rdb_conn, len);
}

/*
	r.table('table').insert([{...}])

*/
char *rethinkdb_insert(struct rethinkdb_connection *rdb_conn, char *dbname, char *tablename, char **json, unsigned int json_n, int upsert) {

	char *json_r = NULL;

	Query q = QUERY__INIT;
        WriteQuery wq = WRITE_QUERY__INIT;
	WriteQuery__Insert in = WRITE_QUERY__INSERT__INIT;
	TableRef tr = TABLE_REF__INIT;
	

        q.type = QUERY__QUERY_TYPE__WRITE;
        q.token = rdb_conn->token;
        q.write_query = &wq;

        wq.type = WRITE_QUERY__WRITE_QUERY_TYPE__INSERT;
	wq.insert = &in;

	tr.db_name = dbname;
        tr.table_name = tablename;

	in.table_ref = &tr;
	in.n_terms = json_n;
	in.terms = malloc(sizeof(Term *) * json_n);
	unsigned int i;
	for(i=0;i<json_n;i++) {
		in.terms[i] = malloc(sizeof(Term));
		term__init(in.terms[i]);
		in.terms[i]->type = TERM__TERM_TYPE__JSON;
		in.terms[i]->jsonstring = json[i];
	}
	in.overwrite = upsert;

        if (rethinkdb_send_query(rdb_conn, &q)) {
		goto clear;
        }

        json_r = rethinkdb_response_json(rdb_conn);
clear:
	for(i=0;i<json_n;i++) {
		free(in.terms[i]);
	}
	free(in.terms);
 
	return json_r;
}
