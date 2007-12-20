// MovieClipLoader.cpp:  Implementation of ActionScript MovieClipLoader class.
// 
//   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tu_config.h"

#include "action.h" // for call_method
#include "as_value.h"
#include "as_object.h" // for inheritance
#include "fn_call.h"
#include "as_function.h"
#include "movie_definition.h"
#include "sprite_instance.h"
#include "character.h" // for loadClip (get_parent)
#include "log.h"
#include "URL.h" // for url parsing
#include "VM.h" // for the string table.
#include "builtin_function.h"
#include "Object.h" // for getObjectInterface
#include "AsBroadcaster.h" // for initializing self as a broadcaster
#include "namedStrings.h"
#include "array.h" // for _listeners construction

#include <typeinfo> 
#include <string>
#include <set>

namespace gnash {

// Forward declarations
static as_value moviecliploader_loadclip(const fn_call& fn);
static as_value moviecliploader_unloadclip(const fn_call& fn);
static as_value moviecliploader_getprogress(const fn_call& fn);
static as_value moviecliploader_new(const fn_call& fn);

static void
attachMovieClipLoaderInterface(as_object& o)
{
  	o.init_member("loadClip", new builtin_function(moviecliploader_loadclip));
	o.init_member("unloadClip", new builtin_function(moviecliploader_unloadclip));
	o.init_member("getProgress", new builtin_function(moviecliploader_getprogress));

	// NOTE: we want addListener/removeListener/broadcastMessage
	//       but don't what the _listeners property here...
	// TODO: add an argument to AsBroadcaster::initialize skip listeners ?
	AsBroadcaster::initialize(o);
	o.delProperty(NSV::PROP_uLISTENERS);

#if 0
	// Load the default event handlers. These should really never
	// be called directly, as to be useful they are redefined
	// within the SWF script. These get called if there is a problem
	// Setup the event handlers
	o.set_event_handler(event_id::LOAD_INIT, new builtin_function(event_test));
	o.set_event_handler(event_id::LOAD_START, new builtin_function(event_test));
	o.set_event_handler(event_id::LOAD_PROGRESS, new builtin_function(event_test));
	o.set_event_handler(event_id::LOAD_ERROR, new builtin_function(event_test));
#endif
  
}

static as_object*
getMovieClipLoaderInterface()
{
	static boost::intrusive_ptr<as_object> o;
	if ( o == NULL )
	{
		o = new as_object(getObjectInterface());
		//log_msg(_("MovieClipLoader interface @ %p"), o.get());
		attachMovieClipLoaderInterface(*o);
	}
	return o.get();
}


// progress info
struct mcl {
	int bytes_loaded;
	int bytes_total;
};


/// Progress object to use as return of MovieClipLoader.getProgress()
struct mcl_as_object : public as_object
{
	struct mcl data;
};

class MovieClipLoader: public as_object
{
public:

	MovieClipLoader();

	~MovieClipLoader();

	struct mcl *getProgress(as_object *ao);

	/// MovieClip
	bool loadClip(const std::string& url, sprite_instance& target);

	void unloadClip(void *);

	/// @todo make an EventDispatcher class for this
	/// @ {
	///

	/// Invoke any listener for the specified event
	void dispatchEvent(const std::string& eventName, const as_value& arg);

	/// @ }

protected:

#if 0
#ifdef GNASH_USE_GC
	/// Mark MovieClipLoader-specific reachable resources and invoke
	/// the parent's class version (markAsObjectReachable)
	//
	/// MovieClipLoader-specific reachable resources are:
	/// 	- The listeners (_listeners)
	///
	virtual void markReachableResources() const;
#endif // GNASH_USE_GC
#endif

private:

	//typedef std::set< boost::intrusive_ptr<as_object> > Listeners;
	//Listeners _listeners;

