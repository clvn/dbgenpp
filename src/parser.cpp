#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include "picojson.h"
#include "parser.h"
#include "generator.h"

using std::cerr;
using std::endl;
using std::string;

std::string read_file(std::string const& path) {
	FILE* file = fopen(path.c_str(), "rb");

	if (!file)
		return "";

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	std::string text;
	char* buffer = new char[size + 1];
	buffer[size] = 0;

	if (fread(buffer, 1, size, file) == (unsigned long)size)
		text = buffer;

	fclose(file);
	delete[] buffer;

	return text;
}


bool table_has_foreign_key(tableinfo& tabinfo) {
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		if (tabinfo.fields[i].keytable.length() > 0) return true;
	}
	return false;
}

bool table_exists(std::vector<tableinfo>& tables, std::string tablename) {
	for (size_t i = 0; i < tables.size(); i++) {
		if (tables[i].tablename == tablename) return true;
	}
	return false;
}

bool table_has_foreign_keys(tableinfo& tabinfo, std::string keytable, std::vector<tableinfo>& tables) {
	bool all_keys_satisfied = true;
	bool found_keytable = false;
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		fieldinfo& fi = tabinfo.fields[i];
		if (tabinfo.fields[i].keytable.length() > 0) {
			if (tabinfo.fields[i].keytable == keytable)
				found_keytable = true;
			else
				if (tabinfo.tablename != tabinfo.fields[i].keytable)
					all_keys_satisfied &= table_exists(tables, tabinfo.fields[i].keytable);
		}
	}
	return found_keytable && all_keys_satisfied;
}


bool parse_varchar(std::string name, int* size) {
	std::string::size_type gp = name.find_first_of('(');
	if (gp == std::string::npos) return false;
	std::string::size_type lp = name.find_first_of(')', gp);
	if (lp == std::string::npos) return false;
	std::string varcharpart = name.substr(0, gp);
	if (varcharpart != "varchar") return false;
	std::string s = name.substr(gp + 1, (lp - gp) - 1);
	*size = atoi(s.c_str());
	return *size != 0;
}

std::string get_object_string(picojson::value obj, const std::string& name) {
	picojson::value val = obj.get(name);
	if (val.is<std::string>()) return val.get<std::string>();
	return "";
}

bool get_object_bool(picojson::value obj, const std::string& name, bool defvalue) {
	picojson::value val = obj.get(name);
	if (val.is<bool>()) return val.get<bool>();
	return defvalue;
}

bool parse_field(const picojson::array& fieldarray, fieldinfo* finfo) {
	string fieldName;
	int type = 0;
	int size = 0;
	bool nullable = true;
	bool primary = false;
	std::string keyname, keytable;
	bool keycascade = true;
	
	for (picojson::value::array::const_iterator i = fieldarray.begin(); i != fieldarray.end(); ++i) {
		size_t index = std::distance(fieldarray.begin(), i);

		if (index == 0) {
			if (i->is<std::string>()) {
				fieldName = i->get<std::string>();
			} else {
				cerr << "invalid field name" << endl;
				return false;
			}
		} else
		if (index == 1) {
			if (i->is<std::string>()) {
				string typeName = i->get<std::string>();
				if (typeName == "int")
					type = dbgen_integer;
				else if (typeName == "bit")
					type = dbgen_integer;
				else if (typeName == "blob")
					type = dbgen_blob;
				else if (typeName == "float")
					type = dbgen_float;
				else if (typeName == "text")
					type = dbgen_text;
				else if (parse_varchar(typeName, &size))
					type = dbgen_text;
				else {
					cerr << "unknown type '" << typeName << "'" << endl;
					return false;
				}
			} else {
				cerr << "unknown type declaration" << endl;
				return false;
			}
		} else {
			if (i->is<picojson::object>()) {
				keytable = get_object_string(*i, "reftable");
				keyname = get_object_string(*i, "refkey");
				keycascade = get_object_bool(*i, "cascade", true); //fieldDecl.get("cascade", Json::Value(true)).asBool();
			} else if (i->is<std::string>()) {
				std::string declName = i->get<std::string>();
				if (declName == "not null")
					nullable = false;
				else if (declName == "null")
					nullable = true;
				else if (declName == "primary")
					primary = true; 
				else {
					cerr << "unknown modifier " << declName << endl;
					return false;
				}
			} else {
				cerr << "unknown element in fields array" << endl;
				return false;
			}
		}
	}
	if ((keytable.length() != 0 && keyname.length() == 0) ||
		(keytable.length() == 0 && keyname.length() != 0)) {
		cerr << "invalid keytable or keyname" << endl;
		return false;
	}

	finfo->fieldname = fieldName;
	finfo->primarykey = primary;
	finfo->type = type;
	finfo->size = size;
	finfo->keytable = keytable;
	finfo->keyname = keyname;
	finfo->cascade = keycascade;
	finfo->nullable = nullable;
	
	return true;
}

