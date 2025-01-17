/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once


#include "Collection/Item.h"
#include "Control/UserInput.h"
#include "Database/DB.h"
#include "Database/MetadataDatabase.h"
#include "Execute/AttractMode.h"
#include "Graphics/FontCache.h"
#include "Video/IVideo.h"
#include "Video/VideoFactory.h"
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <list>
#include <stack>
#include <map>
#include <string>

/* State machine states */
#define RETROFE_STATES \
		X(RETROFE_IDLE, "RETROFE_IDLE") \
        X(RETROFE_LOAD_ART, "RETROFE_LOAD_ART") \
        X(RETROFE_ENTER, "RETROFE_ENTER") \
        X(RETROFE_SPLASH_EXIT, "RETROFE_SPLASH_EXIT") \
        X(RETROFE_PLAYLIST_REQUEST, "RETROFE_PLAYLIST_REQUEST") \
        X(RETROFE_PLAYLIST_EXIT, "RETROFE_PLAYLIST_EXIT") \
        X(RETROFE_PLAYLIST_LOAD_ART, "RETROFE_PLAYLIST_LOAD_ART") \
        X(RETROFE_PLAYLIST_ENTER, "RETROFE_PLAYLIST_ENTER") \
        X(RETROFE_MENUJUMP_REQUEST, "RETROFE_MENUJUMP_REQUEST") \
        X(RETROFE_MENUJUMP_EXIT, "RETROFE_MENUJUMP_EXIT") \
        X(RETROFE_MENUJUMP_LOAD_ART, "RETROFE_MENUJUMP_LOAD_ART") \
        X(RETROFE_MENUJUMP_ENTER, "RETROFE_MENUJUMP_ENTER") \
        X(RETROFE_HIGHLIGHT_REQUEST, "RETROFE_HIGHLIGHT_REQUEST") \
        X(RETROFE_HIGHLIGHT_EXIT, "RETROFE_HIGHLIGHT_EXIT") \
        X(RETROFE_HIGHLIGHT_LOAD_ART, "RETROFE_HIGHLIGHT_LOAD_ART") \
        X(RETROFE_HIGHLIGHT_ENTER, "RETROFE_HIGHLIGHT_ENTER") \
        X(RETROFE_NEXT_PAGE_REQUEST, "RETROFE_NEXT_PAGE_REQUEST") \
        X(RETROFE_NEXT_PAGE_MENU_EXIT, "RETROFE_NEXT_PAGE_MENU_EXIT") \
        X(RETROFE_NEXT_PAGE_MENU_LOAD_ART, "RETROFE_NEXT_PAGE_MENU_LOAD_ART") \
        X(RETROFE_NEXT_PAGE_MENU_ENTER, "RETROFE_NEXT_PAGE_MENU_ENTER") \
        X(RETROFE_HANDLE_MENUENTRY, "RETROFE_HANDLE_MENUENTRY") \
        X(RETROFE_LAUNCH_ENTER, "RETROFE_LAUNCH_ENTER") \
        X(RETROFE_LAUNCH_BIBI_ENTER, "RETROFE_LAUNCH_BIBI_ENTER") \
        X(RETROFE_LAUNCH_BIBI_REQUEST, "RETROFE_LAUNCH_BIBI_REQUEST") \
        X(RETROFE_LAUNCH_REQUEST, "RETROFE_LAUNCH_REQUEST") \
        X(RETROFE_LAUNCH_EXIT, "RETROFE_LAUNCH_EXIT") \
        X(RETROFE_BACK_REQUEST, "RETROFE_BACK_REQUEST") \
        X(RETROFE_BACK_MENU_EXIT, "RETROFE_BACK_MENU_EXIT") \
        X(RETROFE_BACK_MENU_LOAD_ART, "RETROFE_BACK_MENU_LOAD_ART") \
        X(RETROFE_BACK_MENU_ENTER, "RETROFE_BACK_MENU_ENTER") \
        X(RETROFE_MENUMODE_START_REQUEST, "RETROFE_MENUMODE_START_REQUEST") \
        X(RETROFE_MENUMODE_START_LOAD_ART, "RETROFE_MENUMODE_START_LOAD_ART") \
        X(RETROFE_MENUMODE_START_ENTER, "RETROFE_MENUMODE_START_ENTER") \
        X(RETROFE_NEW, "RETROFE_NEW") \
        X(RETROFE_QUIT_REQUEST, "RETROFE_QUIT_REQUEST") \
        X(RETROFE_QUIT, "RETROFE_QUIT") \
        X(NB_RETROFE_STATES, "")

#define BIBI_CMD    "cd /usr/games/bibi/; ./bibi; cd -"

class CollectionInfo;
class Configuration;
class Page;


class RetroFE
{

public:
    RetroFE( Configuration &c );
    virtual ~RetroFE( );
    bool     deInitialize( );
    void     run( );
    void     freeGraphicsMemory( );
    void     allocateGraphicsMemory( );
    void     launchEnter( );
    void     launchExit( );

private:
    volatile bool   initialized;
    volatile bool   initializeError;
    volatile bool   initMetaDb;
    SDL_Thread     *initializeThread;
    static int      initialize( void *context );
    static void     handle_sigusr1(int sig);
    static void     quick_poweroff( );

    #undef X
    #define X(a, b) a,
    enum RETROFE_STATE {RETROFE_STATES};

    void            render( );
    bool            back( bool &exit );
    void            forceRender( bool render );
    void            quit( );
    Page           *loadPage( );
    Page           *loadSplashPage( );
    RETROFE_STATE   processUserInput( Page *page );
    void            update( float dt, bool scrollActive );
    CollectionInfo *getCollection( std::string collectionName );
    CollectionInfo *getMenuCollection( std::string collectionName );
    void            printState(RETROFE_STATE state);

    Configuration     &config_;
    DB                *db_;
    MetadataDatabase  *metadb_;
    UserInput          input_;
    Page              *currentPage_;
    std::stack<Page *> pages_;
    float              keyInputDisable_;
    float              currentTime_;
    float              lastLaunchReturnTime_;
    float              keyLastTime_;
    float              keyDelayTime_;
    Item              *nextPageItem_;
    FontCache          fontcache_;
    AttractMode        attract_;
    bool               menuMode_;
    bool               mustRender_;

    std::map<std::string, unsigned int> lastMenuOffsets_;
    std::map<std::string, std::string>  lastMenuPlaylists_;
};
