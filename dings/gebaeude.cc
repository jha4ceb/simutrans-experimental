/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */
#include <string.h>
#include <ctype.h>
#include "../bauer/hausbauer.h"
#include "../gui/money_frame.h"
#include "../simworld.h"
#include "../simdings.h"
#include "../simfab.h"
#include "../simimg.h"
#include "../simgraph.h"
#include "../simhalt.h"
#include "../simwin.h"
#include "../simcity.h"
#include "../player/simplay.h"
#include "../simtools.h"
#include "../simdebug.h"
#include "../simintr.h"
#include "../simskin.h"

#include "../boden/grund.h"


#include "../besch/haus_besch.h"
#include "../besch/intro_dates.h"

#include "../besch/grund_besch.h"

#include "../utils/cbuffer_t.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/translator.h"
#include "../dataobj/einstellungen.h"
#include "../dataobj/umgebung.h"

#include "../gui/thing_info.h"

#include "gebaeude.h"


/**
 * Initializes all variables with save, usable values
 * @author Hj. Malthaner
 */
void gebaeude_t::init()
{
	tile = NULL;
	ptr.fab = 0;
	is_factory = false;
	anim_time = 0;
	sync = false;
	count = 0;
	zeige_baugrube = false;
	snow = false;
	remove_ground = true;
}



gebaeude_t::gebaeude_t(karte_t *welt) : ding_t(welt)
{
	init();
}



gebaeude_t::gebaeude_t(karte_t *welt, loadsave_t *file) : ding_t(welt)
{
	init();
	rdwr(file);
	if(file->get_version()<88002) {
		set_yoff(0);
	}
	if(tile  &&  tile->get_phasen()>1) {
		welt->sync_add( this );
		sync = true;
	}
}



gebaeude_t::gebaeude_t(karte_t *welt, koord3d pos, spieler_t *sp, const haus_tile_besch_t *t) :
    ding_t(welt, pos)
{
	set_besitzer( sp );

	init();
	if(t) 
	{
		set_tile(t);	// this will set init time etc.
		sint64 maint;
		if(tile->get_besch()->get_base_station_maintenance() == 2147483647)
		{
			maint = welt->get_settings().maint_building*tile->get_besch()->get_level();
		}
		else
		{
			maint = tile->get_besch()->get_station_maintenance();
		}
		spieler_t::add_maintenance(get_besitzer(), maint);
	}

	grund_t *gr=welt->lookup(pos);
	if(gr  &&  gr->get_weg_hang()!=gr->get_grund_hang()) {
		set_yoff(-TILE_HEIGHT_STEP);
	}
}



/**
 * Destructor. Removes this from the list of sync objects if neccesary.
 *
 * @author Hj. Malthaner
 */
gebaeude_t::~gebaeude_t()
{
	stadt_t* city = get_stadt();
	if(city) 
	{
		city->remove_gebaeude_from_stadt(this);
	}

	if(sync) {
		sync = false;
		welt->sync_remove(this);
	}

	// tiles might be invalid, if no description is found during loading
	if(tile  &&  tile->get_besch()  &&  tile->get_besch()->ist_ausflugsziel()) 
	{
		welt->remove_ausflugsziel(this);
	}

	count = 0;
	anim_time = 0;
	if(tile) 
	{
		sint64 maint;
		if(tile->get_besch()->get_base_station_maintenance() == 2147483647)
		{
			maint = welt->get_settings().maint_building*tile->get_besch()->get_level();
		}
		else
		{
			maint = tile->get_besch()->get_station_maintenance();
		}
		spieler_t::add_maintenance(get_besitzer(), -maint);
	}
	if(!welt->get_is_shutting_down())
	{
		const weighted_vector_tpl<stadt_t*>& staedte = welt->get_staedte();
		for(weighted_vector_tpl<stadt_t*>::const_iterator j = staedte.begin(), end = staedte.end(); j != end; ++j) 
		{
			(*j)->remove_connected_attraction(this);
		}
	}
}



