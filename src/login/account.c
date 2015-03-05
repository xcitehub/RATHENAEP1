/**
 * @file account.c
 * Module purpose is to save, load, and update changes into the account table or file.
 * Licensed under GNU GPL.
 *  For more information, see LICENCE in the main folder.
 * @author Athena Dev Teams < r15k
 * @author rAthena Dev Team
 */

#include "../common/malloc.h"
#include "../common/mmo.h"
#include "../common/showmsg.h"
#include "../common/sql.h"
#include "../common/strlib.h"
#include "account.h"
#include <stdlib.h>

/// global defines
#define ACCOUNT_SQL_DB_VERSION 20140928

/// internal structure
typedef struct AccountDB_SQL {
	AccountDB vtable;    // public interface
	Sql* accounts;       // SQL handle accounts storage
	uint16 db_port;
	StringBuf *db_hostname;
	StringBuf *db_username;
	StringBuf *db_password;
	StringBuf *db_database;
	StringBuf *codepage;
	// other settings
	bool case_sensitive;
	//table name
	StringBuf *account_table;
	StringBuf *accreg_table;

} AccountDB_SQL;

/// internal structure
typedef struct AccountDBIterator_SQL {
	AccountDBIterator vtable;    // public interface
	AccountDB_SQL* db;
	int last_account_id;
} AccountDBIterator_SQL;

/// internal functions
static bool account_db_sql_init(AccountDB* self);
static void account_db_sql_destroy(AccountDB* self);
static bool account_db_sql_get_property(AccountDB* self, const char* key, char* buf, size_t buflen);
static bool account_db_sql_set_property(AccountDB* self, const char* option, const char* value);
static bool account_db_sql_create(AccountDB* self, struct mmo_account* acc);
static bool account_db_sql_remove(AccountDB* self, const uint32 account_id);
static bool account_db_sql_save(AccountDB* self, const struct mmo_account* acc);
static bool account_db_sql_load_num(AccountDB* self, struct mmo_account* acc, const uint32 account_id);
static bool account_db_sql_load_str(AccountDB* self, struct mmo_account* acc, const char* userid);
static AccountDBIterator* account_db_sql_iterator(AccountDB* self);
static void account_db_sql_iter_destroy(AccountDBIterator* self);
static bool account_db_sql_iter_next(AccountDBIterator* self, struct mmo_account* acc);

static void account_db_init_conf(AccountDB* self);
static void account_db_destroy_conf(AccountDB* self);
static bool account_db_check_tables(AccountDB* self);

static bool mmo_auth_fromsql(AccountDB_SQL* db, struct mmo_account* acc, uint32 account_id);
static bool mmo_auth_tosql(AccountDB_SQL* db, const struct mmo_account* acc, bool is_new);

/// public constructor
AccountDB* account_db_sql(void) {
	AccountDB_SQL* db = (AccountDB_SQL*)aCalloc(1, sizeof(AccountDB_SQL));

	// set up the vtable
	db->vtable.init         = &account_db_sql_init;
	db->vtable.destroy      = &account_db_sql_destroy;
	db->vtable.get_property = &account_db_sql_get_property;
	db->vtable.set_property = &account_db_sql_set_property;
	db->vtable.save         = &account_db_sql_save;
	db->vtable.create       = &account_db_sql_create;
	db->vtable.remove       = &account_db_sql_remove;
	db->vtable.load_num     = &account_db_sql_load_num;
	db->vtable.load_str     = &account_db_sql_load_str;
	db->vtable.iterator     = &account_db_sql_iterator;
	db->vtable.init_conf    = &account_db_init_conf;
	db->vtable.destroy_conf = &account_db_destroy_conf;
	db->vtable.check_tables = &account_db_check_tables;

	// initialize to default values
	db->accounts            = NULL;
	// other settings
	db->case_sensitive = false;

	return &db->vtable;
}


