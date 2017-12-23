#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "generator.h"

using std::endl;
using std::cerr;
using std::string;
using std::stringstream;

string sqlite_type_to_cpp_type(int type) {
	switch (type) {
		case dbgen_integer:
			return "int";
		case dbgen_text:
			return "std::string";
		case dbgen_float:
			return "double"; // SQLITE_FLOAT is a 64-bit double.
		case dbgen_blob:
			return "std::vector<unsigned char>";
		default:
			return "";
	}
}

void generate_class_header_event(tableinfo& tabinfo, std::ostream& strm) {
	std::string tablename = tabinfo.tablename + "data";
	strm << "struct " << tablename << " {" << endl;

	// generate data members
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		fieldinfo& finfo = tabinfo.fields[i];
		strm << "\t" << sqlite_type_to_cpp_type(finfo.type) << " " << finfo.fieldname << ";" << endl;
	}
	strm << "};" << endl << endl;
}

void generate_class_header(tableinfo& tabinfo, std::ostream& strm) {
	std::string tablename = tabinfo.tablename + "data";
	strm << "struct " << tablename << " {" << endl;

	strm << "\tstruct table_traits {" << endl;
	strm << "\t\tstatic const char* name() { return \"" << tabinfo.tablename << "\"; }" << endl;
	strm << "\t\tstatic int before_insert() { return event_type_before_insert_" << tabinfo.tablename << "; }" << endl;
	strm << "\t\tstatic int after_insert() { return event_type_insert_" << tabinfo.tablename << "; }" << endl;
	strm << "\t\tstatic int before_update() { return event_type_before_update_" << tabinfo.tablename << "; }" << endl;
	strm << "\t\tstatic int after_update() { return event_type_update_" << tabinfo.tablename << "; }" << endl;
	strm << "\t\tstatic int before_delete() { return event_type_before_delete_" << tabinfo.tablename << "; }" << endl;
	strm << "\t\tstatic int after_delete() { return event_type_delete_" << tabinfo.tablename << "; }" << endl;
	strm << "\t};" << endl;

	// generate metadata column traits
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		fieldinfo& finfo = tabinfo.fields[i];
		strm << "\tstruct _" << finfo.fieldname << " {" << endl;
		strm << "\t\tstatic const char* name() { return \"" << finfo.fieldname << "\"; }" << endl;
		strm << "\t\tstatic const char* keytable() { return \"" << finfo.keytable << "\"; }" << endl;
		strm << "\t\tstatic const char* keyname() { return \"" << finfo.keyname << "\"; }" << endl;
		strm << "\t\ttypedef " << sqlite_type_to_cpp_type(finfo.type) << " type;" << endl;
		strm << "\t\tenum { is_primary = " << (finfo.primarykey?"true":"false") << ", is_nullable = " << (finfo.nullable?"true":"false") << " };" << endl;
		strm << "\t\tstatic type " << tablename << "::*member() { return &" << tablename << "::" << finfo.fieldname << "; };" << endl;
		strm << "\t};" << endl;
	}

	// generate metadata type
	strm << "\ttypedef boost::mpl::vector<" << endl;
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		fieldinfo& finfo = tabinfo.fields[i];
		if (i > 0)
			strm << ", " << endl;
		strm << "\t\t_" << finfo.fieldname;
	}
	strm << endl << "\t> column_members;" << endl << endl;

	// generate data members
	for (size_t i = 0; i < tabinfo.fields.size(); i++) {
		fieldinfo& finfo = tabinfo.fields[i];
		strm << "\t_" << finfo.fieldname << "::type " << finfo.fieldname << ";" << endl;
	}
	strm << "};" << endl << endl;
}