void gebaeude_t::rotate90()
{
	ding_t::rotate90();

	// must or can rotate?
	const haus_besch_t* const haus_besch = tile->get_besch();
	if (is_factory || haus_besch->get_all_layouts() > 1 || haus_besch->get_b() * haus_besch->get_h() > 1) {
		uint8 layout = tile->get_layout();
		koord new_offset = tile->get_offset();

		if(haus_besch->get_utyp() == haus_besch_t::unbekannt  ||  haus_besch->get_all_layouts()<=4) {
			layout = (layout & 4) + ((layout+3) % haus_besch->get_all_layouts() & 3);
		}
		else {
			static uint8 layout_rotate[16] = { 1, 8, 5, 10, 3, 12, 7, 14, 9, 0, 13, 2, 11, 4, 15, 6 };
			layout = layout_rotate[layout] % haus_besch->get_all_layouts();
		}
		// have to rotate the tiles :(
		if(  !haus_besch->can_rotate()  &&  haus_besch->get_all_layouts() == 1  ) {
			if ((welt->get_settings().get_rotation() & 1) == 0) {
				// rotate 180 degree
				new_offset = koord(haus_besch->get_b() - 1 - new_offset.x, haus_besch->get_h() - 1 - new_offset.y);
			}
			// do nothing here, since we cannot fix it porperly
		}
		else {
			// rotate on ...
			new_offset = koord(haus_besch->get_h(tile->get_layout()) - 1 - new_offset.y, new_offset.x);
		}

		// correct factory zero pos
		if(is_factory  &&  new_offset==koord(0,0)) {
			ptr.fab->set_pos( get_pos() );
		}

		// suche a tile exist?
		if(  haus_besch->get_b(layout) > new_offset.x  &&  haus_besch->get_h(layout) > new_offset.y  ) {
			const haus_tile_besch_t* const new_tile = haus_besch->get_tile(layout, new_offset.x, new_offset.y);
			// add new tile: but make them old (no construction)
			uint32 old_insta_zeit = insta_zeit;
			set_tile( new_tile );
			insta_zeit = old_insta_zeit;
			if(  haus_besch->get_utyp() != haus_besch_t::hafen  &&  !tile->has_image()  ) {
				// may have a rotation, that is not recoverable
				if(  !is_factory  &&  new_offset!=koord(0,0)  ) {
					welt->set_nosave_warning();
				}
				if(  is_factory  &&  (new_offset!=koord(0,0)  ||  ptr.fab->get_besch()->get_haus()->get_tile(0,0,0)==NULL)  ) {
					// there are factories without a valid zero tile
					// => this map rotation cannot be reloaded!
					welt->set_nosave();
				}
			}
		}
		else {
			welt->set_nosave();
		}
	}
}



/* sets the corresponding pointer to a factory
 * @author prissi
 */
void gebaeude_t::set_fab(fabrik_t *fb)
{
	// sets the pointer in non-zero
	if(fb) {
		if(!is_factory  &&  ptr.stadt!=NULL) {
			dbg->fatal("gebaeude_t::set_fab()","building already bound to city!");
		}
		is_factory = true;
		ptr.fab = fb;
	}
	else if(is_factory) {
		ptr.fab = NULL;
	}
}



/* sets the corresponding city
 * @author prissi
 */
void gebaeude_t::set_stadt(stadt_t *s)
{
	if(is_factory  &&  ptr.fab!=NULL) {
		dbg->fatal("gebaeude_t::set_stadt()","building at (%s) already bound to factory!", get_pos().get_str() );
	}
	// sets the pointer in non-zero
	is_factory = false;
	ptr.stadt = s;
}




/* make this building without construction */
void gebaeude_t::add_alter(uint32 a)
{
	insta_zeit -= min(a,insta_zeit);
}




void gebaeude_t::set_tile(const haus_tile_besch_t *new_tile)
{
	insta_zeit = welt->get_zeit_ms();

	if(!zeige_baugrube  &&  tile!=NULL) {
		// mark old tile dirty
		sint16 ypos = 0;
		for(  int i=0;  i<256;  i++  ) {
			image_id bild = get_bild(i);
			if(bild==IMG_LEER) {
				break;
			}
			mark_image_dirty( bild, 0 );
			ypos -= get_tile_raster_width();
		}
	}

	zeige_baugrube = !new_tile->get_besch()->ist_ohne_grube();
	if(sync) {
		if(new_tile->get_phasen()<=1  &&  !zeige_baugrube) {
			// need to stop animation
			welt->sync_remove(this);
			sync = false;
			count = 0;
		}
	}
	else if(new_tile->get_phasen()>1  ||  zeige_baugrube) {
		// needs now animation
		count = simrand(new_tile->get_phasen(), "void gebaeude_t::set_tile");
		anim_time = 0;
		welt->sync_add(this);
		sync = true;
	}
	tile = new_tile;
	remove_ground = tile->has_image()  &&  !tile->get_besch()->ist_mit_boden();
	set_flag(ding_t::dirty);
}