/* ------------------------------------------------------------------------- */

static void account_db_init_conf(AccountDB* self) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;

	db->db_port       = 3306;
	db->db_hostname   = StringBuf_FromStr("127.0.0.1");
	db->db_username   = StringBuf_FromStr("ragnarok");
	db->db_password   = StringBuf_FromStr("");
	db->db_database   = StringBuf_FromStr("ragnarok");
	db->codepage      = StringBuf_FromStr("");
	db->account_table = StringBuf_FromStr("login");
	db->accreg_table  = StringBuf_FromStr("global_reg_value");
}

static void account_db_destroy_conf(AccountDB* self) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;

	StringBuf_Free(db->db_hostname);
	StringBuf_Free(db->db_username);
	StringBuf_Free(db->db_password);
	StringBuf_Free(db->db_database);
	StringBuf_Free(db->codepage);
	StringBuf_Free(db->account_table);
	StringBuf_Free(db->accreg_table);
}

static bool account_db_check_tables(AccountDB* self) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;

	ShowInfo("Start checking DB integrity (Login)\n");

	// Account table
	if( SQL_ERROR == Sql_Query(db->accounts,
		"SELECT `account_id`, `userid`, `user_pass`, `sex`, `email`, `group_id`, `state`, `unban_time`, `expiration_time`, "
		"`logincount`, `lastlogin`, `last_ip`, `birthdate`, `character_slots`, `pincode`, `pincode_change`, `bank_vault`, "
		"`vip_time`, `old_group` "
		"FROM `%s`;", StringBuf_Value(db->account_table)) )
	{
		Sql_ShowDebug(db->accounts);
		return false;
	}
	// Account registry table
	if( SQL_ERROR == Sql_Query(db->accounts,
		"SELECT `char_id`, `str`, `value`, `type`, `account_id` "
		"FROM `%s`;", StringBuf_Value(db->accreg_table)) )
	{
		Sql_ShowDebug(db->accounts);
		return false;
	}
	return true;
}


/**
 * Establish the database connection.
 * @param self: pointer to db
 */
static bool account_db_sql_init(AccountDB* self) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	Sql* sql_handle;

	db->accounts = Sql_Malloc();
	sql_handle = db->accounts;

	if( SQL_ERROR == Sql_Connect(sql_handle, StringBuf_Value(db->db_username), StringBuf_Value(db->db_password), StringBuf_Value(db->db_hostname), db->db_port, StringBuf_Value(db->db_database)) )
	{
		ShowError("Couldn't connect with uname='%s',passwd='%s',host='%s',port='%d',database='%s'\n",
			StringBuf_Value(db->db_username), StringBuf_Value(db->db_password), StringBuf_Value(db->db_hostname), db->db_port, StringBuf_Value(db->db_database));
		Sql_ShowDebug(sql_handle);
		Sql_Free(db->accounts);
		db->accounts = NULL;
		return false;
	}

	if( StringBuf_Length(db->codepage) && SQL_ERROR == Sql_SetEncoding(sql_handle, StringBuf_Value(db->codepage)) )
		Sql_ShowDebug(sql_handle);

	if (!self->check_tables(self)) {
		ShowFatalError("login-server (login) : A table is missing in sql-server, please fix it, see (sql-files/main.sql for structure) \n");
		exit(EXIT_FAILURE);
	}

	ShowStatus("Login server connection: Database '"CL_WHITE"%s"CL_RESET"' at '"CL_WHITE"%s"CL_RESET"'\n", StringBuf_Value(db->db_database), StringBuf_Value(db->db_hostname));
	return true;
}

/**
 * Destroy the database and close the connection to it.
 * @param self: pointer to db
 */
static void account_db_sql_destroy(AccountDB* self){
	AccountDB_SQL* db = (AccountDB_SQL*)self;

	account_db_destroy_conf(self);
	Sql_Free(db->accounts);
	db->accounts = NULL;
	aFree(db);
}

