// kde.cpp:  K Development Environment top level window, for Gnash.
// 
//   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
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
#include "gnashconfig.h"
#endif


#include <map>
#include <boost/assign/list_inserter.hpp>

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>
#include <QCursor>
#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QEvent>

#include "Range2d.h"

#include "gnash.h"
#include "movie_definition.h" 
#include "log.h"

#include "gui.h"
#include "Kde4Gui.h"
#include "Kde4.moc"
#include "render_handler.h"
#include "utility.h" // for PIXELS_TO_TWIPS 


namespace gnash 
{

Kde4Gui::Kde4Gui(unsigned long xid, float scale, bool loop, unsigned int depth)
 : Gui(xid, scale, loop, depth)
{
}


Kde4Gui::~Kde4Gui()
{
}


bool
Kde4Gui::init(int argc, char **argv[])
{

    char** r = NULL;
    int* i = new int(0);

    _application.reset(new QApplication(*i, r));
    _window.reset(new QMainWindow());
    _drawingWidget = new DrawingWidget(*this);

    _glue.init (argc, argv);

    setupActions();
    setupMenus();

    if (!_xid) {
        createMainMenu();
    }

    // Make sure key events are ready to be passed
    // before the widget can receive them.
    setupKeyMap();
    
    return true;
}


bool
Kde4Gui::run()
{
    return _application->exec();
}


bool
Kde4Gui::createWindow(const char* windowtitle, int width, int height)
{

    _width = width;
    _height = height;

    _drawingWidget->setMinimumSize(_width, _height);

    // Enable receiving of mouse events.
    _drawingWidget->setMouseTracking(true);
    _drawingWidget->setFocusPolicy(Qt::StrongFocus);
    _window->setWindowTitle(windowtitle);
    
    // The QMainWindow takes ownership of the DrawingWidget.
    _window->setCentralWidget(_drawingWidget);
    _window->show();

    _glue.prepDrawingArea(_drawingWidget);

    _renderer = _glue.createRenderHandler();

    if ( ! _renderer ) {
        return false;
    }

    _validbounds.setTo(0, 0, _width, _height);
    _glue.initBuffer(width, height);
    
    set_render_handler(_renderer);
   
    return true;
}


void
Kde4Gui::popupMenu(const QPoint& point)
{
    QMenu popupMenu(_drawingWidget);
    popupMenu.addMenu(fileMenu);
    popupMenu.addMenu(movieControlMenu);
    popupMenu.addMenu(viewMenu);
    popupMenu.exec(point);
}


void
Kde4Gui::renderBuffer()
{
    
    for (DrawBounds::const_iterator i = _drawbounds.begin(),
                        e = _drawbounds.end(); i != e; ++i) {
        
        // it may happen that a particular range is out of the screen, which 
        // will lead to bounds==null. 
        if (i->isNull()) continue;
        
        assert(i->isFinite()); 

        _drawingWidget->update(i->getMinX(), i->getMinY(),
                               i->width(), i->height());

    }
}


void
Kde4Gui::renderWidget(const QRect& updateRect)
{
    // This call renders onto the widget using a QPainter,
    // which *must only happen inside a paint event*.
    _glue.render(updateRect);
}


void
Kde4Gui::setInvalidatedRegions(const InvalidatedRanges& ranges)
{
    _renderer->set_invalidated_regions(ranges);

    _drawbounds.clear();

    for (size_t i = 0, e = ranges.size(); i != e; ++i) {

        geometry::Range2d<int> bounds = Intersection(
        _renderer->world_to_pixel(ranges.getRange(i)),
        _validbounds);

        // it may happen that a particular range is out of the screen, which 
        // will lead to bounds==null. 
        if (bounds.isNull()) continue;


        assert(bounds.isFinite());

        _drawbounds.push_back(bounds);

    }
}


void
Kde4Gui::setTimeout(unsigned int timeout)
{
    QTimer::singleShot(timeout, _application.get(), SLOT(quit()));
}


void
Kde4Gui::setInterval(unsigned int interval)
{
      _drawingWidget->startTimer(interval);
}


void
Kde4Gui::setCursor(gnash_cursor_type newcursor)
{
    switch(newcursor) {
        case CURSOR_HAND:
            _drawingWidget->setCursor(Qt::PointingHandCursor);
            break;
        case CURSOR_INPUT:
            _drawingWidget->setCursor(Qt::IBeamCursor); 
            break;
        default:
            _drawingWidget->unsetCursor(); 
    }
}


gnash::key::code
Kde4Gui::qtToGnashKey(QKeyEvent *event)
{

    // This should be initialized by now.
    assert (!_keyMap.empty());

    // Gnash uses its own keycodes to map key events
    // to the three sometimes weird and confusing values that flash movies
    // can refer to. See gnash.h for the keycodes and map.
    //
    // Gnash's keycodes are gnash::key::code. They are mainly in ascii order.
    // Standard ascii characters (32-127) have the same value. Extended ascii
    // characters (160-254) are in ascii order but correspond to gnash::key::code
    // 169-263. Non-character values must normally be mapped separately.

    const int key = event->key();

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
          return static_cast<gnash::key::code>(
                key - Qt::Key_0 + gnash::key::_0);
    }