/**
 * Methode f�r Echtzeitfunktionen eines Objekts.
 * @return false wenn Objekt aus der Liste der synchronen
 * Objekte entfernt werden sol
 * @author Hj. Malthaner
 */
bool gebaeude_t::sync_step(long delta_t)
{
	if(zeige_baugrube) {
		// still under construction?
		if(welt->get_zeit_ms() - insta_zeit > 5000) {
			set_flag(ding_t::dirty);
			mark_image_dirty(get_bild(),0);
			zeige_baugrube = false;
			if(tile->get_phasen()<=1) {
				welt->sync_remove( this );
				sync = false;
			}
		}
	}
	else {
		// normal animated building
		anim_time += (uint16)delta_t;
		if(anim_time>tile->get_besch()->get_animation_time()) {
			if(!zeige_baugrube)  {

				// old positions need redraw
				if(  tile->is_hintergrund_phases( snow )  ) {
					// the background is animated
					set_flag(ding_t::dirty);
					// we must take care of the tiles above
					for(int i=1;  ;  i++) {
						image_id bild = get_bild(i);
						if(bild==IMG_LEER) {
							break;
						}
						mark_image_dirty(bild,-(i<<6));
					}
				}
				else {
					// try foreground
					image_id bild = tile->get_vordergrund(count, snow);
					mark_image_dirty(bild,0);
					// next phase must be marked dirty too ...
					bild = tile->get_vordergrund( count+1>=tile->get_phasen()?0:count+1, snow);
					mark_image_dirty(bild,0);
				}

				anim_time %= tile->get_besch()->get_animation_time();
				count ++;
				if(count >= tile->get_phasen()) {
					count = 0;
				}
				// winter for buildings only above snowline
			}
		}
	}
	return true;
}



void gebaeude_t::calc_bild()
{
	grund_t *gr = welt->lookup(get_pos());
	// need no ground?
	if(  remove_ground  &&  gr->get_typ()==grund_t::fundament  ) {
		gr->set_bild( IMG_LEER );
	}
	// snow image?
	snow = 0;
	if(  tile->get_seasons()>1  ) {
		snow = (!gr->ist_tunnel()  ||  gr->ist_karten_boden())  &&  (get_pos().z-(get_yoff()/TILE_HEIGHT_STEP)>= welt->get_snowline());
	}
}


image_id gebaeude_t::get_bild() const
{
	if(umgebung_t::hide_buildings!=0) {
		// opaque houses
		if(get_haustyp()!=unbekannt) {
			return umgebung_t::hide_with_transparency ? skinverwaltung_t::fussweg->get_bild_nr(0) : skinverwaltung_t::construction_site->get_bild_nr(0);
		} else if(  (umgebung_t::hide_buildings == umgebung_t::ALL_HIDDEN_BUIDLING  &&  tile->get_besch()->get_utyp() < haus_besch_t::weitere)  ||  !tile->has_image()) {
			// hide with transparency or tile without information
			if(umgebung_t::hide_with_transparency) {
				if(tile->get_besch()->get_utyp() == haus_besch_t::fabrik  &&  ptr.fab->get_besch()->get_platzierung() == fabrik_besch_t::Wasser) {
					// no ground tiles for water thingies
					return IMG_LEER;
				}
				return skinverwaltung_t::fussweg->get_bild_nr(0);
			}
			else {
				int kind=skinverwaltung_t::construction_site->get_bild_anzahl()<=tile->get_besch()->get_utyp() ? skinverwaltung_t::construction_site->get_bild_anzahl()-1 : tile->get_besch()->get_utyp();
				return skinverwaltung_t::construction_site->get_bild_nr( kind );
			}
		}
	}

	// winter for buildings only above snowline
	if(zeige_baugrube)  {
		return skinverwaltung_t::construction_site->get_bild_nr(0);
	}
	else {
		return tile->get_hintergrund(count, 0, snow);
	}
}


image_id gebaeude_t::get_outline_bild() const
{
	if(umgebung_t::hide_buildings!=0  &&  umgebung_t::hide_with_transparency  &&  !zeige_baugrube) {
		// opaque houses
		return tile->get_hintergrund(count, 0, snow);
	}
	return IMG_LEER;
}