/**
 * Get configuration information into buf.
 *  If the option is supported, adjust the internal state.
 * @param self: pointer to db
 * @param key: config keyword
 * @param buf: value set of the keyword
 * @param buflen: size of buffer to avoid out of bound
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_get_property(AccountDB* self, const char* key, char* buf, size_t buflen)
{
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	const char* signature;

	signature = "login_server_";
	if( strncmpi(key, signature, strlen(signature)) == 0 ) {
		key += strlen(signature);
		if( strcmpi(key, "ip") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->db_hostname));
		else
		if( strcmpi(key, "port") == 0 )
			safesnprintf(buf, buflen, "%d", db->db_port);
		else
		if( strcmpi(key, "id") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->db_username));
		else
		if(	strcmpi(key, "pw") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->db_password));
		else
		if( strcmpi(key, "db") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->db_database));
		else
		if( strcmpi(key, "account_table") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->account_table));
		else
		if( strcmpi(key, "accreg_table") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->accreg_table));
		else
			return false;// not found
		return true;
	}

	signature = "login_";
	if( strncmpi(key, signature, strlen(signature)) == 0 ) {
		key += strlen(signature);
		if( strcmpi(key, "codepage") == 0 )
			safesnprintf(buf, buflen, "%s", StringBuf_Value(db->codepage));
		else
		if( strcmpi(key, "case_sensitive") == 0 )
			safesnprintf(buf, buflen, "%d", (db->case_sensitive ? 1 : 0));
		else
			return false;// not found
		return true;
	}

	return false;// not found
}

/**
 * Read and set configuration.
 *  If the option is supported, adjust the internal state.
 * @param self: pointer to db
 * @param key: config keyword
 * @param value: config value for keyword
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_set_property(AccountDB* self, const char* key, const char* value) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	const char* signature;

	signature = "login_server_";
	if( strncmp(key, signature, strlen(signature)) == 0 ) {
		key += strlen(signature);
		if( strcmpi(key, "ip") == 0 )
			StringBuf_PrintfClear(db->db_hostname, value);
		else
		if( strcmpi(key, "port") == 0 )
			db->db_port = (uint16)strtoul(value, NULL, 10);
		else
		if( strcmpi(key, "id") == 0 )
			StringBuf_PrintfClear(db->db_username, value);
		else
		if( strcmpi(key, "pw") == 0 )
			StringBuf_PrintfClear(db->db_password, value);
		else
		if( strcmpi(key, "db") == 0 )
			StringBuf_PrintfClear(db->db_database, value);
		else
		if( strcmpi(key, "account_table") == 0 ) {
			StringBuf_PrintfClear(db->account_table, value);
			ShowDebug("set account_table: %s\n", StringBuf_Value(db->account_table));
		}
		else
		if( strcmpi(key, "accreg_table") == 0 ) {
			StringBuf_PrintfClear(db->accreg_table, value);
			ShowDebug("set accreg_table: %s\n", StringBuf_Value(db->accreg_table));
		}
		else
			return false;// not found
		return true;
	}

	signature = "login_";
	if( strncmpi(key, signature, strlen(signature)) == 0 ) {
		key += strlen(signature);
		if( strcmpi(key, "codepage") == 0 )
			StringBuf_PrintfClear(db->codepage, value);
		else
		if( strcmpi(key, "case_sensitive") == 0 )
			db->case_sensitive = (config_switch(value)==1);
		else
			return false;// not found
		return true;
	}

	return false;// not found
}

/**
 * Create a new account entry.
 *  If acc->account_id is -1, the account id will be auto-generated,
 *  and its value will be written to acc->account_id if everything succeeds.
 * @param self: pointer to db
 * @param acc: pointer of mmo_account to save
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_create(AccountDB* self, struct mmo_account* acc) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	Sql* sql_handle = db->accounts;

	// decide on the account id to assign
	uint32 account_id;
	if( acc->account_id != -1 )
	{// caller specifies it manually
		account_id = acc->account_id;
	}
	else
	{// ask the database
		char* data;
		size_t len;

		if( SQL_SUCCESS != Sql_Query(sql_handle, "SELECT MAX(`account_id`)+1 FROM `%s`", StringBuf_Value(db->account_table)) )
		{
			Sql_ShowDebug(sql_handle);
			return false;
		}
		if( SQL_SUCCESS != Sql_NextRow(sql_handle) )
		{
			Sql_ShowDebug(sql_handle);
			Sql_FreeResult(sql_handle);
			return false;
		}

		Sql_GetData(sql_handle, 0, &data, &len);
		account_id = ( data != NULL ) ? atoi(data) : 0;
		Sql_FreeResult(sql_handle);

		if( account_id < START_ACCOUNT_NUM )
			account_id = START_ACCOUNT_NUM;

	}

	// zero value is prohibited
	if( account_id == 0 )
		return false;

	// absolute maximum
	if( account_id > END_ACCOUNT_NUM )
		return false;

	// insert the data into the database
	acc->account_id = account_id;
	return mmo_auth_tosql(db, acc, true);
}

/**
 * Delete an existing account entry and its regs.
 * @param self: pointer to db
 * @param account_id: id of user account
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_remove(AccountDB* self, const uint32 account_id) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	Sql* sql_handle = db->accounts;
	bool result = false;

	if( SQL_SUCCESS != Sql_QueryStr(sql_handle, "START TRANSACTION")
	||  SQL_SUCCESS != Sql_Query(sql_handle, "DELETE FROM `%s` WHERE `account_id` = %d", StringBuf_Value(db->account_table), account_id)
	||  SQL_SUCCESS != Sql_Query(sql_handle, "DELETE FROM `%s` WHERE `account_id` = %d", StringBuf_Value(db->accreg_table), account_id) )
		Sql_ShowDebug(sql_handle);
	else
		result = true;

	result &= ( SQL_SUCCESS == Sql_QueryStr(sql_handle, (result == true) ? "COMMIT" : "ROLLBACK") );

	return result;
}

/**
 * Update an existing account with the new data provided (both account and regs).
 * @param self: pointer to db
 * @param acc: pointer of mmo_account to save
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_save(AccountDB* self, const struct mmo_account* acc) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	return mmo_auth_tosql(db, acc, false);
}

/**
 * Retrieve data from db and store it in the provided data structure.
 *  Filled data structure is done by delegation to mmo_auth_fromsql.
 * @param self: pointer to db
 * @param acc: pointer of mmo_account to fill
 * @param account_id: id of user account
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_load_num(AccountDB* self, struct mmo_account* acc, const uint32 account_id) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	return mmo_auth_fromsql(db, acc, account_id);
}

/**
 * Retrieve data from db and store it in the provided data structure.
 *  Doesn't actually retrieve data yet: escapes and checks userid, then transforms it to accid for fetching.
 *  Filled data structure is done by delegation to account_db_sql_load_num.
 * @param self: pointer to db
 * @param acc: pointer of mmo_account to fill
 * @param userid: name of user account
 * @return true if successful, false if something has failed
 */
