#include "my_global.h"                   /* ulonglong */
#include "thr_lock.h"                    /* THR_LOCK, THR_LOCK_DATA */
#include "handler.h"                     /* handler */
#include "my_base.h"                     /* ha_rows */

class Funny_share : public Handler_share {
public:
	THR_LOCK lock;
	Funny_share();
	~Funny_share()
	{
		thr_lock_delete(&lock);
	}
};

extern void sql_print_information(const char *format, ...);

/** @brief
Class definition for the storage engine
*/
class ha_funny : public handler
{
private:
	TABLE_LIST* pTargetTable;
	THD* thd;
	THR_LOCK_DATA lock;      ///< MySQL lock
	Funny_share *share;    ///< Shared lock info
	Funny_share *get_share(); ///< Get the share
	my_off_t current_position;
	char buffer[4096];

public:
	ha_funny(handlerton *hton, TABLE_SHARE *table_arg);
	~ha_funny()
	{
	}

	TABLE_LIST* getTarget() {
		return this->pTargetTable;
	}

	const char *table_type() const { return "FUNNY"; }
	const char *index_type(uint inx) { return "NONE"; }
	const char **bas_ext() const;

	ulonglong table_flags() const
	{
		return HA_BINLOG_STMT_CAPABLE;
	}

	ulong index_flags(uint inx, uint part, bool all_parts) const
	{
		return 0;
	}

	uint max_supported_record_length() const { return HA_MAX_REC_LENGTH; }

	uint max_supported_keys()          const { return 0; }

	uint max_supported_key_parts()     const { return 0; }

	uint max_supported_key_length()    const { return 0; }

	virtual double scan_time() { return (double)(stats.records + stats.deleted) / 20.0 + 10; }

	virtual double read_time(uint, uint, ha_rows rows)
	{
		return (double)rows / 20.0 + 1;
	}

	int open(const char *name, int mode, uint test_if_locked);	  
	int close(void);
	int write_row(uchar *buf);
	int update_row(const uchar *old_data, uchar *new_data);
	int delete_row(const uchar *buf);
	int check(THD* thd, HA_CHECK_OPT* check_opt);

	int rnd_init(bool scan);                                    
	int rnd_end();
	int rnd_next(uchar *buf);                                   
	int rnd_pos(uchar *buf, uchar *pos);                        
	void position(const uchar *record);                         
	int info(uint);                                             
	int external_lock(THD *thd, int lock_type);                 
	int start_stmt(THD *thd, thr_lock_type lock_type);
	int create(const char *name, TABLE *form,
		HA_CREATE_INFO *create_info);                      ///< required
	int reset();

	THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
		enum thr_lock_type lock_type);     ///< required
};