/* gives outline colour and plots background tile if needed for transparent view */
PLAYER_COLOR_VAL gebaeude_t::get_outline_colour() const
{
	COLOR_VAL colours[] = { COL_BLACK, COL_YELLOW, COL_YELLOW, COL_PURPLE, COL_RED, COL_GREEN };
	PLAYER_COLOR_VAL disp_colour = 0;
	if(umgebung_t::hide_buildings!=umgebung_t::NOT_HIDE) {
		if(get_haustyp()!=unbekannt) {
			disp_colour = colours[0] | TRANSPARENT50_FLAG | OUTLINE_FLAG;
		} else if (umgebung_t::hide_buildings == umgebung_t::ALL_HIDDEN_BUIDLING && tile->get_besch()->get_utyp() < haus_besch_t::weitere) {
			// special bilding
			disp_colour = colours[tile->get_besch()->get_utyp()] | TRANSPARENT50_FLAG | OUTLINE_FLAG;
		}
	}
	return disp_colour;
}


image_id gebaeude_t::get_bild(int nr) const
{
	if(zeige_baugrube || umgebung_t::hide_buildings) {
		return IMG_LEER;
	}
	else {
		// winter for buildings only above snowline
		return tile->get_hintergrund(count, nr, snow);
	}
}


image_id gebaeude_t::get_after_bild() const
{
	if(zeige_baugrube) {
		return IMG_LEER;
	}
	if (umgebung_t::hide_buildings != 0 && tile->get_besch()->get_utyp() < haus_besch_t::weitere) {
		return IMG_LEER;
	}
	else {
		// Show depots, station buildings etc.
		return tile->get_vordergrund(count, snow);
	}
}


/*
 * calculate the passenger level as funtion of the city size (if there)
 */
int gebaeude_t::get_passagier_level() const
{
	koord dim = tile->get_besch()->get_groesse();  //("Groesse" = "size")
	long pax = tile->get_besch()->get_level();
	if (!is_factory && ptr.stadt != NULL) {
		// belongs to a city ...
		return ((pax + 6) >> 2) * welt->get_settings().get_passenger_factor() / 16;
	}
	return pax*dim.x*dim.y;
}


int gebaeude_t::get_post_level() const
{
	koord dim = tile->get_besch()->get_groesse();
	long post = tile->get_besch()->get_post_level();
	if (!is_factory && ptr.stadt != NULL) {
		return ((post + 5) >> 2) * welt->get_settings().get_passenger_factor() / 16;
	}
	return post*dim.x*dim.y;
}


/**
 * @return eigener Name oder Name der Fabrik falls Teil einer Fabrik
 * @author Hj. Malthaner
 */
const char *gebaeude_t::get_name() const
{
	if(is_factory  &&  ptr.fab) {
		return ptr.fab->get_name();
	}
	switch(tile->get_besch()->get_typ()) {
		case wohnung:
			break;//return "Wohnhaus";
		case gewerbe:
			break;//return "Gewerbehaus";
		case industrie:
			break;//return "Industriegeb�ude";
		default:
			switch(tile->get_besch()->get_utyp()) {
				case haus_besch_t::attraction_city:   return "Besonderes Gebaeude";
				case haus_besch_t::attraction_land:   return "Sehenswuerdigkeit";
				case haus_besch_t::denkmal:           return "Denkmal";
				case haus_besch_t::rathaus:           return "Rathaus";
				case haus_besch_t::depot:			  return tile->get_besch()->get_name();
				default: break;
			}
			break;
	}
	return "Gebaeude";
}


/**
 * waytype associated with this object
 */
waytype_t gebaeude_t::get_waytype() const
{
	const haus_besch_t *besch = tile->get_besch();
	waytype_t wt = invalid_wt;
	if (besch->get_typ() == gebaeude_t::unbekannt) {
		const haus_besch_t::utyp utype = besch->get_utyp();
		if (utype == haus_besch_t::depot  ||  utype == haus_besch_t::generic_stop  ||  utype == haus_besch_t::generic_extension) {
			wt = (waytype_t)besch->get_extra();
		}
	}
	return wt;
}


bool gebaeude_t::ist_rathaus() const
{
	return tile->get_besch()->ist_rathaus();
}


bool gebaeude_t::is_monument() const
{
	return tile->get_besch()->get_utyp() == haus_besch_t::denkmal;
}


