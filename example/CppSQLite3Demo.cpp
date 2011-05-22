////////////////////////////////////////////////////////////////////////////////
// CppSQLite3 - A C++ wrapper around the SQLite3 embedded database library.
//
// Copyright (c) 2004 Rob Groves. All Rights Reserved. rob.groves@btinternet.com
// 
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose, without fee, and without a written
// agreement, is hereby granted, provided that the above copyright notice, 
// this paragraph and the following two paragraphs appear in all copies, 
// modifications, and distributions.
//
// IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
// INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
// PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
// EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
// ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
// TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
//
// V3.0		03/08/2004	-Initial Version for sqlite3
//
// V3.1		16/09/2004	-Implemented getXXXXField using sqlite3 functions
//						-Added CppSQLiteDB3::tableExists()
////////////////////////////////////////////////////////////////////////////////
#include "CppSQLite3.h"
#include <ctime>
#include <iostream>
#include <unistd.h>
#include "fcntl.h"

using namespace std;

const char* gszFile = "test.db";

extern "C" int sqlite3Corrupt()
{
	return 11;
}

int main(int argc, char** argv)
{
	{
		int i, fld;
		time_t tmStart, tmEnd;
		CppSQLite3DB db;

		cout << "SQLite Version: " << db.SQLiteVersion() << endl;

		::remove(gszFile);
		db.open(gszFile);

		cout << endl << "emp table exists=" << (db.tableExists("emp") ? "TRUE":"FALSE") << endl;
		cout << endl << "Creating emp table" << endl;
		db.execDML("create table emp(empno int, empname char(20));");
		cout << endl << "emp table exists=" << (db.tableExists("emp") ? "TRUE":"FALSE") << endl;
		////////////////////////////////////////////////////////////////////////////////
		// Execute some DML, and print number of rows affected by each one
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "DML tests" << endl;
		int nRows = db.execDML("insert into emp values (7, 'David Beckham');");
		cout << nRows << " rows inserted" << endl;

		nRows = db.execDML("update emp set empname = 'Christiano Ronaldo' where empno = 7;");
		cout << nRows << " rows updated" << endl;

		nRows = db.execDML("delete from emp where empno = 7;");
		cout << nRows << " rows deleted" << endl;

		////////////////////////////////////////////////////////////////////////////////
		// Transaction Demo
		// The transaction could just as easily have been rolled back
		////////////////////////////////////////////////////////////////////////////////
		int nRowsToCreate(1000);
		cout << endl << "Transaction test, creating " << nRowsToCreate;
		cout << " rows please wait..." << endl;
		tmStart = time(0);
		db.execDML("begin transaction;");

		for (i = 0; i < nRowsToCreate; i++)
		{
			char buf[128];
			::sprintf(buf, "insert into emp values (%d, 'Empname%06d');", i, i);
			db.execDML(buf);
		}

		db.execDML("commit transaction;");
		tmEnd = time(0);

		////////////////////////////////////////////////////////////////////////////////
		// Demonstrate CppSQLiteDB::execScalar()
		////////////////////////////////////////////////////////////////////////////////
		cout << db.execScalar("select count(*) from emp;") << " rows in emp table in ";
		cout << tmEnd-tmStart << " seconds (that was fast!)" << endl;

		////////////////////////////////////////////////////////////////////////////////
		// Re-create emp table with auto-increment field
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Auto increment test" << endl;
		db.execDML("drop table emp;");
		db.execDML("create table emp(empno integer primary key, empname char(20));");
		cout << nRows << " rows deleted" << endl;

		for (i = 0; i < 5; i++)
		{
			char buf[128];
			::sprintf(buf, "insert into emp (empname) values ('Empname%06d');", i+1);
			db.execDML(buf);
			cout << " primary key: " << (int)db.lastRowId() << endl;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Query data and also show results of inserts into auto-increment field
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Select statement test" << endl;
		CppSQLite3Query q = db.execQuery("select * from emp order by 1;");

		for (fld = 0; fld < q.numFields(); fld++)
		{
			cout << q.fieldName(fld) << "(" << q.fieldDeclType(fld) << ")|";
		}
		cout << endl;

		while (!q.eof())
		{
			cout << q.fieldValue(0) << "|";
			cout << q.fieldValue(1) << "|" << endl;
			q.nextRow();
		}


		////////////////////////////////////////////////////////////////////////////////
		// SQLite's printf() functionality. Handles embedded quotes and NULLs
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "SQLite sprintf test" << endl;
		CppSQLite3Buffer bufSQL;
		bufSQL.format("insert into emp (empname) values (%Q);", "He's bad");
		cout << (const char*)bufSQL << endl;
		db.execDML(bufSQL);

		bufSQL.format("insert into emp (empname) values (%Q);", NULL);
		cout << (const char*)bufSQL << endl;
		db.execDML(bufSQL);

		////////////////////////////////////////////////////////////////////////////////
		// Fetch table at once, and also show how to use CppSQLiteTable::setRow() method
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "getTable() test" << endl;
		CppSQLite3Table t = db.getTable("select * from emp order by 1;");

		for (fld = 0; fld < t.numFields(); fld++)
		{
			cout << t.fieldName(fld) << "|";
		}
		cout << endl;
		for (int row = 0; row < t.numRows(); row++)
		{
			t.setRow(row);
			for (int fld = 0; fld < t.numFields(); fld++)
			{
				if (!t.fieldIsNull(fld))
					cout << t.fieldValue(fld) << "|";
				else
					cout << "NULL" << "|";
			}
			cout << endl;
		}


		////////////////////////////////////////////////////////////////////////////////
		// Test CppSQLiteBinary by storing/retrieving some binary data, checking
		// it afterwards to make sure it is the same
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Binary data test" << endl;
		db.execDML("create table bindata(desc char(10), data blob);");
    
		unsigned char bin[256];
		CppSQLite3Binary blob;

		for (i = 0; i < sizeof bin; i++)
		{
			bin[i] = i;
		}

		blob.setBinary(bin, sizeof bin);

		bufSQL.format("insert into bindata values ('testing', %Q);", blob.getEncoded());
		db.execDML(bufSQL);
		cout << "Stored binary Length: " << sizeof bin << endl;

		q = db.execQuery("select data from bindata where desc = 'testing';");

		if (!q.eof())
		{
			blob.setEncoded((unsigned char*)q.fieldValue("data"));
			cout << "Retrieved binary Length: " << blob.getBinaryLength() << endl;
		}
		q.finalize();

		const unsigned char* pbin = blob.getBinary();
		for (i = 0; i < sizeof bin; i++)
		{
			if (pbin[i] != i)
			{
				cout << "Problem: i: ," << i << " bin[i]: " << pbin[i] << endl;
			}
		}


		////////////////////////////////////////////////////////////////////////////////
		// Pre-compiled Statements Demo
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Transaction test, creating " << nRowsToCreate;
		cout << " rows please wait..." << endl;
		db.execDML("drop table emp;");
		db.execDML("create table emp(empno int, empname char(20));");
		tmStart = time(0);
		db.execDML("begin transaction;");

		CppSQLite3Statement stmt = db.compileStatement("insert into emp values (?, ?);");
		for (i = 0; i < nRowsToCreate; i++)
		{
			char buf[16];
			::sprintf(buf, "EmpName%06d", i);
			stmt.bind(1, i);
			stmt.bind(2, buf);
			stmt.execDML();
			stmt.reset();
		}

		db.execDML("commit transaction;");
		tmEnd = time(0);

		cout << db.execScalar("select count(*) from emp;") << " rows in emp table in ";
		cout << tmEnd-tmStart << " seconds (that was even faster!)" << endl;
		cout << endl << "End of tests" << endl;
    }

	{
		int row;
		CppSQLite3DB db;

		cout << "SQLite Version: " << db.SQLiteVersion() << endl;

		::remove(gszFile);
		db.open(gszFile);

		////////////////////////////////////////////////////////////////////////////////
		// Demonstrate getStringField(), getIntField(), getFloatField()
		////////////////////////////////////////////////////////////////////////////////
		db.execDML("create table parts(no int, name char(20), qty int, cost number);");
		db.execDML("insert into parts values(1, 'part1', 100, 1.11);");
		db.execDML("insert into parts values(2, null, 200, 2.22);");
		db.execDML("insert into parts values(3, 'part3', null, 3.33);");
		db.execDML("insert into parts values(4, 'part4', 400, null);");

		cout << endl << "CppSQLite3Query getStringField(), getIntField(), getFloatField() tests" << endl;
		CppSQLite3Query q = db.execQuery("select * from parts;");
		while (!q.eof())
		{
			cout << q.getIntField(0) << "|";
			cout << q.getStringField(1) << "|";
			cout << q.getIntField(2) << "|";
			cout << q.getFloatField(3) << "|" << endl;
			q.nextRow();
		}

		cout << endl << "specify NULL values tests" << endl;
		q = db.execQuery("select * from parts;");
		while (!q.eof())
		{
			cout << q.getIntField(0) << "|";
			cout << q.getStringField(1, "NULL") << "|";
			cout << q.getIntField(2, -1) << "|";
			cout << q.getFloatField(3, -3.33) << "|" << endl;
			q.nextRow();
		}

		cout << endl << "Specify fields by name" << endl;
		q = db.execQuery("select * from parts;");
		while (!q.eof())
		{
			cout << q.getIntField("no") << "|";
			cout << q.getStringField("name") << "|";
			cout << q.getIntField("qty") << "|";
			cout << q.getFloatField("cost") << "|" << endl;
			q.nextRow();
		}

		cout << endl << "specify NULL values tests" << endl;
		q = db.execQuery("select * from parts;");
		while (!q.eof())
		{
			cout << q.getIntField("no") << "|";
			cout << q.getStringField("name", "NULL") << "|";
			cout << q.getIntField("qty", -1) << "|";
			cout << q.getFloatField("cost", -3.33) << "|" << endl;
			q.nextRow();
		}
		q.finalize();

		////////////////////////////////////////////////////////////////////////////////
		// Demonstrate getStringField(), getIntField(), getFloatField()
		// But this time on CppSQLite3Table
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "CppSQLite3Table getStringField(), getIntField(), getFloatField() tests" << endl;
		CppSQLite3Table t = db.getTable("select * from parts;");
		for (row = 0; row < t.numRows(); row++)
		{
			t.setRow(row);
			cout << t.getIntField(0) << "|";
			cout << t.getStringField(1) << "|";
			cout << t.getIntField(2) << "|";
			cout << t.getFloatField(3) << "|" << endl;
		}

		cout << endl << "specify NULL values tests" << endl;
		for (row = 0; row < t.numRows(); row++)
		{
			t.setRow(row);
			cout << t.getIntField(0, -1) << "|";
			cout << t.getStringField(1, "NULL") << "|";
			cout << t.getIntField(2, -1) << "|";
			cout << t.getFloatField(3, -3.33) << "|" << endl;
		}

		cout << endl << "Specify fields by name" << endl;
		for (row = 0; row < t.numRows(); row++)
		{
			t.setRow(row);
			cout << t.getIntField("no") << "|";
			cout << t.getStringField("name") << "|";
			cout << t.getIntField("qty") << "|";
			cout << t.getFloatField("cost") << "|" << endl;
		}

		cout << endl << "specify NULL values tests" << endl;
		for (row = 0; row < t.numRows(); row++)
		{
			t.setRow(row);
			cout << t.getIntField("no") << "|";
			cout << t.getStringField("name", "NULL") << "|";
			cout << t.getIntField("qty", -1) << "|";
			cout << t.getFloatField("cost", -3.33) << "|" << endl;
		}


		////////////////////////////////////////////////////////////////////////////////
		// Demonstrate multi-statement DML
		// Note that number of rows affected is only from the last statement
		// when multiple statements are used
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Multi-Statement execDML()" << endl;
		const char* szDML = "insert into parts values(5, 'part5', 500, 5.55);"
							"insert into parts values(6, 'part6', 600, 6.66);"
							"insert into parts values(7, 'part7', 700, 7.77);";
		int nRows = db.execDML(szDML);
		cout << endl << nRows << " rows affected" << endl;
		cout << db.execScalar("select count(*) from parts;") << " rows in parts table" << endl;
					szDML = "delete from parts where no = 2;"
							"delete from parts where no = 3;";
		nRows = db.execDML(szDML);
		cout << endl << nRows << " rows affected" << endl;
		cout << db.execScalar("select count(*) from parts;") << " rows in parts table" << endl;


		////////////////////////////////////////////////////////////////////////////////
		// Demonstrate new typing system & BLOBS
		// ANy data can be stored in any column
		////////////////////////////////////////////////////////////////////////////////
		cout << endl << "Data types and BLOBs()" << endl;
		db.execDML("create table types(no int, "
				"name char(20), qty float, dat blob);");
		db.execDML("insert into types values(null, null, null, null);");
		db.execDML("insert into types values(1, 2, 3, 4);");
		db.execDML("insert into types values(1.1, 2.2, 3.3, 4.4);");
		db.execDML("insert into types values('a', 'b', 'c', 'd');");

		CppSQLite3Statement stmt = db.compileStatement("insert into types values(?,?,?,?);");
		unsigned char buf[256];
		::memset(buf, 1, 1); stmt.bind(1, buf, 1);
		::memset(buf, 2, 2); stmt.bind(2, buf, 2);
		::memset(buf, 3, 3); stmt.bind(3, buf, 3);
		::memset(buf, 4, 4); stmt.bind(4, buf, 4);
		stmt.execDML();
		cout << db.execScalar("select count(*) from types;") << " rows in types table" << endl;

		q = db.execQuery("select * from types;");
		while (!q.eof())
		{
			for (int i = 0; i < q.numFields(); i++)
			{
				switch (q.fieldDataType(i))
				{
				case SQLITE_INTEGER  : cout << "SQLITE_INTEGER|"; break;
				case SQLITE_FLOAT    : cout << "SQLITE_FLOAT  |"; break;
				case SQLITE_TEXT     : cout << "SQLITE_TEXT   |"; break;
				case SQLITE_BLOB     : cout << "SQLITE_BLOB   |"; break;
				case SQLITE_NULL     : cout << "SQLITE_NULL   |"; break;
				default: cout << "***UNKNOWN TYPE***"; break;
				}
			}
			q.nextRow();
			cout << endl;
		}

		nRows = db.execDML("delete from types where no = 1 or no = 1.1 or no = 'a' or no is null;");
		cout << endl << nRows << " rows deleted, leaving binary row only" << endl;

		q = db.execQuery("select * from types;");
		const unsigned char* pBlob;
		int nLen;
		pBlob = q.getBlobField(0, nLen);
		cout << "Field 1 BLOB length: " << nLen << endl;
		pBlob = q.getBlobField(1, nLen);
		cout << "Field 2 BLOB length: " << nLen << endl;
		pBlob = q.getBlobField(2, nLen);
		cout << "Field 3 BLOB length: " << nLen << endl;
		pBlob = q.getBlobField(3, nLen);
		cout << "Field 4 BLOB length: " << nLen << endl;

		q.finalize();

		nRows = db.execDML("delete from types;");
		cout << endl << nRows << " rows deleted, leaving empty table" << endl;
		unsigned char bin[256];

		int i;
		for (i = 0; i < sizeof bin; i++)
		{
			bin[i] = i;
		}

		stmt = db.compileStatement("insert into types values(?,0,0,0);");
		stmt.bind(1, bin, sizeof bin);
		stmt.execDML();

		cout << "Stored binary Length: " << sizeof bin << endl;

		q = db.execQuery("select * from types;");

		pBlob = q.getBlobField(0, nLen);
		cout << "Field 1 BLOB length: " << nLen << endl;

		for (i = 0; i < sizeof bin; i++)
		{
			if (pBlob[i] != i)
			{
				cout << "Problem: i: ," << i << " BLOB[i]: " << pBlob[i] << endl;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////
    // Quit
    ////////////////////////////////////////////////////////////////////////////////
    return 0;
}

