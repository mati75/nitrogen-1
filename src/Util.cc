/*

This file is from Nitrogen, an X11 background setter.  
Copyright (C) 2006  Dave Foster & Javeed Shaikh

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include "Util.h"
#include "SetBG.h"
#include "Config.h"
#include <iostream>
#include <stdio.h>
#include "gcs-i18n.h"

namespace Util {

// parafarmance testing / bug finding
// http://primates.ximian.com/~federico/news-2006-03.html#09
//
// Must compile with --enable-debug for this function to have a body.
void program_log (const char *format, ...)
{
#ifdef DEBUG
	va_list args;
    char *formatted, *str;

    va_start (args, format);
    formatted = g_strdup_vprintf (format, args);
    va_end (args);

    str = g_strdup_printf ("MARK: %s: %s", g_get_prgname(), formatted);
    g_free (formatted);

	access (str, F_OK);
    g_free (str);
#endif    
} 

// Converts a relative path to an absolute path.
// Probably works best if the path doesn't start with a '/' :)
// XXX: There must be a better way to do this, haha.
Glib::ustring path_to_abs_path(Glib::ustring path) {
	unsigned parents = 0;
	Glib::ustring cwd = Glib::get_current_dir();
	Glib::ustring::size_type pos;

	// find out how far up we have to go.
	while( (pos = path.find("../", 0)) !=Glib::ustring::npos ) {
		path.erase(0, 3);
		parents++;
	}
	
	// now erase that many directories from the path to the current
	// directory.
	
	while( parents ) {
		if ( (pos = cwd.rfind ('/')) != Glib::ustring::npos) {
			// remove this directory from the path
			cwd.erase (pos);
		}
		parents--;
	}
	
	return cwd + '/' + path;
}

//
// TODO: i turned this kind of into a shitshow with the xinerama, must redo a bit
//
void restore_saved_bgs() {

	program_log("entering set_saved_bgs()");
	
	Glib::ustring file, display;
	SetBG::SetMode mode;
	Gdk::Color bgcolor;
	
	std::vector<Glib::ustring> displist;
	std::vector<Glib::ustring>::const_iterator i;
	Config *cfg = Config::get_instance();

	if ( cfg->get_bg_groups(displist) == false ) {
		std::cerr << _("Could not get bg groups from config file.") << std::endl;
		return;
	}

	// determine what mode we are going into
	Glib::RefPtr<Gdk::DisplayManager> manager = Gdk::DisplayManager::get();
	Glib::RefPtr<Gdk::Display> disp	= manager->get_default_display();
	
#ifdef USE_XINERAMA
	XineramaScreenInfo* xinerama_info;
	gint xinerama_num_screens;

	int event_base, error_base;
	int xinerama = XineramaQueryExtension(GDK_DISPLAY_XDISPLAY(disp->gobj()), &event_base, &error_base);
	if (xinerama && XineramaIsActive(GDK_DISPLAY_XDISPLAY(disp->gobj()))) {
		xinerama_info = XineramaQueryScreens(GDK_DISPLAY_XDISPLAY(disp->gobj()), &xinerama_num_screens);

		// dirty, dirty hack to make sure we're not on some crazy 1 monitor display
		if (xinerama_num_screens > 1) {
			program_log("using xinerama");

			bool foundfull = false;
			for (i=displist.begin(); i!=displist.end(); i++)
				if ((*i) == Glib::ustring("xin_-1"))
					foundfull = true;

			if (foundfull) {
				if (cfg->get_bg(Glib::ustring("xin_-1"), file, mode, bgcolor)) {
					program_log("setting full xinerama screen to %s", file.c_str());
					SetBG::set_bg_xinerama(xinerama_info, xinerama_num_screens, Glib::ustring("xin_-1"), file, mode, bgcolor); 
					program_log("done setting full xinerama screen");
				} else {
					std::cerr << _("Could not get bg info for fullscreen xinerama") << std::endl;
				}
			} else {

				// iterate through, pick out the xin_*
				for (i=displist.begin(); i!=displist.end(); i++) {
					if ((*i).find(Glib::ustring("xin_").c_str(), 0, 4) != Glib::ustring::npos) {
						if (cfg->get_bg((*i), file, mode, bgcolor)) {
							program_log("setting %s to %s", (*i).c_str(), file.c_str());
							SetBG::set_bg_xinerama(xinerama_info, xinerama_num_screens, (*i), file, mode, bgcolor);
							program_log("done setting %s", (*i).c_str());
						} else {
							std::cerr << _("Could not get bg info for") << " " << (*i) << std::endl;
						}
					}
				}
			}
		
			// must return here becuase not all systems have xinerama
			program_log("leaving set_saved_bgs()");	
			return;
		}
	} 
#endif

	for (int n=0; n<disp->get_n_screens(); n++) {
				
		display = disp->get_screen(n)->make_display_name();

		program_log("display: %s", display.c_str());
			
		if (cfg->get_bg(display, file, mode, bgcolor)) {
					
			program_log("setting bg on %s to %s (mode: %d)", display.c_str(), file.c_str(), mode);
			SetBG::set_bg(display, file, mode, bgcolor);
			program_log("set bg on %s to %s (mode: %d)", display.c_str(), file.c_str(), mode);
				
		} else {
			std::cerr << _("Could not get bg info") << std::endl;
		}
	}

	program_log("leaving set_saved_bgs()");	
}

/**
 * Creates an instance of an ArgParser with all options set up.
 *
 * @returns		An ArgParser instance.
 */
ArgParser* create_arg_parser() {

	ArgParser* parser = new ArgParser();
	parser->register_option("restore", _("Restore saved backgrounds"));
	parser->register_option("no-recurse", _("Do not recurse into subdirectories"));
	parser->register_option("sort", _("How to sort the backgrounds. Valid options are:\n\t\t\t* alpha, for alphanumeric sort\n\t\t\t* ralpha, for reverse alphanumeric sort\n\t\t\t* time, for last modified time sort (oldest first)\n\t\t\t* rtime, for reverse last modified time sort (newest first)"), true);

	// command line set modes
	Glib::ustring openp(" (");
	Glib::ustring closep(")");
	parser->register_option("set-scaled", _("Sets the background to the given file") + openp + _("scaled") + closep);
	parser->register_option("set-tiled", _("Sets the background to the given file") + openp + _("tiled") + closep);
	parser->register_option("set-best", _("Sets the background to the given file") + openp + _("best-fit") + closep);
	parser->register_option("set-centered", _("Sets the background to the given file") + openp + _("centered") + closep);
	parser->register_option("save", _("Saves the background permanently"));
	
	std::vector<std::string> vecsetopts;
	vecsetopts.push_back("set-scaled");
	vecsetopts.push_back("set-tiled");
	vecsetopts.push_back("set-best");
	vecsetopts.push_back("set-centered");
	parser->make_exclusive(vecsetopts);

	return parser;
}

/**
 * Prepares the inputted start dir to the conventions needed throughout the program.
 *
 * Changes a relative dir to a absolute dir, trims terminating slash.
 */
std::string fix_start_dir(std::string startdir) {

	// trims the terminating '/'. if it exists
	if ( startdir[startdir.length()-1] == '/' ) {
		startdir.resize(startdir.length()-1);
	}
	
	// if this is not an absolute path, make it one.
	if ( !Glib::path_is_absolute(startdir) ) {
		startdir = Util::path_to_abs_path(startdir);
	}

	return startdir;
}

}