bool gebaeude_t::ist_firmensitz() const
{
	return tile->get_besch()->ist_firmensitz();
}


gebaeude_t::typ gebaeude_t::get_haustyp() const
{
	return tile->get_besch()->get_typ();
}


void gebaeude_t::zeige_info()
{
	// F�r die Anzeige ist bei mehrteiliggen Geb�uden immer
	// das erste laut Layoutreihenfolge zust�ndig.
	// Sonst gibt es f�r eine 2x2-Fabrik bis zu 4 Infofenster.
	koord k = tile->get_offset();
	if(k != koord(0, 0)) {
		grund_t *gr = welt->lookup(get_pos() - k);
		if(!gr) {
			gr = welt->lookup_kartenboden(get_pos().get_2d() - k);
		}
		gebaeude_t* gb = gr->find<gebaeude_t>();
		// is the info of the (0,0) tile on multi tile buildings
		if(gb) {
			// some version made buildings, that had not tile (0,0)!
			gb->zeige_info();
		}
	}
	else {
DBG_MESSAGE("gebaeude_t::zeige_info()", "at %d,%d - name is '%s'", get_pos().x, get_pos().y, get_name());

		if(get_fabrik()) {
			ptr.fab->zeige_info();
		}
		else if(ist_firmensitz()) {
			int old_count = win_get_open_count();
			create_win( new money_frame_t(get_besitzer()), w_info, magic_finances_t+get_besitzer()->get_player_nr() );
			// already open?
			if(umgebung_t::townhall_info  &&  old_count==win_get_open_count()) {
				create_win( new ding_infowin_t(this), w_info, (long)this);
			}
		}
		else if(!tile->get_besch()->ist_ohne_info()) {
			if(ist_rathaus()) {
				int old_count = win_get_open_count();
				welt->suche_naechste_stadt(get_pos().get_2d())->zeige_info();
				// already open?
				if(umgebung_t::townhall_info  &&  old_count==win_get_open_count()) {
					create_win( new ding_infowin_t(this), w_info, (long)this);
				}
			}
			else {
				create_win( new ding_infowin_t(this), w_info, (long)this);
			}
		}
	}
}



void gebaeude_t::info(cbuffer_t & buf) const
{
	ding_t::info(buf);

	if(is_factory  &&  ptr.fab != NULL) {
		buf.append((char *)0);
	}
	else if(zeige_baugrube) {
		buf.append(translator::translate("Baustelle"));
		buf.append("\n");
	}
	else {
		const char *desc = tile->get_besch()->get_name();
		if(desc != NULL) {
			const char *trans_desc = translator::translate(desc);
			if(trans_desc==desc) {
				// no descrition here
				switch(get_haustyp()) {
					case wohnung:
						trans_desc = translator::translate("residential house");
						break;
					case industrie:
						trans_desc = translator::translate("industrial building");
						break;
					case gewerbe:
						trans_desc = translator::translate("shops and stores");
						break;
					default:
						// use file name
						break;
				}
				buf.append(trans_desc);
			}
			else {
				// since the format changed, we remove all but double newlines
				char *text = new char[strlen(trans_desc)+1];
				char *dest = text;
				const char *src = trans_desc;
				while(  *src!=0  ) {
					*dest = *src;
					if(src[0]=='\n') {
						if(src[1]=='\n') {
							src ++;
							dest++;
							*dest = '\n';
						}
						else {
							*dest = ' ';
						}
					}
					src ++;
					dest ++;
				}
				// remove double line breaks at the end
				*dest = 0;
				while( dest>text  &&  *--dest=='\n'  ) {
					*dest = 0;
				}

				buf.append(text);
				delete [] text;
			}
		}
		buf.append( "\n" );

		// belongs to which city?
		/*if (!is_factory && ptr.stadt != NULL) {
			buf.printf(translator::translate("Town: %s\n"), ptr.stadt->get_name());
		}*/

		if( get_tile()->get_besch()->get_utyp() < haus_besch_t::bahnhof ) {
			buf.printf("%s: %d\n", translator::translate("Passagierrate"), get_passagier_level());
			buf.printf("%s: %d\n", translator::translate("Postrate"),      get_post_level());
		}

		haus_besch_t const& h = *tile->get_besch();
		buf.printf("%s%u", translator::translate("\nBauzeit von"), h.get_intro_year_month() / 12);
		if (h.get_retire_year_month() != DEFAULT_RETIRE_DATE * 12) {
			buf.printf("%s%u", translator::translate("\nBauzeit bis"), h.get_retire_year_month() / 12);
		}

		buf.append("\n");
		if(get_besitzer()==NULL) {
			buf.append("\n");
			buf.append(translator::translate("Wert"));
			buf.append(": ");
			buf.append(-(welt->get_settings().cst_buy_land*(tile->get_besch()->get_level()+1)/100) * 5);
			buf.append("$\n");
		}

		if (char const* const maker = tile->get_besch()->get_copyright()) {
			buf.append("\n");
			buf.printf(translator::translate("Constructed by %s"), maker);
		}
	}
}


