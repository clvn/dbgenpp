#pragma once

struct documentgen;
struct fieldinfo;
struct tableinfo;

struct documentgenparser {
	bool parse_dbgen(const char* jsonfile, documentgen* result);
};

