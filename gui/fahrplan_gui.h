/*
 * Dialog window for defining a schedule
 *
 * Hj. Malthaner
 *
 * Juli 2000
 */

#ifndef gui_fahrplan_gui_h
#define gui_fahrplan_gui_h

#include "gui_frame.h"

#include "components/gui_label.h"
#include "components/gui_numberinput.h"
#include "components/gui_combobox.h"
#include "components/gui_button.h"
#include "components/action_listener.h"

#include "components/gui_textarea.h"
#include "components/gui_scrollpane.h"

#include "../convoihandle_t.h"
#include "../linehandle_t.h"
#include "../simwin.h"
#include "../tpl/vector_tpl.h"


class zeiger_t;
class schedule_t;
struct linieneintrag_t;
class spieler_t;
class cbuffer_t;
class karte_t;
class loadsave_t;


class fahrplan_gui_stats_t : public gui_komponente_t
{
private:
	static karte_t *welt;
	static cbuffer_t buf;
	static zeiger_t *aktuell_mark;


	schedule_t* fpl;
	spieler_t* sp;

public:
	fahrplan_gui_stats_t(karte_t* w, spieler_t *s);
	~fahrplan_gui_stats_t();

	void set_fahrplan( schedule_t* f ) { fpl = f; }

	void highlight_schedule( schedule_t *markfpl, bool marking );

	/** Zeichnet die Komponente */
	void zeichnen(koord offset);
};



/**
 * GUI fuer Fahrplaene
 *
 * @author Hj. Malthaner
 */
class fahrplan_gui_t :	public gui_frame_t,
						public action_listener_t
{
 public:
	/**
         * Fills buf with description of schedule's i'th entry.
	 *
	 * @author Hj. Malthaner
	 */
	static void gimme_stop_name(cbuffer_t & buf, karte_t *welt, const spieler_t *sp, const linieneintrag_t &entry, bool no_control_tower = false );

	/**
         * Fills buf with description of schedule's i'th entry.
	 * short version, without loading level and position ...
	 * @author Hj. Malthaner
	 */
	static void gimme_short_stop_name(cbuffer_t & buf, karte_t *welt, const spieler_t *sp, const schedule_t *fpl, int i, int max_chars);

private:
	static char no_line[128];

	enum mode_t {adding, inserting, removing, undefined_mode};

	vector_tpl<linehandle_t> lines;

	mode_t mode;

	// only active with lines
	button_t bt_promote_to_line;
	gui_combobox_t line_selector;
	gui_label_t lb_line;

	// always needed
	button_t bt_add, bt_insert, bt_remove; // stop management
	button_t bt_bidirectional, bt_mirror, bt_always_load, bt_same_spacing_shift;

	button_t bt_wait_prev, bt_wait_next;	// waiting in parts of month
	gui_label_t lb_wait, lb_waitlevel, lb_waitlevel_as_clock;

	gui_label_t lb_load;
	gui_numberinput_t numimp_load;

	gui_label_t lb_spacing;
	gui_numberinput_t numimp_spacing;
	gui_label_t lb_spacing_as_clock;

	gui_label_t lb_spacing_shift;
	gui_numberinput_t numimp_spacing_shift;
	gui_label_t lb_spacing_shift_as_clock;

	char str_ladegrad[16];
	char str_parts_month[32];
	char str_parts_month_as_clock[32];

	char str_spacing_as_clock[32];
	char str_spacing_shift_as_clock[32];

	fahrplan_gui_stats_t stats;
	gui_scrollpane_t scrolly;

	// to add new lines automatically
	uint32 old_line_count;
	uint32 last_schedule_count;

	// set the correct tool now ...
	void update_werkzeug(bool set);

	// changes the waiting/loading levels if allowed
	void update_selection();
protected:
	static karte_t *welt;
	schedule_t *fpl;
	schedule_t* old_fpl;
	spieler_t *sp;
	convoihandle_t cnv;

	linehandle_t new_line, old_line;

public:
	fahrplan_gui_t(schedule_t* fpl, spieler_t* sp, convoihandle_t cnv);

	virtual ~fahrplan_gui_t();

	// for updating info ...
	void init_line_selector();

	bool infowin_event(event_t const*) OVERRIDE;

	const char *get_hilfe_datei() const {return "schedule.txt";}

	/**
	 * Zeichnet das Frame
	 * @author Hansj�rg Malthaner
	 */
	void zeichnen(koord pos, koord gr);

	/**
	 * Set window size and adjust component sizes and/or positions accordingly
	 * @author Hj. Malthaner
	 */
	virtual void set_fenstergroesse(koord groesse);

	/**
	 * show or hide the line selector combobox and its associated label
	 * @author hsiegeln
	 */
	void show_line_selector(bool yesno) {
		line_selector.set_visible(yesno);
		lb_line.set_visible(yesno);
	}

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	/**
	 * Map rotated, rotate schedules too
	 */
	void map_rotate90( sint16 );

	// this contructor is only used during loading
	fahrplan_gui_t(karte_t *welt);

	virtual void rdwr( loadsave_t *file );

	uint32 get_rdwr_id() { return magic_schedule_rdwr_dummy; }
};

#endif