void gebaeude_t::rdwr(loadsave_t *file)
{
	// do not save factory buildings => factory will reconstruct them
	assert(!is_factory);
	xml_tag_t d( file, "gebaeude_t" );

	ding_t::rdwr(file);

	char buf[128];
	short idx;

	if(file->is_saving()) {
		const char *s = tile->get_besch()->get_name();
		file->rdwr_str(s);
		idx = tile->get_index();
	}
	else {
		file->rdwr_str(buf, lengthof(buf));
	}
	file->rdwr_short(idx);
	if(file->get_experimental_version() <= 1)
	{
		uint32 old_insta_zeit = (uint32) insta_zeit;
		file->rdwr_long(old_insta_zeit);
		insta_zeit = old_insta_zeit;
	}
	else
	{
		file->rdwr_longlong(insta_zeit);
	}

	if(file->is_loading()) {
		tile = hausbauer_t::find_tile(buf, idx);
		if(tile==NULL) {
			// try with compatibility list first
			tile = hausbauer_t::find_tile(translator::compatibility_name(buf), idx);
			if(tile==NULL) {
				DBG_MESSAGE("gebaeude_t::rdwr()","neither %s nor %s, tile %i not found, try other replacement",translator::compatibility_name(buf),buf,idx);
			}
			else {
				DBG_MESSAGE("gebaeude_t::rdwr()","%s replaced by %s, tile %i",buf,translator::compatibility_name(buf),idx);
			}
		}
		if(tile==NULL) {
			// first check for special buildings
			if(strstr(buf,"TrainStop")!=NULL) {
				tile = hausbauer_t::find_tile("TrainStop", idx);
			} else if(strstr(buf,"BusStop")!=NULL) {
				tile = hausbauer_t::find_tile("BusStop", idx);
			} else if(strstr(buf,"ShipStop")!=NULL) {
				tile = hausbauer_t::find_tile("ShipStop", idx);
			} else if(strstr(buf,"PostOffice")!=NULL) {
				tile = hausbauer_t::find_tile("PostOffice", idx);
			} else if(strstr(buf,"StationBlg")!=NULL) {
				tile = hausbauer_t::find_tile("StationBlg", idx);
			}
			else {
				// try to find a fitting building
				int level=atoi(buf);
				gebaeude_t::typ type = gebaeude_t::unbekannt;

				if(level>0) {
					// May be an old 64er, so we can try some
					if(strncmp(buf+3,"WOHN",4)==0) {
						type = gebaeude_t::wohnung;
					} else if(strncmp(buf+3,"FAB",3)==0) {
						type = gebaeude_t::industrie;
					}
					else {
						type = gebaeude_t::gewerbe;
					}
					level --;
				}
				else if(buf[3]=='_') {
					/* should have the form of RES/IND/COM_xx_level
					 * xx is usually a number by can be anything without underscores
					 */
					level = atoi(strrchr( buf, '_' )+1);
					if(level>0) {
						switch(toupper(buf[0])) {
							case 'R': type = gebaeude_t::wohnung; break;
							case 'I': type = gebaeude_t::industrie; break;
							case 'C': type = gebaeude_t::gewerbe; break;
						}
					}
					level --;
				}
				// we try to replace citybuildings with their matching counterparts
				// if none are matching, we try again without climates and timeline!
				switch(type) {
					case gebaeude_t::wohnung:
						{
							const haus_besch_t *hb = hausbauer_t::get_wohnhaus(level,welt->get_timeline_year_month(),welt->get_climate(get_pos().z));
							if(hb==NULL) {
								hb = hausbauer_t::get_wohnhaus(level,0, MAX_CLIMATES );
							}
							if( hb) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with residence level %i by %s",buf,level,hb->get_name());
								tile = hb->get_tile(0);
							}
						}
						break;

					case gebaeude_t::gewerbe:
						{
							const haus_besch_t *hb = hausbauer_t::get_gewerbe(level,welt->get_timeline_year_month(),welt->get_climate(get_pos().z));
							if(hb==NULL) {
								hb = hausbauer_t::get_gewerbe(level,0, MAX_CLIMATES );
							}
							if(hb) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with commercial level %i by %s",buf,level,hb->get_name());
								tile = hb->get_tile(0);
							}
						}
						break;

					case gebaeude_t::industrie:
						{
							const haus_besch_t *hb = hausbauer_t::get_industrie(level,welt->get_timeline_year_month(),welt->get_climate(get_pos().z));
							if(hb==NULL) {
								hb = hausbauer_t::get_industrie(level,0, MAX_CLIMATES );
								if(hb==NULL) {
									hb = hausbauer_t::get_gewerbe(level,0, MAX_CLIMATES );
								}
							}
							if (hb) {
								dbg->message("gebaeude_t::rwdr", "replace unknown building %s with industrie level %i by %s",buf,level,hb->get_name());
								tile = hb->get_tile(0);
							}
						}
						break;

					default:
						dbg->warning("gebaeude_t::rwdr", "description %s for building at %d,%d not found (will be removed)!", buf, get_pos().x, get_pos().y);
						welt->add_missing_paks( buf, karte_t::MISSING_BUILDING );
				}
			}
		}	// here we should have a valid tile pointer or nothing ...

		/* avoid double contruction of monuments:
		 * remove them from selection lists
		 */
		if (tile  &&  tile->get_besch()->get_utyp() == haus_besch_t::denkmal) {
			hausbauer_t::denkmal_gebaut(tile->get_besch());
		}
		if (tile) {
			remove_ground = tile->has_image()  &&  !tile->get_besch()->ist_mit_boden();
		}
	}

	if(file->get_version()<99006) {
		// ignore the sync flag
		uint8 dummy=sync;
		file->rdwr_byte(dummy);
	}

	// restore city pointer here
	if(  file->get_version()>=99014  ) {
		sint32 city_index = -1;
		if(  file->is_saving()  &&  ptr.stadt!=NULL  ) {
			city_index = welt->get_staedte().index_of( ptr.stadt );
		}
		file->rdwr_long(city_index);
		if(  file->is_loading()  &&  city_index!=-1  &&  (tile==NULL  ||  tile->get_besch()==NULL  ||  tile->get_besch()->is_connected_with_town())  ) {
			ptr.stadt = welt->get_staedte()[city_index];
		}
	}

	if(file->is_loading()) {
		count = 0;
		anim_time = 0;
		sync = false;

		// Hajo: rebuild tourist attraction list
		if(tile && tile->get_besch()->ist_ausflugsziel()) {
			welt->add_ausflugsziel( this );
		}
	}
}