	bool          _started;
	bool          _completed;
	std::string     _filespec;
	int           _progress;
	bool          _error;
	struct mcl    _mcl;
};

MovieClipLoader::MovieClipLoader()
	:
	as_object(getMovieClipLoaderInterface())
{
	_mcl.bytes_loaded = 0;
	_mcl.bytes_total = 0;  

	set_member(NSV::PROP_uLISTENERS, new as_array_object());
}

MovieClipLoader::~MovieClipLoader()
{
	GNASH_REPORT_FUNCTION;
}

// progress of the downloaded file(s).
struct mcl *
MovieClipLoader::getProgress(as_object* /*ao*/)
{
  GNASH_REPORT_FUNCTION;

  return &_mcl;
}


bool
MovieClipLoader::loadClip(const std::string& url_str, sprite_instance& target)
{

	URL url(url_str.c_str(), get_base_url());
	
#if GNASH_DEBUG
	log_msg(_(" resolved url: %s"), url.str().c_str());
#endif
			 
	string_table& st = _vm.getStringTable();

	bool ret = target.loadMovie(url);
	if ( ! ret ) 
	{
		// TODO: find semantic of last argument
		callMethod(st.find("onLoadError"), as_value(&target), as_value("Failed to load movie or jpeg"), as_value(0));

		return false;
	}

	// Dispatch onLoadStart
	callMethod(st.find("onLoadStart"), as_value(&target));

	// Dispatch onLoadProgress
	struct mcl *mcl_data = getProgress(&target);
	// the callback since we're done loading the file
	mcl_data->bytes_loaded = target.get_bytes_loaded();
	mcl_data->bytes_total = target.get_bytes_total();
	callMethod(st.find("onLoadProgress"), as_value(&target), mcl_data->bytes_loaded, mcl_data->bytes_total);

	// Dispatch onLoadComplete
	callMethod(st.find("onLoadComplete"), as_value(&target), as_value(0)); // TODO: find semantic of last arg

	/// This event must be dispatched when actions
	/// in first frame of loaded clip have been executed.
	///
	/// Since movie_def_impl::create_instance takes
	/// care of this, this should be the correct place
	/// to invoke such an event.
	///
	/// TODO: check if we need to place it before calling
	///       this function though...
	///
	callMethod(st.find("onLoadInit"), as_value(&target));

	return true;
}

void
MovieClipLoader::unloadClip(void *)
{
  GNASH_REPORT_FUNCTION;
}

// Callbacks
void
MovieClipLoader::dispatchEvent(const std::string& event, const as_value& arg)
{
	as_value ev(event);

	log_debug("dispatchEvent calling broadcastMessage with args %s and %s", ev.to_debug_string().c_str(), arg.to_debug_string().c_str());
	callMethod(NSV::PROP_BROADCAST_MESSAGE, ev, arg);
}

static as_value
moviecliploader_loadclip(const fn_call& fn)
{
	as_value	val, method;

	//log_msg(_("%s: nargs = %d"), __FUNCTION__, fn.nargs);

	boost::intrusive_ptr<MovieClipLoader> ptr = ensureType<MovieClipLoader>(fn.this_ptr);
  
	if ( fn.nargs < 2 )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		std::stringstream ss; fn.dump_args(ss);
		log_aserror(_("MovieClipLoader.loadClip(%s): missing arguments"), ss.str().c_str());
		);
		return as_value(false);
	}

	as_value url_arg = fn.arg(0);
	std::string str_url = url_arg.to_string(); 

	as_value tgt_arg = fn.arg(1);
	std::string tgt_str = tgt_arg.to_string();
	character* target = fn.env().find_target(tgt_str);
	if ( ! target )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror(_("Could not find target %s (evaluated from %s)"),
			tgt_str.c_str(), tgt_arg.to_debug_string().c_str());
		);
		return as_value(false);
	}

	sprite_instance* sprite = target->to_movie();
	if ( ! sprite )
	{
		IF_VERBOSE_ASCODING_ERRORS(
		log_aserror(_("Target %s is not a sprite instance (%s)"),
			target->getTarget().c_str(), typeName(*target).c_str());
		);
		return as_value(false);
	}

#if GNASH_DEBUG
	log_msg(_("load clip: %s, target is: %p\n"),
		str_url.c_str(), (void*)sprite);
#endif

	ptr->loadClip(str_url, *sprite);

	// We always want to return true unless something went wrong
	return as_value(true);

}

static as_value
moviecliploader_unloadclip(const fn_call& fn)
{
  const std::string filespec = fn.arg(0).to_string();
  log_unimpl (_("%s: %s"), __PRETTY_FUNCTION__, filespec.c_str());
  return as_value();
}

static as_value
moviecliploader_new(const fn_call& /* fn */)
{

  as_object*	mov_obj = new MovieClipLoader;
  //log_msg(_("MovieClipLoader instance @ %p"), mov_obj);

  return as_value(mov_obj); // will store in a boost::intrusive_ptr
}

// Invoked every time the loading content is written to disk during
// the loading process.
static as_value
moviecliploader_getprogress(const fn_call& fn)
{
  //log_msg(_("%s: nargs = %d"), __FUNCTION__, nargs);
  
  boost::intrusive_ptr<MovieClipLoader> ptr = ensureType<MovieClipLoader>(fn.this_ptr);
  
  boost::intrusive_ptr<as_object> target = fn.arg(0).to_object();
  
  struct mcl *mcl_data = ptr->getProgress(target.get());

  boost::intrusive_ptr<mcl_as_object> mcl_obj ( new mcl_as_object );

  mcl_obj->init_member("bytesLoaded", mcl_data->bytes_loaded);
  mcl_obj->init_member("bytesTotal",  mcl_data->bytes_total);
  
  return as_value(mcl_obj.get()); // will store in a boost::intrusive_ptr
}

void
moviecliploader_class_init(as_object& global)
{
	// This is going to be the global Number "class"/"function"
	static boost::intrusive_ptr<builtin_function> cl=NULL;

	if ( cl == NULL )
	{
		cl=new builtin_function(&moviecliploader_new, getMovieClipLoaderInterface());
	}
	global.init_member("MovieClipLoader", cl.get()); //as_value(moviecliploader_new));
	//log_msg(_("MovieClipLoader class @ %p"), cl.get());
}

} // end of gnash namespace