static bool account_db_sql_load_str(AccountDB* self, struct mmo_account* acc, const char* userid) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	Sql* sql_handle = db->accounts;
	char esc_userid[2*NAME_LENGTH+1];
	uint32 account_id;
	char* data;

	Sql_EscapeString(sql_handle, esc_userid, userid);

	// get the list of account IDs for this user ID
	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `account_id` FROM `%s` WHERE `userid`= %s '%s'",
		StringBuf_Value(db->account_table), (db->case_sensitive ? "BINARY" : ""), esc_userid) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}

	if( Sql_NumRows(sql_handle) > 1 )
	{// serious problem - duplicit account
		ShowError("account_db_sql_load_str: multiple accounts found when retrieving data for account '%s'!\n", userid);
		Sql_FreeResult(sql_handle);
		return false;
	}

	if( SQL_SUCCESS != Sql_NextRow(sql_handle) )
	{// no such entry
		Sql_FreeResult(sql_handle);
		return false;
	}

	Sql_GetData(sql_handle, 0, &data, NULL);
	account_id = atoi(data);

	return account_db_sql_load_num(self, acc, account_id);
}

/**
 * Create a new forward iterator.
 * @param self: pointer to db iterator
 * @return a new db iterator
 */
static AccountDBIterator* account_db_sql_iterator(AccountDB* self) {
	AccountDB_SQL* db = (AccountDB_SQL*)self;
	AccountDBIterator_SQL* iter = (AccountDBIterator_SQL*)aCalloc(1, sizeof(AccountDBIterator_SQL));

	// set up the vtable
	iter->vtable.destroy = &account_db_sql_iter_destroy;
	iter->vtable.next    = &account_db_sql_iter_next;

	// fill data
	iter->db = db;
	iter->last_account_id = -1;

	return &iter->vtable;
}