bool parse_table_fields(const std::string& name, const picojson::value& fields, std::vector<fieldinfo>& result) {
	const picojson::value::array& fieldsarray = fields.get<picojson::array>();

	for (picojson::value::array::const_iterator i = fieldsarray.begin(); i != fieldsarray.end(); ++i) {
		const picojson::value& field = *i;

		if (!field.is<picojson::array>()) {
			cerr << "field description contains non-array elements on " << name << endl;
			return false;
		}

		const picojson::value::array& fieldarray = field.get<picojson::array>();

		if (fieldarray.size() < 2) {
			cerr << "invalid field on " << name << endl;
			return false;
		}
		fieldinfo finfo;
		if (!parse_field(fieldarray, &finfo))
			return false;
		result.push_back(finfo);
	}

	return true;
}

bool parse_table(const picojson::value& table, const std::string& name, tableinfo* tabinfo) {
	picojson::value fields = table.get("fields");
	if (!fields.is<picojson::array>()) return false;

	tabinfo->tablename = name;
	if (!parse_table_fields(name, fields, tabinfo->fields))
		return false;

	tabinfo->generate_before_insert = get_object_bool(table, "before_insert", false);
	tabinfo->generate_after_insert = get_object_bool(table, "after_insert", false);
	tabinfo->generate_before_update = get_object_bool(table, "before_update", false);
	tabinfo->generate_after_update = get_object_bool(table, "after_update", false);
	tabinfo->generate_before_delete = get_object_bool(table, "before_delete", false);
	tabinfo->generate_after_delete = get_object_bool(table, "after_delete", false);
	tabinfo->generate_undo = get_object_bool(table, "undo", true);
	return true;
}

bool parse_events(const picojson::value& events, const std::string& name, tableinfo* tabinfo) {
	if (!events.is<picojson::array>()) return false;

	tabinfo->tablename = name;
	return parse_table_fields(name, events, tabinfo->fields);
}

bool documentgenparser::parse_dbgen(const char* jsonfile, documentgen* result) {
	
	std::ifstream strm(jsonfile);
	if (!strm) {
		std::cerr << "cannot open " << jsonfile << endl;
		return false;
	}

	picojson::value root;
	std::string err = picojson::parse(root, strm);
	bool parsingSuccessful = err.empty();

	if (!parsingSuccessful) {
		std::cerr << "documentgen: failed to parse file: " << jsonfile << std::endl;
		std::cerr << "documentgen: parsing error: " << err << std::endl;
		return false;
	}

	if (!root.is<picojson::object>()) return false;
	picojson::value events = root.get("events");
	picojson::value tables = root.get("tables");

	if (!events.is<picojson::null>() && !events.is<picojson::object>()) {
		cerr << "could not parse events object";
		return false;
	}

	const picojson::value::object& tablesobj = tables.get<picojson::object>();
	std::vector<tableinfo> tableinfos;
	for (picojson::value::object::const_iterator i = tablesobj.begin(); i != tablesobj.end(); ++i) {
		tableinfo tabinfo;
		if (!parse_table(i->second, i->first, &tabinfo))
			return false;
		tableinfos.push_back(tabinfo);
	}

	// sort tables topologically
	std::deque<tableinfo*> input;
	for (size_t i = 0; i < tableinfos.size(); i++) {
		tableinfo& tabinfo = tableinfos[i];
		if (!table_has_foreign_key(tabinfo))
			input.push_back(&tabinfo);
	}

	if (input.size() == 0) {
		cerr << "no root table(s)" << endl;
		return false;
	}

	while (input.size()) {
		tableinfo* intab = input.front();
		input.pop_front();
		result->tables.push_back(*intab);

		for (size_t i = 0; i < tableinfos.size(); i++) {
			tableinfo& tabinfo = tableinfos[i];
			if (table_has_foreign_keys(tabinfo, intab->tablename, result->tables) && !table_exists(result->tables, tabinfo.tablename)) {
				input.push_back(&tabinfo);
			}
		}
	}

	if (result->tables.size() != tableinfos.size()) {
		cerr << "warning: result table count mismatch" << endl;
	}
	
	if (!events.is<picojson::null>()) {
		const picojson::value::object& eventsobj = events.get<picojson::object>();

		for (picojson::value::object::const_iterator i = eventsobj.begin(); i != eventsobj.end(); ++i) {
			tableinfo tabinfo;
			tabinfo.tablename = i->first;
			if (!parse_table_fields(i->first, i->second, tabinfo.fields))
				return false;
			result->events.push_back(tabinfo);
		}
	}
	return true;
}
