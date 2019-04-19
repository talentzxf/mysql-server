#define MYSQL_SERVER 1 // required for THD class

#include "my_global.h"
#include "sql_class.h"                          // SSV
#include "sql_base.h"
#include <mysql/plugin.h>
#include <mysql/psi/mysql_file.h>
#include "funny.h"
#include "probes_mysql.h"
#include <algorithm>
#include "my_bitmap.h"
#include <stdlib.h>

Funny_share::Funny_share(){
	thr_lock_init(&lock);
}

Funny_share *ha_funny::get_share()
{
	Funny_share *tmp_share;  

	DBUG_ENTER("ha_example::get_share()");

	lock_shared_ha_data();
	if (!(tmp_share = static_cast<Funny_share*>(get_ha_share_ptr())))
	{
		tmp_share = new Funny_share;
		if (!tmp_share)
			goto err;

		set_ha_share_ptr(static_cast<Handler_share*>(tmp_share));
	}
err:
	unlock_shared_ha_data();
	DBUG_RETURN(tmp_share);
}

ha_funny::ha_funny(handlerton *hton, TABLE_SHARE *table_arg)
	:handler(hton, table_arg), current_position(0)
{
	pTargetTable = new TABLE_LIST();
	pTargetTable->init_one_table("test", 4, "r", 1, "r", TL_READ);
}

static const char *funny_example_exts[] = {
	NullS
};

const char **ha_funny::bas_ext() const
{
	return funny_example_exts;
}

int ha_funny::check(THD* thd, HA_CHECK_OPT* check_opt) {
	DBUG_ENTER("ha_funny::check");
	this->thd = thd;
	DBUG_RETURN(HA_ADMIN_OK);
}

int ha_funny::start_stmt(THD *thd, thr_lock_type lock_type) { 
	this->thd = thd;
	return 0;
}

int ha_funny::reset() {
	DBUG_ENTER("ha_funny::reset");

	int rc = this->pTargetTable->table->file->ha_reset();

	DBUG_RETURN(rc);
}

THR_LOCK_DATA **ha_funny::store_lock(THD *thd,
	THR_LOCK_DATA **to,
enum thr_lock_type lock_type)
{
return pTargetTable->table->file->store_lock(thd, to, lock_type);
}

int ha_funny::open(const char *name, int mode, uint test_if_locked) {

	DBUG_ENTER("ha_example::open");

	uint counter;
	bool open_error = open_tables(this->table->in_use, &pTargetTable, &counter,
		MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL);
	if (!open_error && pTargetTable->is_view_or_derived())
	{
		/*
		Prepare result table for view so that we can read the column list.
		Notice that Show_create_error_handler remains active, so that any
		errors due to missing underlying objects are converted to warnings.
		*/
		open_error = pTargetTable->resolve_derived(thd, true);
	}

	if (!(share = get_share()))
		DBUG_RETURN(1);
	thr_lock_data_init(&share->lock, &lock, NULL);
	DBUG_RETURN(0);
}

int ha_funny::close(void) {
	DBUG_ENTER("ha_funny::close");
	pTargetTable->table->file->ha_close();
	DBUG_RETURN(0);
}