/**
 * Destroys this iterator, releasing all allocated memory (including itself).
 * @param self: pointer to db iterator
 */
static void account_db_sql_iter_destroy(AccountDBIterator* self) {
	AccountDBIterator_SQL* iter = (AccountDBIterator_SQL*)self;
	aFree(iter);
}

/**
 * Fetches the next account in the database.
 * @param self: pointer to db iterator
 * @param acc: pointer of mmo_account to fill
 * @return true if next account found and filled, false if something has failed
 */
static bool account_db_sql_iter_next(AccountDBIterator* self, struct mmo_account* acc) {
	AccountDBIterator_SQL* iter = (AccountDBIterator_SQL*)self;
	AccountDB_SQL* db = (AccountDB_SQL*)iter->db;
	Sql* sql_handle = db->accounts;
	char* data;

	// get next account ID
	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `account_id` FROM `%s` WHERE `account_id` > '%d' ORDER BY `account_id` ASC LIMIT 1",
		StringBuf_Value(db->account_table), iter->last_account_id) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}

	if( SQL_SUCCESS == Sql_NextRow(sql_handle) &&
		SQL_SUCCESS == Sql_GetData(sql_handle, 0, &data, NULL) &&
		data != NULL )
	{// get account data
		uint32 account_id;
		account_id = atoi(data);
		if( mmo_auth_fromsql(db, acc, account_id) )
		{
			iter->last_account_id = account_id;
			Sql_FreeResult(sql_handle);
			return true;
		}
	}
	Sql_FreeResult(sql_handle);
	return false;
}

/**
 * Fetch a struct mmo_account from sql.
 * @param db: pointer to db
 * @param acc: pointer of mmo_account to fill
 * @param account_id: id of user account to take data from
 * @return true if successful, false if something has failed
 */