/**
 * Wird nach dem Laden der Welt aufgerufen - �blicherweise benutzt
 * um das Aussehen des Dings an Boden und Umgebung anzupassen
 * 
 * "After loading is called adapting to the world - normally used to the
 * look of the thing in the ground and surrounding area" (Google)
 *
 * @author Hj. Malthaner
 */
void gebaeude_t::laden_abschliessen()
{
	calc_bild();
	sint64 maint = tile->get_besch()->get_station_maintenance();
	if(maint == 2147483647) 
	{
		maint = welt->get_settings().maint_building*tile->get_besch()->get_level();
	}
	spieler_t::add_maintenance(get_besitzer(), maint);

	// citybuilding, but no town?
	if(  tile->get_offset()==koord(0,0)  ) {
		if(  tile->get_besch()->is_connected_with_town()  ) {
			stadt_t *city = (ptr.stadt==NULL) ? welt->suche_naechste_stadt( get_pos().get_2d() ) : ptr.stadt;
			if(city) {
				city->add_gebaeude_to_stadt(this);
			}
		}
		else if(  !is_factory  ) {
			ptr.stadt = NULL;
		}
	}
}



void gebaeude_t::entferne(spieler_t *sp)
{
//	DBG_MESSAGE("gebaeude_t::entferne()","gb %i");
	// remove costs

	const haus_besch_t* besch = tile->get_besch();
	sint64 cost = 0;

	if(besch->get_utyp()<haus_besch_t::bahnhof) 
	{
		const sint64 bulldoze_cost =welt->get_settings().cst_multiply_remove_haus * (besch->get_level()+1);
		const sint64 purchase_cost =welt->get_settings().cst_buy_land * besch->get_level() * 5;
		cost = sp != get_besitzer() ? bulldoze_cost + purchase_cost : bulldoze_cost;
		spieler_t::accounting(sp, cost, get_pos().get_2d(), COST_CONSTRUCTION);
	}
	else 
	{
		// tearing down halts is always single costs only
		cost = besch->get_station_price();
		// This check is necessary because the number of 2147483647 is used if no price is specified. 
		if(besch->get_base_station_price() == 2147483647)
		{
			// TODO: find a way of checking what *kind* of stop that this is. This assumes railway.
			cost = welt->get_settings().cst_multiply_station * besch->get_level();
		}
		// Should be cheaper to bulldoze than build. 
		spieler_t::accounting(sp, -(cost / 1.5F), get_pos().get_2d(), COST_CONSTRUCTION);
	}

	// may need to update next buildings, in the case of start, middle, end buildings
	if(besch->get_all_layouts()>1  &&  get_haustyp()==unbekannt) {

		// realign surrounding buildings...
		uint32 layout = tile->get_layout();

		// detect if we are connected at far (north/west) end
		grund_t * gr = welt->lookup( get_pos() );
		if(gr) {
			sint8 offset = gr->get_weg_yoff()/TILE_HEIGHT_STEP;
			gr = welt->lookup( get_pos()+koord3d( (layout & 1 ? koord::ost : koord::sued), offset) );
			if(!gr) {
				// check whether bridge end tile
				grund_t * gr_tmp = welt->lookup( get_pos()+koord3d( (layout & 1 ? koord::ost : koord::sued),offset - 1) );
				if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 1) {
					gr = gr_tmp;
				}
			}
			if(gr) {
				gebaeude_t* gb = gr->find<gebaeude_t>();
				if(gb  &&  gb->get_tile()->get_besch()->get_all_layouts()>4u) {
					koord xy = gb->get_tile()->get_offset();
					uint8 layoutbase = gb->get_tile()->get_layout();
					if((layoutbase & 1u) == (layout & 1u)) {
						layoutbase |= 4u; // set far bit on neighbour
						gb->set_tile(gb->get_tile()->get_besch()->get_tile(layoutbase, xy.x, xy.y));
					}
				}
			}

			// detect if near (south/east) end
			gr = welt->lookup( get_pos()+koord3d( (layout & 1 ? koord::west : koord::nord), offset) );
			if(!gr) {
				// check whether bridge end tile
				grund_t * gr_tmp = welt->lookup( get_pos()+koord3d( (layout & 1 ? koord::west : koord::nord),offset - 1) );
				if(gr_tmp && gr_tmp->get_weg_yoff()/TILE_HEIGHT_STEP == 1) {
					gr = gr_tmp;
				}
			}
			if(gr) {
				gebaeude_t* gb = gr->find<gebaeude_t>();
				if(gb  &&  gb->get_tile()->get_besch()->get_all_layouts()>4) {
					koord xy = gb->get_tile()->get_offset();
					uint8 layoutbase = gb->get_tile()->get_layout();
					if((layoutbase & 1u) == (layout & 1u)) {
						layoutbase |= 2u; // set near bit on neighbour
						gb->set_tile(gb->get_tile()->get_besch()->get_tile(layoutbase, xy.x, xy.y));
					}
				}
			}
		}
	}
	mark_images_dirty();
}

void gebaeude_t::mark_images_dirty() const
{
	// remove all traces from the screen
	image_id img;
	if(  zeige_baugrube  ||
			(!umgebung_t::hide_with_transparency  &&
				umgebung_t::hide_buildings>(get_haustyp()!=unbekannt ? umgebung_t::NOT_HIDE : umgebung_t::SOME_HIDDEN_BUIDLING))  ) {
		img = skinverwaltung_t::construction_site->get_bild_nr(0);
	}
	else {
		img = tile->get_hintergrund(count, 0, snow) ;
	}
	for(  int i=0;  img!=IMG_LEER;  img=get_bild(++i)  ) {
		mark_image_dirty( img, -(i<<6) );
	}
}
