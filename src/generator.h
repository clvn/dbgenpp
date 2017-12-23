#pragma once

enum valuetype {
	dbgen_integer,
	dbgen_float,
	dbgen_blob,
	dbgen_text
};

struct fieldinfo {
	std::string fieldname;
	int type; // SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_BLOB, SQLITE_TEXT
	int size; // for varchar(size)
	bool primarykey; // for integer primary key
	std::string keytable; // foreign key table
	std::string keyname;  // foreign key field
	bool cascade; // cascade delete by default
	bool nullable; // nullable foreign keys
};

struct tableinfo {
	std::string tablename;
	std::vector<fieldinfo> fields;
	bool generate_before_insert;
	bool generate_after_insert;
	bool generate_before_update;
	bool generate_after_update;
	bool generate_before_delete;
	bool generate_after_delete;
	bool generate_undo;
};

struct documentgen {
	std::vector<tableinfo> tables;
	std::vector<tableinfo> events;

	void generate_document_header(const std::string& prefix, std::ostream& strm);
	void generate_document_implementation(const std::string& prefix, std::ostream& strm);
};