static bool mmo_auth_fromsql(AccountDB_SQL* db, struct mmo_account* acc, uint32 account_id) {
	Sql* sql_handle = db->accounts;
	char* data;
	int i = 0;

	// retrieve login entry for the specified account
	if( SQL_ERROR == Sql_Query(sql_handle,
#ifdef VIP_ENABLE
		"SELECT `account_id`,`userid`,`user_pass`,`sex`,`email`,`group_id`,`state`,`unban_time`,`expiration_time`,`logincount`,`lastlogin`,`last_ip`,`birthdate`,`character_slots`,`pincode`, `pincode_change`, `vip_time`, `old_group` FROM `%s` WHERE `account_id` = %d",
#else
		"SELECT `account_id`,`userid`,`user_pass`,`sex`,`email`,`group_id`,`state`,`unban_time`,`expiration_time`,`logincount`,`lastlogin`,`last_ip`,`birthdate`,`character_slots`,`pincode`, `pincode_change` FROM `%s` WHERE `account_id` = %d",
#endif
		StringBuf_Value(db->account_table), account_id )
	) {
		Sql_ShowDebug(sql_handle);
		return false;
	}

	if( SQL_SUCCESS != Sql_NextRow(sql_handle) )
	{// no such entry
		Sql_FreeResult(sql_handle);
		return false;
	}

	Sql_GetData(sql_handle,  0, &data, NULL); acc->account_id = atoi(data);
	Sql_GetData(sql_handle,  1, &data, NULL); safestrncpy(acc->userid, data, sizeof(acc->userid));
	Sql_GetData(sql_handle,  2, &data, NULL); safestrncpy(acc->pass, data, sizeof(acc->pass));
	Sql_GetData(sql_handle,  3, &data, NULL); acc->sex = data[0];
	Sql_GetData(sql_handle,  4, &data, NULL); safestrncpy(acc->email, data, sizeof(acc->email));
	Sql_GetData(sql_handle,  5, &data, NULL); acc->group_id = atoi(data);
	Sql_GetData(sql_handle,  6, &data, NULL); acc->state = strtoul(data, NULL, 10);
	Sql_GetData(sql_handle,  7, &data, NULL); acc->unban_time = atol(data);
	Sql_GetData(sql_handle,  8, &data, NULL); acc->expiration_time = atol(data);
	Sql_GetData(sql_handle,  9, &data, NULL); acc->logincount = strtoul(data, NULL, 10);
	Sql_GetData(sql_handle, 10, &data, NULL); safestrncpy(acc->lastlogin, data, sizeof(acc->lastlogin));
	Sql_GetData(sql_handle, 11, &data, NULL); safestrncpy(acc->last_ip, data, sizeof(acc->last_ip));
	Sql_GetData(sql_handle, 12, &data, NULL); safestrncpy(acc->birthdate, data, sizeof(acc->birthdate));
	Sql_GetData(sql_handle, 13, &data, NULL); acc->char_slots = atoi(data);
	Sql_GetData(sql_handle, 14, &data, NULL); safestrncpy(acc->pincode, data, sizeof(acc->pincode));
	Sql_GetData(sql_handle, 15, &data, NULL); acc->pincode_change = atol(data);
#ifdef VIP_ENABLE
	Sql_GetData(sql_handle, 16, &data, NULL); acc->vip_time = atol(data);
	Sql_GetData(sql_handle, 17, &data, NULL); acc->old_group = atoi(data);
#endif
	Sql_FreeResult(sql_handle);
	
	// retrieve account regs for the specified user
	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT `str`,`value` FROM `%s` WHERE `type`='1' AND `account_id`='%d'", StringBuf_Value(db->accreg_table), acc->account_id) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}

	acc->account_reg2_num = (int)Sql_NumRows(sql_handle);

	while( SQL_SUCCESS == Sql_NextRow(sql_handle) )
	{
		char* data_tmp;
		Sql_GetData(sql_handle, 0, &data_tmp, NULL); safestrncpy(acc->account_reg2[i].str, data_tmp, sizeof(acc->account_reg2[i].str));
		Sql_GetData(sql_handle, 1, &data_tmp, NULL); safestrncpy(acc->account_reg2[i].value, data_tmp, sizeof(acc->account_reg2[i].value));
		++i;
	}
	Sql_FreeResult(sql_handle);

	if( i != acc->account_reg2_num )
		return false;

	return true;
}

/**
 * Save a struct mmo_account in sql.
 * @param db: pointer to db
 * @param acc: pointer of mmo_account to save
 * @param is_new: if it's a new entry or should we update
 * @return true if successful, false if something has failed
 */
