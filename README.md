# dbgenpp - Database code generator for C++ and sqlite

dbgenpp generates C++ helper code for working with SQLite database schemas
described in JSON format.

Windows:
- Visual Studio for Desktop 2012 (Express or newer)

Posix/Linux:
- Run the following commands from the project directory:

		autoreconf -vi
		./configure
		make
		sudo make install

Usage:

	dbgenpp inputfile.dbgen

If the .dbgen file parses all right, the program creates inputfile_types.h and
inputfile_types_cpp.h as output.

Generates compile time type inspection information. The generated code is
intended for use with boost::mpl and sqlite3.

The dbgen library of run time functions include:

- handle SQL triggers with associated data in C++ callbacks
- undo/redo of any changes made in the database
- cascade delete via foreign keys

The dbgenpp library of compile time template functions include:

- CREATE TABLE query generator
- clone and import query generator templates
- SELECT/INSERT/UPDATE/DELETE query generators for single records

Example JSON database description:

```json
{
	"tables" : {
		"album" : {
			"fields" : [
				[ "id", "int", "not null", "primary" ],
				[ "name", "varchar(256)", "not null" ],
				[ "artist_id", "int", "not null", { 
					"reftable":"artist", "refkey": "id" 
				} ], ]
			],
			"before_insert":false,
			"after_insert":true,
			"before_update":false,
			"after_update":true,
			"before_delete":false,
			"after_delete":true
		}
		"artist" : {
			"fields" : [
				[ "id", "int", "not null", "primary" ],
				[ "name", "varchar(256)", "not null" ]
			],
			"before_insert":false,
			"after_insert":true,
			"before_update":false,
			"after_update":true,
			"before_delete":false,
			"after_delete":true
		}
	},
	"events" : { 
		"barrier" : [], 
		"save_album_bitmap" : [
			[ "album_id", "int" ],
			[ "filename", "varchar(1024)" ]
		]
	}
}
```

In this example, there are two SQL tables, "artist" and "album". Each SQL
table object has a "fields" array" and flags to control how to generate
trigger handlers for insert/update/delete operations.

The contents of the "fields"-array is interpreted as follows:
- index 0: field name
- index 1: field type, one of "int", "varchar(N)", "text", "float", "bit",
	         "blob"
- index 2..N: optional field type modifiers, one or more of "primary", "not
	         null", or a foreign table mapping object

The foreign table mapping object has two string properties: "reftable" and
"refkey", refering to the foreign table and field being mapped.

Custom events provide a mechanism to implement actions that support undo/redo,
but which does not rely on database changes for invocation.
