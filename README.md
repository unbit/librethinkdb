librethinkdb
============

a C library for accessing rethinkdb servers

Example
-------

```c

   #include <rethinkdb.h>
   
   // connect to the rethinkdb server (addr, port, milliseconds_timeout)
   struct rethinkdb_connection *r = rethinkdb_init("127.0.0.1", 28015, 1000);
   
   // create a db (returns 0 on success)
   rethinkdb_create_db(r, "foobar");
   // create a table (returns 0 on success)
   rethinkdb_create_table(r, "foobar", "mytable", NULL, NULL, 0);
   
   // insert 3 json items
   char *json[] = {
     "{\"foo\": \"bar\"}",
     "{\"foo2\": \"bar2\"}",
     "{\"foo3\": \"bar3\"}",
   };
   // returns a json string with the response
   rethinkdb_insert(r, "foobar", "mytable", json, 3, 0);
   
   // get the items of a table (returns NULL on error)
   unsigned int i, n_items = 0;
   char **items = rethinkdb_table(r, "foobar", "mytable", 0, &n_items);
   
   for(i=0;i<n_items;i++) {
       printf("JSON = %s\n", items[i]);
   }
```

Requirements
------------

You need protobuf-c library (apt-get install libprotobuf-c0-dev)

Install
-------

simply run make then move the resulting librethinkdb.so to a libraries directory (like /usr/local/lib) and librethinkdb.h
to a headers directory (like /usr/local/include)

Status
------

Error checking is still flaky

Timeout is still unimplemented

The protobuf api is exposed, so while waiting for higher level functions you can build
your queries using the Query, Term, ... objects