static bool mmo_auth_tosql(AccountDB_SQL* db, const struct mmo_account* acc, bool is_new) {
	Sql* sql_handle = db->accounts;
	SqlStmt* stmt = SqlStmt_Malloc(sql_handle);
	bool result = false;
	int i;

	// try
	do
	{

	if( SQL_SUCCESS != Sql_QueryStr(sql_handle, "START TRANSACTION") )
	{
		Sql_ShowDebug(sql_handle);
		break;
	}

	if( is_new )
	{// insert into account table
		if( SQL_SUCCESS != SqlStmt_Prepare(stmt,
#ifdef VIP_ENABLE
			"INSERT INTO `%s` (`account_id`, `userid`, `user_pass`, `sex`, `email`, `group_id`, `state`, `unban_time`, `expiration_time`, `logincount`, `lastlogin`, `last_ip`, `birthdate`, `character_slots`, `pincode`, `pincode_change`, `vip_time`, `old_group` ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
#else
			"INSERT INTO `%s` (`account_id`, `userid`, `user_pass`, `sex`, `email`, `group_id`, `state`, `unban_time`, `expiration_time`, `logincount`, `lastlogin`, `last_ip`, `birthdate`, `character_slots`, `pincode`, `pincode_change`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
#endif
			StringBuf_Value(db->account_table))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  0, SQLDT_INT,       (void*)&acc->account_id,      sizeof(acc->account_id))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  1, SQLDT_STRING,    (void*)acc->userid,           strlen(acc->userid))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  2, SQLDT_STRING,    (void*)acc->pass,             strlen(acc->pass))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  3, SQLDT_ENUM,      (void*)&acc->sex,             sizeof(acc->sex))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  4, SQLDT_STRING,    (void*)&acc->email,           strlen(acc->email))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  5, SQLDT_INT,       (void*)&acc->group_id,        sizeof(acc->group_id))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  6, SQLDT_UINT,      (void*)&acc->state,           sizeof(acc->state))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  7, SQLDT_LONG,      (void*)&acc->unban_time,      sizeof(acc->unban_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  8, SQLDT_INT,       (void*)&acc->expiration_time, sizeof(acc->expiration_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  9, SQLDT_UINT,      (void*)&acc->logincount,      sizeof(acc->logincount))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 10, SQLDT_STRING,    (void*)&acc->lastlogin,       strlen(acc->lastlogin))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 11, SQLDT_STRING,    (void*)&acc->last_ip,         strlen(acc->last_ip))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 12, SQLDT_STRING,    (void*)&acc->birthdate,       strlen(acc->birthdate))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 13, SQLDT_UCHAR,     (void*)&acc->char_slots,      sizeof(acc->char_slots))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 14, SQLDT_STRING,    (void*)&acc->pincode,         strlen(acc->pincode))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 15, SQLDT_LONG,      (void*)&acc->pincode_change,  sizeof(acc->pincode_change))