    // All other characters between ascii 32 and 126 are simple.
    // From space (32) to slash (47):
    else if (key >= Qt::Key_Space && key <= Qt::Key_AsciiTilde) {
        return static_cast<gnash::key::code>(
                key - Qt::Key_Space + gnash::key::SPACE);
    }

    // Function keys:
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F15) {
        return static_cast<gnash::key::code>(
                key - Qt::Key_F1 + gnash::key::F1);
    }

    // Extended ascii from non-breaking (160) space to ÿ (264) is in the same
    // order.
    else if (key >= Qt::Key_nobreakspace && key <= Qt::Key_ydiaeresis) {
        return static_cast<gnash::key::code>(
                key - Qt::Key_nobreakspace + gnash::key::NOBREAKSPACE);
    }

    const KeyMap::const_iterator it = _keyMap.find(key);
    
    if (it == _keyMap.end()) return gnash::key::INVALID;
    
    return it->second;

}


void
Kde4Gui::handleKeyEvent(QKeyEvent *event, bool down)
{
    gnash::key::code c = qtToGnashKey(event);
//    int mod = qtToGnashModifier(event->state());
    notify_key_event(c, 0, down);
}


void
Kde4Gui::resize(int width, int height)
{
    _glue.resize(width, height);
    resize_view(width, height);
}


void
Kde4Gui::quit()
{
    _application->quit();
}


void
Kde4Gui::setupActions()
{

    // File Menu actions
    quitAction = new QAction(_("Quit Gnash"), _window.get());
    // This is connected directly to the QApplication's quit() slot
    _drawingWidget->connect(quitAction, SIGNAL(triggered()),
                     _application.get(), SLOT(quit()));

    // Movie Control Menu actions
    playAction = new QAction(_("Play"), _window.get());
    _drawingWidget->connect(playAction, SIGNAL(triggered()),
                     _drawingWidget, SLOT(play()));

    pauseAction = new QAction(_("Pause"), _window.get());
    _drawingWidget->connect(pauseAction, SIGNAL(triggered()),
                     _drawingWidget, SLOT(pause()));

    stopAction = new QAction(_("Stop"), _window.get());
    _drawingWidget->connect(stopAction, SIGNAL(triggered()),
                     _drawingWidget, SLOT(stop()));

    restartAction = new QAction(_("Restart"), _window.get());
    _drawingWidget->connect(restartAction, SIGNAL(triggered()),
                     _drawingWidget, SLOT(restart()));

    // View Menu actions
    refreshAction = new QAction(_("Refresh"), _window.get());
    _drawingWidget->connect(refreshAction, SIGNAL(triggered()),
                     _drawingWidget, SLOT(refresh()));

}


void
Kde4Gui::setupMenus()
{
    /// The menus are children of the QMainWindow so that
    /// they are destroyed on exit. The QMainWindow already has
    /// ownership of the main QMenuBar.

    // Set up the File menu.
    fileMenu = new QMenu(_("File"), _window.get());
    fileMenu->addAction(quitAction);

    // Set up the Movie Control menu
    movieControlMenu = new QMenu(_("Movie Control"), _window.get());
    movieControlMenu->addAction(playAction);
    movieControlMenu->addAction(pauseAction);
    movieControlMenu->addAction(stopAction);
    movieControlMenu->addAction(restartAction);

    // Set up the View menu
    viewMenu = new QMenu(_("View"), _window.get());
    viewMenu->addAction(refreshAction);
}


