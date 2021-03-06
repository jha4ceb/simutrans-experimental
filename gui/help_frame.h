/*
 * Copyright (c) 1997 - 2002 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef gui_help_frame_h
#define gui_help_frame_h

#include "gui_frame.h"
#include "components/gui_scrollpane.h"
#include "components/gui_flowtext.h"
#include "components/action_listener.h"

class help_frame_t : public gui_frame_t, action_listener_t
{
private:
	gui_scrollpane_t scrolly;
	gui_flowtext_t flow;

public:
	help_frame_t();
	help_frame_t(char const* filename);

	void set_text(const char * text);


	/**
	 * resize window in response to a resize event
	 * @author Hj. Malthaner
	 */
	void resize(const koord delta);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