#ifdef VIP_ENABLE
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 16, SQLDT_LONG,       (void*)&acc->vip_time,         sizeof(acc->vip_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 17, SQLDT_INT,        (void*)&acc->old_group,        sizeof(acc->old_group))
#endif
		||  SQL_SUCCESS != SqlStmt_Execute(stmt)
		) {
			SqlStmt_ShowDebug(stmt);
			break;
		}
	}
	else
	{// update account table
		if( SQL_SUCCESS != SqlStmt_Prepare(stmt, 
#ifdef VIP_ENABLE
			"UPDATE `%s` SET `userid`=?,`user_pass`=?,`sex`=?,`email`=?,`group_id`=?,`state`=?,`unban_time`=?,`expiration_time`=?,`logincount`=?,`lastlogin`=?,`last_ip`=?,`birthdate`=?,`character_slots`=?,`pincode`=?, `pincode_change`=?, `vip_time`=?, `old_group`=? WHERE `account_id` = '%d'",
#else
			"UPDATE `%s` SET `userid`=?,`user_pass`=?,`sex`=?,`email`=?,`group_id`=?,`state`=?,`unban_time`=?,`expiration_time`=?,`logincount`=?,`lastlogin`=?,`last_ip`=?,`birthdate`=?,`character_slots`=?,`pincode`=?, `pincode_change`=? WHERE `account_id` = '%d'",
#endif
			StringBuf_Value(db->account_table), acc->account_id)
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  0, SQLDT_STRING,    (void*)acc->userid,           strlen(acc->userid))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  1, SQLDT_STRING,    (void*)acc->pass,             strlen(acc->pass))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  2, SQLDT_ENUM,      (void*)&acc->sex,             sizeof(acc->sex))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  3, SQLDT_STRING,    (void*)acc->email,            strlen(acc->email))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  4, SQLDT_INT,       (void*)&acc->group_id,        sizeof(acc->group_id))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  5, SQLDT_UINT,      (void*)&acc->state,           sizeof(acc->state))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  6, SQLDT_LONG,      (void*)&acc->unban_time,      sizeof(acc->unban_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  7, SQLDT_LONG,      (void*)&acc->expiration_time, sizeof(acc->expiration_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  8, SQLDT_UINT,      (void*)&acc->logincount,      sizeof(acc->logincount))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt,  9, SQLDT_STRING,    (void*)&acc->lastlogin,       strlen(acc->lastlogin))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 10, SQLDT_STRING,    (void*)&acc->last_ip,         strlen(acc->last_ip))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 11, SQLDT_STRING,    (void*)&acc->birthdate,       strlen(acc->birthdate))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 12, SQLDT_UCHAR,     (void*)&acc->char_slots,      sizeof(acc->char_slots))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 13, SQLDT_STRING,    (void*)&acc->pincode,         strlen(acc->pincode))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 14, SQLDT_LONG,      (void*)&acc->pincode_change,  sizeof(acc->pincode_change))
#ifdef VIP_ENABLE
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 15, SQLDT_LONG,      (void*)&acc->vip_time,        sizeof(acc->vip_time))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 16, SQLDT_INT,       (void*)&acc->old_group,       sizeof(acc->old_group))
#endif
		||  SQL_SUCCESS != SqlStmt_Execute(stmt)
		) {
			SqlStmt_ShowDebug(stmt);
			break;
		}
	}

	// remove old account regs
	if( SQL_SUCCESS != Sql_Query(sql_handle, "DELETE FROM `%s` WHERE `type`='1' AND `account_id`='%d'", StringBuf_Value(db->accreg_table), acc->account_id) )
	{
		Sql_ShowDebug(sql_handle);
		break;
	}
	// insert new account regs
	if( SQL_SUCCESS != SqlStmt_Prepare(stmt, "INSERT INTO `%s` (`type`, `account_id`, `str`, `value`) VALUES ( 1 , '%d' , ? , ? );",  StringBuf_Value(db->accreg_table), acc->account_id) )
	{
		SqlStmt_ShowDebug(stmt);
		break;
	}
	for( i = 0; i < acc->account_reg2_num; ++i )
	{
		if( SQL_SUCCESS != SqlStmt_BindParam(stmt, 0, SQLDT_STRING, (void*)acc->account_reg2[i].str, strlen(acc->account_reg2[i].str))
		||  SQL_SUCCESS != SqlStmt_BindParam(stmt, 1, SQLDT_STRING, (void*)acc->account_reg2[i].value, strlen(acc->account_reg2[i].value))
		||  SQL_SUCCESS != SqlStmt_Execute(stmt)
		) {
			SqlStmt_ShowDebug(stmt);
			break;
		}
	}
	if( i < acc->account_reg2_num )
	{
		result = false;
		break;
	}

	// if we got this far, everything was successful
	result = true;

	} while(0);
	// finally

	result &= ( SQL_SUCCESS == Sql_QueryStr(sql_handle, (result == true) ? "COMMIT" : "ROLLBACK") );
	SqlStmt_Free(stmt);

	return result;
}
