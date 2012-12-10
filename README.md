librethinkdb
============

a C library for accessing rethinkdb servers

Example
-------

```c
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