void documentgen::generate_document_header(const std::string& prefix, std::ostream& strm) {
	strm << "#pragma once" << endl << endl;
	strm << "// (automatically generated)" << endl << endl;

	strm << "enum {" << endl;
	for (size_t i = 0; i < tables.size(); i++) {
		strm << "\tevent_type_before_insert_" << tables[i].tablename << ", " << endl;
		strm << "\tevent_type_insert_" << tables[i].tablename << ", " << endl;

		strm << "\tevent_type_before_delete_" << tables[i].tablename << ", " << endl;
		strm << "\tevent_type_delete_" << tables[i].tablename << ", " << endl;

		strm << "\tevent_type_before_update_" << tables[i].tablename << ", " << endl;
		strm << "\tevent_type_update_" << tables[i].tablename << ", " << endl;
	}
	strm << endl;

	for (size_t i = 0; i < events.size(); i++) {
		strm << "\tevent_type_" << events[i].tablename << ", " << endl;
	}
	strm << "};" << endl;

	strm << endl;

	for (size_t i = 0; i < tables.size(); i++) {
		generate_class_header(tables[i], strm);
	}

	strm << "typedef boost::mpl::vector<" << endl;
	strm << "\t";
	for (size_t i = 0; i < tables.size(); i++) {
		if (i > 0) strm << ", ";
		strm << tables[i].tablename << "data";
	}
	strm << endl;
	strm << "> database_tables;" << endl << endl;

	for (size_t i = 0; i < events.size(); i++) {
		if (!events[i].fields.empty())
			generate_class_header_event(events[i], strm);
	}

	strm << "struct tableunion {" << endl;
	strm << "\tunion {" << endl;
	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << "\t\t" << tabinfo.tablename << "data* " << tabinfo.tablename << ";" << endl;
	}
	for (size_t i = 0; i < events.size(); i++) {
		if (!events[i].fields.empty()) {
			tableinfo& tabinfo = events[i];
			strm << "\t\t" << tabinfo.tablename << "data* " << tabinfo.tablename << ";" << endl;
		}			
	}

	strm << "\t};" << endl;
	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << endl;
		strm << "\tvoid operator=(" << tabinfo.tablename << "data& data) {" << endl;
		strm << "\t\t" << tabinfo.tablename << " = &data;" << endl;
		strm << "\t}" << endl;
	}
	strm << "};" << endl << endl;

	strm << "struct document_event_data {" << endl;
	strm << "\tint type;" << endl;
	strm << "\tint id;" << endl;
	strm << "\ttableunion newdata;" << endl;
	strm << "\ttableunion olddata;" << endl;

	strm << "};" << endl << endl;

}