void
Kde4Gui::createMainMenu()
{
    std::auto_ptr<QMenuBar> mainMenu(new QMenuBar);

    // Set up the menu bar.
    mainMenu->addMenu(fileMenu);
    mainMenu->addMenu(movieControlMenu);
    mainMenu->addMenu(viewMenu);

    // The QMainWindow::setMenuBar transfers ownership
    // of the QMenuBar.
    _window->setMenuBar(mainMenu.release());

}

void
Kde4Gui::setupKeyMap()
{
    // We only want to do this once, although it would not
    // be harmful to do it more.
    assert (_keyMap.empty());
    
    boost::assign::insert(_keyMap)
    (Qt::Key_Backspace, gnash::key::BACKSPACE)
    (Qt::Key_Tab, gnash::key::TAB)
    (Qt::Key_Clear, gnash::key::CLEAR)
    (Qt::Key_Return, gnash::key::ENTER)
    (Qt::Key_Enter, gnash::key::ENTER)
    (Qt::Key_Shift, gnash::key::SHIFT)
    (Qt::Key_Control, gnash::key::CONTROL)
    (Qt::Key_Alt, gnash::key::ALT)
    (Qt::Key_CapsLock, gnash::key::CAPSLOCK)
    (Qt::Key_Escape, gnash::key::ESCAPE)
    (Qt::Key_Space, gnash::key::SPACE)
    (Qt::Key_PageDown, gnash::key::PGDN)
    (Qt::Key_PageUp, gnash::key::PGUP)
    (Qt::Key_Home, gnash::key::HOME)
    (Qt::Key_End, gnash::key::END)
    (Qt::Key_Left, gnash::key::LEFT)
    (Qt::Key_Up, gnash::key::UP)
    (Qt::Key_Right, gnash::key::RIGHT)
    (Qt::Key_Down, gnash::key::DOWN)
    (Qt::Key_Insert, gnash::key::INSERT)
    (Qt::Key_Delete, gnash::key::DELETEKEY)
    (Qt::Key_Help, gnash::key::HELP)
    (Qt::Key_NumLock, gnash::key::NUM_LOCK)
    (Qt::Key_Semicolon, gnash::key::SEMICOLON)
    (Qt::Key_Equal, gnash::key::EQUALS)
    (Qt::Key_Minus, gnash::key::MINUS)
    (Qt::Key_Slash, gnash::key::SLASH)
    (Qt::Key_BracketLeft, gnash::key::LEFT_BRACKET)
    (Qt::Key_Backslash, gnash::key::BACKSLASH)
    (Qt::Key_BracketRight, gnash::key::RIGHT_BRACKET)
    (Qt::Key_QuoteDbl, gnash::key::DOUBLE_QUOTE);
}


/// DrawingWidget implementation

void 
DrawingWidget::paintEvent(QPaintEvent *event)
{
    _gui.renderWidget(event->rect());
}


void
DrawingWidget::timerEvent(QTimerEvent*)
{
    Gui::advance_movie(&_gui);
}


void
DrawingWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint position = event->pos();
    _gui.notify_mouse_moved(position.x(), position.y());
}


void
DrawingWidget::contextMenuEvent(QContextMenuEvent* event)
{
    _gui.popupMenu(event->globalPos());
}


void
DrawingWidget::mousePressEvent(QMouseEvent* /* event */)
{
    _gui.notify_mouse_clicked(true, 1);
}


void
DrawingWidget::mouseReleaseEvent(QMouseEvent* /* event */)
{
    _gui.notify_mouse_clicked(false, 1);
}


void
DrawingWidget::keyPressEvent(QKeyEvent *event)
{
    _gui.handleKeyEvent(event, true);
}


void
DrawingWidget::keyReleaseEvent(QKeyEvent *event)
{
    _gui.handleKeyEvent(event, false);
}


void
DrawingWidget::resizeEvent(QResizeEvent *event)
{
    _gui.resize(event->size().width(), event->size().height());
    update();
}


void
DrawingWidget::play()
{
    _gui.menu_play();
}


void
DrawingWidget::pause()
{
    _gui.menu_pause();
}


void
DrawingWidget::restart()
{
    _gui.menu_restart();
}


void
DrawingWidget::stop()
{
    _gui.menu_stop();
}


void
DrawingWidget::refresh()
{
    _gui.refreshView();
}



}