int ha_funny::write_row(uchar *buf) {
	DBUG_ENTER("ha_funny::write_row");
	DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_funny::update_row(const uchar *old_data, uchar *new_data) {
	DBUG_ENTER("ha_funny::update_row");
	DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_funny::delete_row(const uchar *buf) {
	DBUG_ENTER("ha_funny::delete_row");
	DBUG_RETURN(HA_ERR_WRONG_COMMAND);
}

int ha_funny::rnd_init(bool scan) {
	DBUG_ENTER("ha_funny::rnd_init");
	current_position = 0;
	memset(buffer, 0, sizeof(buffer));

	if (!pTargetTable->table->in_use) {
		pTargetTable->table->in_use = this->table->in_use;
	}

	pTargetTable->table->file->ha_rnd_init(scan);

	DBUG_RETURN(0);
}

int ha_funny::rnd_end() {
	DBUG_ENTER("ha_funny::rnd_end");
	int rc = pTargetTable->table->file->ha_rnd_end();
	DBUG_RETURN(rc);
}

int ha_funny::rnd_next(uchar *buf) {
	DBUG_ENTER("ha_funny::rnd_next");

	sql_print_information("Funny engine 1\n");
	memset(buf, 0, table->s->null_bytes);

	my_bitmap_map *org_bitmap = NULL;
	int rc = 0;
	int idx = 0;
	MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
		TRUE);

	if (pTargetTable->table->read_set->n_bits != this->table->read_set->n_bits
		|| pTargetTable->table->write_set->n_bits != this->table->write_set->n_bits) {
		rc = HA_ERR_END_OF_FILE;
		goto end;
	}

	pTargetTable->table->read_set = this->table->read_set;
	pTargetTable->table->write_set = this->table->write_set;

	rc = pTargetTable->table->file->ha_rnd_next(buf);

	if (rc == HA_ERR_END_OF_FILE) {
		goto end;
	}

	org_bitmap = dbug_tmp_use_all_columns(table, table->write_set);
	ha_statistic_increment(&SSV::ha_read_rnd_next_count);

	//// if result is stored in pTargetTable
	//if ( ((*table->field)->ptr - buf) != table->s->null_bytes){
	//	for (Field **field = table->field; *field; field++) {
	//		String buffer;
	//		pTargetTable->table->field[idx]->val_str(&buffer);

	//		(*field)->store(buffer.ptr(), buffer.length(), &my_charset_bin);
	//		idx++;
	//	}
	//}


	current_position = 1;

	stats.records++;
	rc = 0;
	dbug_tmp_restore_column_map(table->write_set, org_bitmap);

end:
	MYSQL_READ_ROW_DONE(rc);	
	DBUG_RETURN(rc);	
}

int ha_funny::rnd_pos(uchar *buf, uchar *pos) {
	DBUG_ENTER("ha_funny::rnd_pos");

	MYSQL_READ_ROW_START(table_share->db.str, table_share->table_name.str,
		FALSE);
	ha_statistic_increment(&SSV::ha_read_rnd_count);

	int rc = pTargetTable->table->file->ha_rnd_pos(buf, pos);

	DBUG_RETURN(rc);
}

void ha_funny::position(const uchar *record) {
	DBUG_ENTER("ha_funny::position");
	DBUG_VOID_RETURN;
}

int ha_funny::info(uint) {
	DBUG_ENTER("ha_funny::info");
	DBUG_RETURN(0);
}

int ha_funny::external_lock(THD *thd, int lock_type) {
	this->thd = thd;
	DBUG_ENTER("ha_funny::external_lock");
	pTargetTable->table->file->ha_external_lock(thd, lock_type);
	DBUG_RETURN(0);
}

int ha_funny::create(const char *name, TABLE *table_arg,
	HA_CREATE_INFO *create_info)
{
	DBUG_ENTER("ha_funny::create");

	sql_print_information("Funny engine can't create table!");

	/*
	This is not implemented but we want someone to be able to see that it
	works.
	*/
	DBUG_RETURN(0);
}


static handler* funny_create_handler(handlerton *hton,
	TABLE_SHARE *table,
	MEM_ROOT *mem_root)
{
	return new (mem_root) ha_funny(hton, table);
}

static int funny_init_func(void *p)
{
	handlerton *funny_hton;

	funny_hton = (handlerton *)p;
	funny_hton->state = SHOW_OPTION_YES;
	funny_hton->create = funny_create_handler;
	funny_hton->flags = HTON_CAN_RECREATE;
	return 0;
}

static int funny_done_func(void *p)
{
	return 0;
}

struct st_mysql_storage_engine funny_storage_engine =
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(funny)
{
	MYSQL_STORAGE_ENGINE_PLUGIN,
		&funny_storage_engine,
		"FUNNY",
		"VincentZhang, Electronic Arts Co.,Ltd",
		"Funny storage engine",
		PLUGIN_LICENSE_GPL,
		funny_init_func, /* Plugin Init */
		funny_done_func, /* Plugin Deinit */
		0x0100 /* 1.0 */,
		NULL,                       /* status variables                */
		NULL,                       /* system variables                */
		NULL,                       /* config options                  */
		0,                          /* flags                           */
}
mysql_declare_plugin_end;