void documentgen::generate_document_implementation(const std::string& prefix, std::ostream& strm) {
	strm << "// (automatically generated)" << endl << endl;

	// generate functions which are used as trigger callbacks
	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << "extern \"C\" void " << tables[i].tablename << "_notify_callback(sqlite3_context* ctx, int, sqlite3_value** row) {" << endl;
		strm << "\tbool result = dbgenpp::table_notify_callback<" << tables[i].tablename << "data, document_event_data>(ctx, row);" << endl;
		strm << "\tsqlite3_result_int(ctx, result?1:0);" << endl;
		strm << "}" << endl;
		strm << endl;
	}

	// generate a function which generates a query that generates tables in an empty database
	strm << "void create_tables(std::ostream& query, const char* prefix) {" << endl;

	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << "\tquery << \"create table \" << prefix << \"" << tabinfo.tablename << " (";
		for (size_t j = 0; j < tabinfo.fields.size(); j++) {
			if (j > 0) strm << ", ";
			fieldinfo& finfo = tabinfo.fields[j];
			strm << finfo.fieldname << " ";
			switch (finfo.type) {
				case dbgen_integer:
					strm << "integer";
					break;
				case dbgen_float:
					strm << "real";
					break;
				case dbgen_text:
					strm << "varchar(" << finfo.size << ")";
					break;
				case dbgen_blob:
					strm << "blob";
					break;
			}
			if (finfo.primarykey) strm << " primary key";
			if (!finfo.keytable.empty()) strm << " references " << finfo.keytable << "(" << finfo.keyname << ")";
		}
		strm << ");\" << endl;" << endl;

		// create indexes for foreign keys to prevent full table scans during enforcing
		for (size_t j = 0; j < tabinfo.fields.size(); j++) {
			fieldinfo& finfo = tabinfo.fields[j];
			if (finfo.keytable.empty()) {
				continue;
			}
			std::string indexname = tabinfo.tablename + "_" + finfo.keytable + "_" + finfo.keyname + "_index";
			strm << "\tquery << \"create index \" << prefix << \"" << indexname << " ON ";
			strm << tabinfo.tablename << "(" << finfo.fieldname << ");\" << endl;" << endl;
		}
	}
	strm << "}" << endl << endl;

	strm << "void create_callbacks(sqlite3* db, void* self) {" << endl;
	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << "\tsqlite3_create_function(db, \"" << tabinfo.tablename << "_notify_callback\", -1, SQLITE_ANY, self, " << tabinfo.tablename << "_notify_callback, 0, 0);" << endl;
	}
	strm << "}" << endl << endl;

	strm << "void create_triggers(sqlite3* db, std::ostream& query) {" << endl;

	for (size_t i = 0; i < tables.size(); i++) {
		tableinfo& tabinfo = tables[i];
		strm << "\t// " << tabinfo.tablename << ":" << endl;

		stringstream newfieldsquery, oldfieldsquery, updatefieldsquery, oldfieldsnoquotequery;
		for (size_t j = 0; j < tabinfo.fields.size(); j++) {
			fieldinfo& fi = tabinfo.fields[j];
			if (j > 0) newfieldsquery << ", ";
			newfieldsquery << "new." << fi.fieldname;

			if (j > 0) oldfieldsquery << ", ";
			oldfieldsquery << "'||quote(old." << fi.fieldname << ")||'";

			if (j > 0) oldfieldsnoquotequery << ", ";
			oldfieldsnoquotequery << "old." << fi.fieldname;

			if (j > 0) updatefieldsquery << ", ";
			updatefieldsquery << fi.fieldname << " = '||quote(old." << fi.fieldname << ")||'";
		}

		// generate before insert trigger:
		if (tabinfo.generate_before_insert) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_before_insert_trigger before insert on " << tabinfo.tablename << " begin\" << endl;" << endl;
			strm << "\tquery << \"select raise(abort, 'before insert failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(10, " << newfieldsquery.str() << ") = 0;\" << endl;" << endl;
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}

		// generate after insert trigger:
		if (tabinfo.generate_after_insert || tabinfo.generate_undo) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_insert_notify_trigger after insert on " << tabinfo.tablename << " begin\" << endl;" << endl;
			if (tabinfo.generate_undo)
				strm << "\tquery << \"select undoredo_add_query('delete from " << tabinfo.tablename << " where id = '||quote(new.id)||';') where undoredo_enabled_callback() = 1;\" << endl;" << endl;
			if (tabinfo.generate_after_insert) {
				strm << "\tquery << \"select raise(abort, 'after insert failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(0, " << newfieldsquery.str() << ") = 0;\" << endl;" << endl;
			}
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}

		// generate before delete trigger:
		int cascadecount = 0;
		for (size_t j = 0; j < tables.size(); j++) {
			tableinfo& otabinfo = tables[j];
			for (size_t i = 0; i < otabinfo.fields.size(); i++) {	
				fieldinfo& finfo = otabinfo.fields[i];
				if (finfo.keytable == tabinfo.tablename && finfo.cascade) {
					cascadecount ++;
				}
			}
		}

		if (cascadecount > 0 || tabinfo.generate_before_delete || tabinfo.generate_undo) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_delete_notify_trigger before delete on " << tabinfo.tablename << " begin\" << endl;" << endl;
			
			if (tabinfo.generate_before_delete) {
				strm << "\tquery << \"select raise(abort, 'before delete failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(11, " << oldfieldsnoquotequery.str() << ") = 0;\" << endl;" << endl;
			}

			// cascade delete
			for (size_t j = 0; j < tables.size(); j++) {
				tableinfo& otabinfo = tables[j];
				for (size_t i = 0; i < otabinfo.fields.size(); i++) {	
					fieldinfo& finfo = otabinfo.fields[i];
					if (finfo.keytable == tabinfo.tablename && finfo.cascade) {
						strm << "\tquery << \"delete from " << otabinfo.tablename << " where " << finfo.fieldname << " = old.id;\" << endl;" << endl;
					}
				}
			}

			if (tabinfo.generate_undo)
				strm << "\tquery << \"select undoredo_add_query('insert into " << tabinfo.tablename << " values(" << oldfieldsquery.str() << ");') where undoredo_enabled_callback() = 1;\" << endl;" << endl;
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}

		// generate after delete trigger:
		if (tabinfo.generate_after_delete) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_after_delete_trigger after delete on " << tabinfo.tablename << " begin\" << endl;" << endl;
			strm << "\tquery << \"select raise(abort, 'after delete failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(1, " << oldfieldsnoquotequery.str() << ") = 0;\" << endl;" << endl;
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}

		// generate before update trigger:
		if (tabinfo.generate_before_update) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_before_update_notify_trigger before update on " << tabinfo.tablename << " begin\" << endl;" << endl;
			strm << "\tquery << \"select raise(abort, 'before update failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(12, " << newfieldsquery.str() << ") = 0;\" << endl;" << endl;
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}

		// generate after update trigger:
		if (tabinfo.generate_after_update || tabinfo.generate_undo) {
			strm << "\tquery << \"create temp trigger " << tabinfo.tablename << "_update_notify_trigger after update on " << tabinfo.tablename << " begin\" << endl;" << endl;
			if (tabinfo.generate_after_update) {
				strm << "\tquery << \"select raise(abort, 'after update failed from callback constraint') where " << tabinfo.tablename << "_notify_callback(2, " << newfieldsquery.str() << ", " << oldfieldsnoquotequery.str() << ") = 0;\" << endl;" << endl;
			}
			if (tabinfo.generate_undo)
				strm << "\tquery << \"select undoredo_add_query('update " << tabinfo.tablename << " set " << updatefieldsquery.str() << " where id = '||quote(old.id)||';') where undoredo_enabled_callback() = 1;\" << endl;" << endl;
			strm << "\tquery << \"end;\" << endl;" << endl << endl;
		}
	}

	strm << "}" << endl << endl;

}
