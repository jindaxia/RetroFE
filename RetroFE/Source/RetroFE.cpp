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


#include "RetroFE.h"
#include "Collection/CollectionInfoBuilder.h"
#include "Collection/CollectionInfo.h"
#include "Database/Configuration.h"
#include "Collection/Item.h"
#include "Execute/Launcher.h"
#include "Menu/Menu.h"
#include "Menu/MenuMode.h"
#include "Utility/Log.h"
#include "Utility/Utils.h"
#include "Collection/MenuParser.h"
#include "SDL.h"
#include <SDL/SDL_ttf.h>
#include "Control/UserInput.h"
#include "Graphics/PageBuilder.h"
#include "Graphics/Page.h"
#include "Graphics/Component/ScrollingList.h"
#include "Graphics/Component/Video.h"
#include "Video/VideoFactory.h"
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <time.h>
#include <signal.h> 

#if defined(__linux) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#endif

#ifdef WIN32
#include <Windows.h>
#include <SDL/SDL_syswm.h>
#include <SDL/SDL_thread.h>
#endif

//#define GET_RUN_TIME_MS    (clock() / (CLOCKS_PER_SEC/1000))
#define GET_RUN_TIME_MS    (SDL_GetTicks())

//#define PERIOD_FORCE_REFRESH    1000 //ms
#define FPS 60 // TODO: set in conf file

//#define DEBUG_FPS
#ifdef DEBUG_FPS
#define DEBUG_FPS_PRINTF(...)   printf(__VA_ARGS__);
#else
#define DEBUG_FPS_PRINTF(...)
#endif //MENU_DEBUG


RetroFE::RetroFE( Configuration &c )
    : initialized(false)
    , initializeError(false)
    , initMetaDb(true)
    , initializeThread(NULL)
    , config_(c)
    , db_(NULL)
    , metadb_(NULL)
    , input_(config_)
    , currentPage_(NULL)
    , keyInputDisable_(0)
    , currentTime_(0)
    , lastLaunchReturnTime_(0)
    , keyLastTime_(0)
    , keyDelayTime_(.3f)
{
    menuMode_ = false;
    mustRender_ = true;
}


RetroFE::~RetroFE( )
{
    deInitialize( );
}


// Render the current page to the screen
void RetroFE::render( )
{

    SDL_LockMutex( SDL::getMutex( ) );
    //SDL_SetRenderDrawColor( SDL::getRenderer( ), 0x0, 0x0, 0x00, 0xFF );
    //SDL_RenderClear( SDL::getRenderer( ) );
    SDL_FillRect(SDL::getWindow( ), NULL, SDL_MapRGB(SDL::getWindow( )->format, 0, 0, 0));

#ifdef DEBUG_FPS
    uint32_t draw_ticks = static_cast<unsigned int>GET_RUN_TIME_MS;
#endif //DEBUG_FPS
    if ( currentPage_ )
    {
        currentPage_->draw( );
    }
#ifdef DEBUG_FPS
    int draw_time = static_cast<int>(GET_RUN_TIME_MS)-draw_ticks;
    //printf("draw time: %dms\n", draw_time);
#endif //DEBUG_FPS


#ifdef DEBUG_FPS
    // DEBUG: Average draw time over FPS*5 frames
    static int avg_draw_time = 0;
    static int avg_draw_time_nb_vals = 0;
    avg_draw_time += draw_time;
    avg_draw_time_nb_vals++;
    if(avg_draw_time_nb_vals >= FPS*5){
        printf("Average draw time: %dms\n", avg_draw_time/avg_draw_time_nb_vals);
        avg_draw_time=0;
        avg_draw_time_nb_vals=0;
    }
#endif //DEBUG_FPS

    //SDL_Flip(SDL::getWindow( ));
    SDL::renderAndFlipWindow();

    SDL_UnlockMutex( SDL::getMutex( ) );

}


// Initialize the configuration and database
int RetroFE::initialize( void *context )
{

    RetroFE *instance = static_cast<RetroFE *>(context);

    Logger::write( Logger::ZONE_INFO, "RetroFE", "Initializing" );

    if ( !instance->input_.initialize( ) )
    {
        Logger::write( Logger::ZONE_ERROR, "RetroFE", "Could not initialize user controls" );
        instance->initializeError = true;
        return -1;
    }
    Logger::write( Logger::ZONE_INFO, "RetroFE", "Initialized user controls" );

    instance->db_ = new DB( Utils::combinePath( Configuration::absolutePath, "meta.db" ) );

    if ( !instance->db_->initialize( ) )
    {
        Logger::write( Logger::ZONE_ERROR, "RetroFE", "Could not initialize database" );
        instance->initializeError = true;
        return -1;
    }
    Logger::write( Logger::ZONE_INFO, "RetroFE", "Initialized database" );

    instance->metadb_ = new MetadataDatabase( *(instance->db_), instance->config_ );


    if(instance->initMetaDb){
        Logger::write( Logger::ZONE_INFO, "RetroFE", "Initializing meta database..." );
        if ( !instance->metadb_->initialize( ) )
        {
            Logger::write( Logger::ZONE_ERROR, "RetroFE", "Could not initialize meta database" );
            instance->initializeError = true;
            return -1;
        }
        Logger::write( Logger::ZONE_INFO, "RetroFE", "Initialized meta database" );
    }

    /* Init Signals */
    Logger::write( Logger::ZONE_INFO, "RetroFE", "Initializing signal USR1..." );
    signal(SIGUSR1, instance->handle_sigusr1); 

    instance->initialized = true;
    return 0;

}


// Launch a game/program
void RetroFE::launchEnter( )
{

    // Disable window focus
    //SDL_SetWindowGrab(SDL::getWindow( ), SDL_FALSE);

    // Free the textures, and optionally take down SDL
    freeGraphicsMemory( );

}


// Return from the launch of a game/program
void RetroFE::launchExit( )
{

    // Optionally set up SDL, and load the textures
    allocateGraphicsMemory( );

    // Restore the SDL settings
    //SDL_RestoreWindow( SDL::getWindow( ) );
    //SDL_RaiseWindow( SDL::getWindow( ) );
    //SDL_SetWindowGrab( SDL::getWindow( ), SDL_TRUE );

    // Empty event queue, but handle joystick add/remove events
    SDL_Event e;
    while ( SDL_PollEvent( &e ) )
    {
        /*if ( e.type == SDL_JOYDEVICEADDED || e.type == SDL_JOYDEVICEREMOVED )
        {
            input_.update( e );
        }*/
    }
    input_.resetStates( );
    attract_.reset( );

    // Restore time settings
    currentTime_ = static_cast<float>( GET_RUN_TIME_MS ) / 1000;
    lastLaunchReturnTime_ = currentTime_;

}


// Free the textures, and optionally take down SDL
void RetroFE::freeGraphicsMemory( )
{

    // Free textures
    if ( currentPage_ )
    {
        currentPage_->freeGraphicsMemory( );
    }

    // Close down SDL
    bool unloadSDL = false;
    config_.getProperty( "unloadSDL", unloadSDL );
    if ( unloadSDL )
    {
        currentPage_->deInitializeFonts( );
        // Deinit menuMode
        MenuMode::end( );
        SDL::deInitialize( );
        //input_.clearJoysticks( );
    }

}


// Optionally set up SDL, and load the textures
void RetroFE::allocateGraphicsMemory( )
{

    // Reopen SDL
    bool unloadSDL = false;
    config_.getProperty( "unloadSDL", unloadSDL );
    if ( unloadSDL )
    {
        SDL::initialize( config_ );
        currentPage_->initializeFonts( );

        // Init MenuMode
        MenuMode::init( config_ );
    }

    // Allocate textures
    if ( currentPage_ )
    {
        currentPage_->allocateGraphicsMemory( );
    }

}


// Deinitialize RetroFE
bool RetroFE::deInitialize( )
{

    bool retVal = true;

    // Free textures
    freeGraphicsMemory( );

    // Delete page
    if ( currentPage_ )
    {
        currentPage_->deInitialize( );
        delete currentPage_;
        currentPage_ = NULL;
    }

    // Delete databases
    if ( metadb_ )
    {
        delete metadb_;
        metadb_ = NULL;
    }

    if ( db_ )
    {
        delete db_;
        db_ = NULL;
    }

    initialized = false;

    Logger::write( Logger::ZONE_INFO, "RetroFE", "Exiting" );

    return retVal;
}


// Run RetroFE
void RetroFE::run( )
{

    // Initialize SDL
    if(! SDL::initialize( config_ ) ) return;
    fontcache_.initialize( );

    // Initialize MenuMode
    MenuMode::init( config_ );

    // Define control configuration
    std::string controlsConfPath = Utils::combinePath( Configuration::absolutePath, "controls.conf" );
    if ( !config_.import( "controls", controlsConfPath ) )
    {
        Logger::write( Logger::ZONE_ERROR, "RetroFE", "Could not import \"" + controlsConfPath + "\"" );
        return;
    }

    float preloadTime = 0;

    // Force refresh variables
#ifdef PERIOD_FORCE_REFRESH
    int ticks_last_refresh = 0;
#endif  //PERIOD_FORCE_REFRESH

    // Initialize video
    bool videoEnable = true;
    int  videoLoop   = 0;
    config_.getProperty( "videoEnable", videoEnable );
    config_.getProperty( "videoLoop", videoLoop );
    VideoFactory::setEnabled( videoEnable );
    VideoFactory::setNumLoops( videoLoop );
    VideoFactory::createVideo( ); // pre-initialize the gstreamer engine
    Video::setEnabled( videoEnable );

    // Init thread
    bool initMetaDbtmp;
    config_.getProperty( "initMetaDb", initMetaDbtmp );
    initMetaDb = initMetaDbtmp;
    initializeThread = SDL_CreateThread( initialize, (void *)this );

    if ( !initializeThread )
    {
        Logger::write( Logger::ZONE_INFO, "RetroFE", "Could not initialize RetroFE" );
        return;
    }

    int attractModeTime = 0;
    std::string firstCollection = "Main";
    bool running = true;
    bool initInBackground = false;
    int res;
    config_.getProperty( "initInBackground", initInBackground );

    RETROFE_STATE state = RETROFE_NEW;

    config_.getProperty( "attractModeTime", attractModeTime );
    config_.getProperty( "firstCollection", firstCollection );

    attract_.idleTime = static_cast<float>(attractModeTime);

    int initializeStatus = 0;

    // load the initial splash screen, unload it once it is complete
    currentPage_        = loadSplashPage( );
    state               = RETROFE_ENTER;
    bool splashMode     = true;
    bool exitSplashMode = false;

    Launcher l( config_ );
    Menu     m( config_ );
    preloadTime = static_cast<float>( GET_RUN_TIME_MS ) / 1000;

    while ( running )
    {

        float lastTime = 0;
        float deltaTime = 0;

        // Exit splash mode when an active key is pressed
        SDL_Event e;
        if ( splashMode )
        {
	    while ( SDL_PollEvent( &e ) );

            if ( input_.update( e ) && input_.keystate(UserInput::KeyCodeSelect) )
            {
                exitSplashMode = true;
                while ( SDL_PollEvent( &e ) )
                input_.resetStates( );
                attract_.reset( );
            }
        }

        if ( !currentPage_ )
        {
            Logger::write(  Logger::ZONE_WARNING, "RetroFE", "Could not load page"  );
            running = false;
            break;
        }

        // Print state for debug purposes
#if 0
        static RETROFE_STATE prev_state = state;
        #undef X
        #define X(a, b) b,
        static const char *retrofe_states_str[] = {RETROFE_STATES};
        if(prev_state != state){
            printf("RetroFE new state: %s\n", retrofe_states_str[state]);

            std::stringstream ss;
            ss << "New state: " << retrofe_states_str[state];
            Logger::write( Logger::ZONE_INFO, "RetroFE", ss.str() );

            prev_state = state;
        }
#endif

        // Main state machine
        switch(state)
        {

        // Idle state; waiting for input
        case RETROFE_IDLE:

            // Not in splash mode
            if ( currentPage_ && !splashMode )
            {
                // account for when returning from a menu and the previous key was still "stuck"
                if ( lastLaunchReturnTime_ == 0 || (currentTime_ - lastLaunchReturnTime_ > .3) )
                {
                    if ( currentPage_->isIdle( ) )
                    {
                        state = processUserInput( currentPage_ );
                    }
                    lastLaunchReturnTime_ = 0;
                }
            }

            // Handle end of splash mode
            if ( ( (initialized || initializeError) && splashMode && (exitSplashMode || (currentPage_->getMinShowTime( ) <= (currentTime_ - preloadTime) && !(currentPage_->isPlaying( )))) )
                || (initInBackground && splashMode) )
            {
                if(!initInBackground){
                    SDL_WaitThread( initializeThread, &initializeStatus );
                    printf("SDL_WaitThread finisheds\n");
                }

                if ( initializeError )
                {
                    state = RETROFE_QUIT_REQUEST;
                    break;
                }

                currentPage_->stop( );
                state = RETROFE_SPLASH_EXIT;

            }
            break;

        // Load art on entering RetroFE
        case RETROFE_LOAD_ART:
            currentPage_->start( );
            state = RETROFE_ENTER;
            break;

        // Wait for onEnter animation to finish
        case RETROFE_ENTER:
            if ( currentPage_->isIdle( ) )
            {
                state = RETROFE_IDLE;
            }
            break;

        // Handle end of splash mode
        case RETROFE_SPLASH_EXIT:
            if ( currentPage_->isIdle( ) )
            {
                // delete the splash screen and use the standard menu
                currentPage_->deInitialize( );
                delete currentPage_;

                currentPage_ = loadPage( );
                splashMode = false;
                if ( currentPage_ )
                {
                    std::string firstCollection = "Main";

                    config_.getProperty( "firstCollection", firstCollection );
                    config_.setProperty( "currentCollection", firstCollection );
                    CollectionInfo *info = getCollection(firstCollection);

                    currentPage_->pushCollection(info);

                    bool autoFavorites = true;
                    config_.getProperty( "autoFavorites", autoFavorites );

                    if (autoFavorites)
                    {
                        currentPage_->selectPlaylist("favorites"); // Switch to favorites playlist
                    }
                    else
                    {
                        currentPage_->selectPlaylist("all"); // Switch to all games playlist
                    }

                    currentPage_->onNewItemSelected( );
                    currentPage_->reallocateMenuSpritePoints( );

                    state = RETROFE_LOAD_ART;
                }
                else
                {
                    state = RETROFE_QUIT_REQUEST;
                }
            }
            break;

        // Switch playlist; start onHighlightExit animation
        case RETROFE_PLAYLIST_REQUEST:
            currentPage_->playlistExit( );
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_PLAYLIST_EXIT;
            break;

        // Switch playlist; wait for onHighlightExit animation to finish; load art
        case RETROFE_PLAYLIST_EXIT:
            if (currentPage_->isIdle( ))
            {
                currentPage_->onNewItemSelected( );
                state = RETROFE_PLAYLIST_LOAD_ART;
            }
            break;

        // Switch playlist; start onHighlightEnter animation
        case RETROFE_PLAYLIST_LOAD_ART:
            if (currentPage_->isIdle( ))
            {
                currentPage_->reallocateMenuSpritePoints( );
                currentPage_->playlistEnter( );
                state = RETROFE_PLAYLIST_ENTER;
            }
            break;

        // Switch playlist; wait for onHighlightEnter animation to finish
        case RETROFE_PLAYLIST_ENTER:
            if (currentPage_->isIdle( ))
            {
                bool collectionInputClear = false;
                config_.getProperty( "collectionInputClear", collectionInputClear );
                if (  collectionInputClear  )
                {
                    // Empty event queue

                    SDL_Event e;
                    while ( SDL_PollEvent( &e ) );
                    input_.resetStates( );
                }
                state = RETROFE_IDLE;
            }
            break;

        // Jump in menu; start onMenuJumpExit animation
        case RETROFE_MENUJUMP_REQUEST:
            currentPage_->menuJumpExit( );
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            state = RETROFE_MENUJUMP_EXIT;
            break;

        // Jump in menu; wait for onMenuJumpExit animation to finish; load art
        case RETROFE_MENUJUMP_EXIT:
            if (currentPage_->isIdle( ))
            {
                currentPage_->onNewItemSelected( );
                state = RETROFE_MENUJUMP_LOAD_ART;
            }
            break;

        // Jump in menu; start onMenuJumpEnter animation
        case RETROFE_MENUJUMP_LOAD_ART:
            if (currentPage_->isIdle( ))
            {
                currentPage_->reallocateMenuSpritePoints( );
                currentPage_->menuJumpEnter( );
                forceRender(true);
                state = RETROFE_MENUJUMP_ENTER;
            }
            break;

        // Jump in menu; wait for onMenuJump animation to finish
        case RETROFE_MENUJUMP_ENTER:
            if (currentPage_->isIdle( ))
            {
                state = RETROFE_IDLE;
            }
            break;

        // Start onHighlightExit animation
        case RETROFE_HIGHLIGHT_REQUEST:
            currentPage_->setScrolling(Page::ScrollDirectionIdle);
            currentPage_->onNewItemSelected( );
            currentPage_->highlightExit( );
            state = RETROFE_HIGHLIGHT_EXIT;
            break;

        // Wait for onHighlightExit animation to finish; load art
        case RETROFE_HIGHLIGHT_EXIT:
            if (currentPage_->isIdle( ))
            {
                currentPage_->highlightLoadArt( );
                state = RETROFE_HIGHLIGHT_LOAD_ART;
            }
            break;

        // Start onHighlightEnter animation
        case RETROFE_HIGHLIGHT_LOAD_ART:
            currentPage_->highlightEnter( );
            state = RETROFE_HIGHLIGHT_ENTER;
            break;

        // Wait for onHighlightEnter animation to finish
        case RETROFE_HIGHLIGHT_ENTER:
            RETROFE_STATE state_tmp;
            if (currentPage_->isMenuIdle( ) &&
                ((state_tmp = processUserInput( currentPage_ )) == RETROFE_HIGHLIGHT_REQUEST ||
                  state_tmp                                     == RETROFE_MENUJUMP_REQUEST) )
            {
                state = state_tmp;
            }
            else if (currentPage_->isIdle( ))
            {
                state = RETROFE_IDLE;
            }
            break;

        // Next page; start onMenuExit animation
        case RETROFE_NEXT_PAGE_REQUEST:
            currentPage_->exitMenu( );
            state = RETROFE_NEXT_PAGE_MENU_EXIT;
            break;

        // Wait for onMenuExit animation to finish; load new page if applicable; load art
        case RETROFE_NEXT_PAGE_MENU_EXIT:
            if ( currentPage_->isIdle( ) )
            {
                lastMenuOffsets_[currentPage_->getCollectionName( )]   = currentPage_->getScrollOffsetIndex( );
                lastMenuPlaylists_[currentPage_->getCollectionName( )] = currentPage_->getPlaylistName( );
                std::string nextPageName = nextPageItem_->name;
                if ( !menuMode_ )
                {
                    // Load new layout if available
                    std::string layoutName;
                    bool userLayout;
                    config_.getProperty( "layout", layoutName );
                    config_.getProperty( "userTheme", userLayout );
                    PageBuilder pb( layoutName, "layout", config_, &fontcache_, false, userLayout);
                    Page *page = pb.buildPage( nextPageItem_->name );
                    if ( page )
                    {
                        currentPage_->freeGraphicsMemory( );
                        pages_.push( currentPage_ );
                        currentPage_ = page;
                    }
                }

                config_.setProperty( "currentCollection", nextPageName );

                CollectionInfo *info;
                if ( menuMode_ )
                    info = getMenuCollection( nextPageName );
                else
                    info = getCollection( nextPageName );

                currentPage_->pushCollection(info);

                bool rememberMenu = false;
                config_.getProperty( "rememberMenu", rememberMenu );
                bool autoFavorites = true;
                config_.getProperty( "autoFavorites", autoFavorites );

                if (rememberMenu && lastMenuPlaylists_.find( nextPageName ) != lastMenuPlaylists_.end( ))
                {
                  currentPage_->selectPlaylist( lastMenuPlaylists_[nextPageName] ); // Switch to last playlist
                }
                else if (autoFavorites)
                {
                  currentPage_->selectPlaylist( "favorites" ); // Switch to favorites playlist
                }
                else
                {
                  currentPage_->selectPlaylist( "all" ); // Switch to all games playlist
                }

                if ( rememberMenu && lastMenuOffsets_.find( nextPageName ) != lastMenuOffsets_.end( ) )
                {
                    currentPage_->setScrollOffsetIndex( lastMenuOffsets_[nextPageName] );
                }

                currentPage_->onNewItemSelected( );
                currentPage_->reallocateMenuSpritePoints( );

                state = RETROFE_NEXT_PAGE_MENU_LOAD_ART;

             }
             break;

        // Start onMenuEnter animation
        case RETROFE_NEXT_PAGE_MENU_LOAD_ART:
            if (currentPage_->getMenuDepth( ) != 1 )
            {
                currentPage_->enterMenu( );
            }
            else
            {
                currentPage_->start( );
            }
            state = RETROFE_NEXT_PAGE_MENU_ENTER;
            break;

        // Wait for onMenuEnter animation to finish
        case RETROFE_NEXT_PAGE_MENU_ENTER:
            if ( currentPage_->isIdle( ) )
            {
                bool collectionInputClear = false;
                config_.getProperty( "collectionInputClear", collectionInputClear );
                if (  collectionInputClear  )
                {
                    // Empty event queue
                    SDL_Event e;
                    while ( SDL_PollEvent( &e ) );
                    input_.resetStates( );
                }
                state = RETROFE_IDLE;
            }
            break;

        // Launching a menu entry
        case RETROFE_HANDLE_MENUENTRY:

            // Empty event queue
            SDL_Event e;
            while ( SDL_PollEvent( &e ) );
            input_.resetStates( );

            // Handle menu entry
            m.handleEntry( currentPage_->getSelectedItem( ) );

            // Empty event queue
            while ( SDL_PollEvent( &e ) );
            input_.resetStates( );

            state = RETROFE_IDLE;
            break;

        // Launching game; start onGameEnter animation
        case RETROFE_LAUNCH_ENTER:
            currentPage_->enterGame( );  // Start onGameEnter animation
            currentPage_->playSelect( ); // Play launch sound
            state = RETROFE_LAUNCH_REQUEST;
            break;

        // Wait for onGameEnter animation to finish; launch game; start onGameExit animation
        case RETROFE_LAUNCH_REQUEST:
            if ( currentPage_->isIdle( ) && !currentPage_->isSelectPlaying( ) )
            {
                nextPageItem_ = currentPage_->getSelectedItem( );
                launchEnter( );
                l.run(nextPageItem_->collectionInfo->name, nextPageItem_);

                launchExit( );
                currentPage_->exitGame( );

                // Warning test this
                /*currentPage_->onNewItemSelected( );
                currentPage_->reallocateMenuSpritePoints( );*/

                state = RETROFE_LAUNCH_EXIT;
            }
            break;

        // Launching bibi; start onGameEnter animation
        case RETROFE_LAUNCH_BIBI_ENTER:
            currentPage_->enterGame( );  // Start onGameEnter animation
            currentPage_->playSelect( ); // Play launch sound
            state = RETROFE_LAUNCH_BIBI_REQUEST;
            break;

        // Wait for onGameEnter animation to finish; launch ; start onGameExit animation
        case RETROFE_LAUNCH_BIBI_REQUEST:
            if ( currentPage_->isIdle( ) && !currentPage_->isSelectPlaying( ) )
            {
                launchEnter( );
                printf("BIBI !!!!\n");

                /* Restart audio amp */
                system(SHELL_CMD_AUDIO_AMP_ON);

                /* Execute game */
                if(system(BIBI_CMD) < 0)
                {
                    Logger::write(Logger::ZONE_ERROR, "Launcher", "Failed to launch bibi with cmd: \"" + std::string(BIBI_CMD) +"\"");
                }

                /* Stop audio amp */
                system(SHELL_CMD_AUDIO_AMP_OFF);

                /* Exit animation */
                launchExit( );
                currentPage_->exitGame( );

                state = RETROFE_LAUNCH_EXIT;
            }
            break;

        // Wait for onGameExit animation to finish
        case RETROFE_LAUNCH_EXIT:
            if ( currentPage_->isIdle( ) )
            {
                state = RETROFE_IDLE;
            }
            break;

        // Go back a page; start onMenuExit animation
        case RETROFE_BACK_REQUEST:
            if (currentPage_->getMenuDepth( ) == 1 )
            {
                currentPage_->stop( );
                m.clearPage( );
                menuMode_ = false;
            }
            else
            {
                currentPage_->exitMenu( );
            }
            state = RETROFE_BACK_MENU_EXIT;
            break;

        // Wait for onMenuExit animation to finish; load previous page; load art
        case RETROFE_BACK_MENU_EXIT:
            if ( currentPage_->isIdle( ) )
            {
                lastMenuOffsets_[currentPage_->getCollectionName( )]   = currentPage_->getScrollOffsetIndex( );
                lastMenuPlaylists_[currentPage_->getCollectionName( )] = currentPage_->getPlaylistName( );
                if (currentPage_->getMenuDepth( ) == 1)
                {
                    currentPage_->deInitialize( );
                    delete currentPage_;
                    currentPage_ = pages_.top( );
                    pages_.pop( );
                    currentPage_->allocateGraphicsMemory( );
                }
                else
                {
                    currentPage_->popCollection( );
                }
                config_.setProperty( "currentCollection", currentPage_->getCollectionName( ) );

                bool rememberMenu = false;
                config_.getProperty( "rememberMenu", rememberMenu );
                bool autoFavorites = true;
                config_.getProperty( "autoFavorites", autoFavorites );

                if (rememberMenu && lastMenuPlaylists_.find( currentPage_->getCollectionName( ) ) != lastMenuPlaylists_.end( ))
                {
                  currentPage_->selectPlaylist( lastMenuPlaylists_[currentPage_->getCollectionName( )] ); // Switch to last playlist
                }
                else if ( autoFavorites )
                {
                  currentPage_->selectPlaylist( "favorites" ); // Switch to favorites playlist
                }
                else
                {
                  currentPage_->selectPlaylist( "all" ); // Switch to all games playlist
                }

                if ( rememberMenu && lastMenuOffsets_.find( currentPage_->getCollectionName( ) ) != lastMenuOffsets_.end( ) )
                {
                    currentPage_->setScrollOffsetIndex( lastMenuOffsets_[currentPage_->getCollectionName( )] );
                }

                currentPage_->onNewItemSelected( );
                currentPage_->reallocateMenuSpritePoints( );
                state = RETROFE_BACK_MENU_LOAD_ART;
            }
            break;

        // Start onMenuEnter animation
        case RETROFE_BACK_MENU_LOAD_ART:
            currentPage_->enterMenu( );
            state = RETROFE_BACK_MENU_ENTER;
            break;

        // Wait for onMenuEnter animation to finish
        case RETROFE_BACK_MENU_ENTER:
            if ( currentPage_->isIdle( ) )
            {
                currentPage_->cleanup( );
                bool collectionInputClear = false;
                config_.getProperty( "collectionInputClear", collectionInputClear );
                if (  collectionInputClear  )
                {
                    // Empty event queue
                    SDL_Event e;
                    while ( SDL_PollEvent( &e ) );
                    input_.resetStates( );
                }
                state = RETROFE_IDLE;
            }
            break;

        // Start menu mode
        case RETROFE_MENUMODE_START_REQUEST:
            /*if ( currentPage_->isIdle( ) )
            {
                lastMenuOffsets_[currentPage_->getCollectionName( )]   = currentPage_->getScrollOffsetIndex( );
                lastMenuPlaylists_[currentPage_->getCollectionName( )] = currentPage_->getPlaylistName( );
                std::string layoutName;
                config_.getProperty( "layout", layoutName );
                PageBuilder pb( layoutName, "layout", config_, &fontcache_, true );
                Page *page = pb.buildPage( );
                if ( page )
                {
                    currentPage_->freeGraphicsMemory( );
                    pages_.push( currentPage_ );
                    currentPage_ = page;
                    menuMode_ = true;
                    m.setPage( page );
                }
                config_.setProperty( "currentCollection", "menu" );
                CollectionInfo *info = getMenuCollection( "menu" );
                currentPage_->pushCollection(info);
                currentPage_->onNewItemSelected( );
                currentPage_->reallocateMenuSpritePoints( );
                state = RETROFE_MENUMODE_START_LOAD_ART;
            }*/


            /// Launch menu
            menuMode_ = true;
            printf("Menu launched here\n");
            res = MenuMode::launch();
            menuMode_ = false;
            forceRender(true);

            /// Clear events
            SDL_Event ev;
            while ( SDL_PollEvent( &ev ) );
            input_.resetStates( );
            if(res == MENU_RETURN_EXIT){
                state = RETROFE_QUIT_REQUEST;
            }
            else{
                state = RETROFE_IDLE;
            }
            break;

        case RETROFE_MENUMODE_START_LOAD_ART:
            //currentPage_->start();
            state = RETROFE_MENUMODE_START_ENTER;
            break;

        case RETROFE_MENUMODE_START_ENTER:
            if ( currentPage_->isIdle( ) )
            {
                SDL_Event e;
                while ( SDL_PollEvent( &e ) );
                input_.resetStates( );
                state = RETROFE_IDLE;
            }
            break;

        // Wait for splash mode animation to finish
        case RETROFE_NEW:
            if ( currentPage_->isIdle( ) )
            {
                state = RETROFE_IDLE;
            }
            break;

        // Start the onExit animation
        case RETROFE_QUIT_REQUEST:
            currentPage_->stop( );
            state = RETROFE_QUIT;
            break;

        // Wait for onExit animation to finish before quitting RetroFE
        case RETROFE_QUIT:
            if ( currentPage_->isGraphicsIdle( ) )
            {
              running = false;
            }
            break;

        // Unknown state
        default:
            std::stringstream ss;
            ss << "Wrong state: " << state;
            Logger::write( Logger::ZONE_ERROR, "RetroFE", ss.str() );
            state = RETROFE_IDLE;
            break;
        }

        // Handle screen updates and attract mode
        if ( running )
        {
            // Handle FPS
            lastTime = currentTime_;
            currentTime_ = static_cast<float>( GET_RUN_TIME_MS ) / 1000;

            if ( currentTime_ < lastTime )
            {
                currentTime_ = lastTime;
            }

            deltaTime = currentTime_ - lastTime;

            double sleepTime = 1000.0/FPS - deltaTime*1000;
            if ( sleepTime > 0 )
            {
                SDL_Delay( static_cast<unsigned int>( sleepTime ) );
            }

            // ------- Check if previous update of page needed to be rendered -------
            if(!currentPage_->isIdle( ) || currentPage_->mustRender( ) || splashMode){
                //printf("Not idle\n");
                forceRender(true);
            }

            // Force refresh variables
#ifdef PERIOD_FORCE_REFRESH
            if(static_cast<int>(GET_RUN_TIME_MS) - ticks_last_refresh > PERIOD_FORCE_REFRESH){
                forceRender(true);
                //printf("force render\n");
            }
#endif  //PERIOD_FORCE_REFRESH

            // ------- Handle current pages updates -------
            if ( currentPage_ )
            {
                currentPage_->update( deltaTime );
            }

            // ------- Real render here -------
            if(mustRender_){
                //printf("render\n");
                mustRender_ = false;
                render( );
#ifdef PERIOD_FORCE_REFRESH
		ticks_last_refresh = static_cast<int>(GET_RUN_TIME_MS);
#endif  //PERIOD_FORCE_REFRESH
            }
        }
    }
}


// Check if we can go back a page or quite RetroFE
bool RetroFE::back(bool &exit)
{
    bool canGoBack  = false;
    bool exitOnBack = false;
    config_.getProperty( "exitOnFirstPageBack", exitOnBack );
    exit = false;

    if ( currentPage_->getMenuDepth( ) <= 1 && pages_.empty( ) )
    {
        exit = exitOnBack;
    }
    else
    {
        canGoBack = true;
    }

    return canGoBack;
}


// Force render retroFE
void RetroFE::forceRender( bool render )
{
    mustRender_ = true;
}


// Process the user input
RetroFE::RETROFE_STATE RetroFE::processUserInput( Page *page )
{
    bool exit = false;
    RETROFE_STATE state = RETROFE_IDLE;

#if 0
    // Poll all events until we find an active one
    SDL_Event e;
    while ( SDL_PollEvent( &e ) )
    {
        if ( e.type == SDL_QUIT  )
        {
            printf("How dare you interrupt me!\n");
            attract_.reset( );
            state = RETROFE_QUIT_REQUEST;

            /* Finish polling events */
            //SDL_Event e_trash;
            //while ( SDL_PollEvent( &e_trash ) );

            return state;
        }

        input_.update(e);
        /*if ( e.type == SDL_KEYDOWN && !e.key.repeat )
        {
            break;
        }*/
        if ( e.type == SDL_KEYDOWN  )
        {
            //printf("e.key.keysym.sym = %d\n", e.key.keysym.sym);

            /* Finish polling events */
            /*SDL_Event e_trash;
            while ( SDL_PollEvent( &e_trash ) );
            break;*/
        }
    }
#endif

#if 0
    // Poll all events until we find an active one
    SDL_Event e;
    while ( SDL_PollEvent( &e ) );

    if ( e.type == SDL_QUIT  )
    {
        printf("How dare you interrupt me!\n");
        attract_.reset( );
        state = RETROFE_QUIT_REQUEST;
        return state;
    }

    input_.update(e);
#endif


#if 1
    // Poll all events until we find an active one and process it
    SDL_Event e;
    while ( SDL_PollEvent( &e ) )
    {
        if ( e.type == SDL_QUIT  )
        {
            printf("How dare you interrupt me!\n");
            attract_.reset( );
            state = RETROFE_QUIT_REQUEST;

	    /* Finish polling events */
	    //SDL_Event e_trash;
	    //while ( SDL_PollEvent( &e_trash ) );

            return state;
        }

        input_.update(e);
    }
#endif

    //   ,-*  ,-*  ,-*
    //  (_)  (_)  (_)
    #define KONAMI_CODE_SIZE    10
    static const int konami_code_sdl[KONAMI_CODE_SIZE]={SDLK_u,SDLK_u,SDLK_d,SDLK_d,SDLK_l,SDLK_r,SDLK_l,SDLK_r,SDLK_b,SDLK_a};
    static uint8_t idx_konami_code = 0;
    if(e.type==SDL_KEYDOWN && e.key.keysym.sym != konami_code_sdl[idx_konami_code]) idx_konami_code=0;   
    if(e.type==SDL_KEYDOWN && e.key.keysym.sym == konami_code_sdl[idx_konami_code]) idx_konami_code++;
    if(idx_konami_code >= KONAMI_CODE_SIZE){
        idx_konami_code = 0;
        return RETROFE_LAUNCH_BIBI_ENTER;   
    }

    // Handle next/previous game inputs
    if ( page->isHorizontalScroll( ) )
    {
        if (input_.keystate(UserInput::KeyCodeLeft))
        {
            attract_.reset( );
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod( );
        }
        if (input_.keystate(UserInput::KeyCodeRight))
        {
            attract_.reset( );
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod( );
        }
    }
    else
    {
        if (input_.keystate(UserInput::KeyCodeUp))
        {
            attract_.reset( );
            page->setScrolling(Page::ScrollDirectionBack);
            page->scroll(false);
            page->updateScrollPeriod( );
        }
        if (input_.keystate(UserInput::KeyCodeDown))
        {
            attract_.reset( );
            page->setScrolling(Page::ScrollDirectionForward);
            page->scroll(true);
            page->updateScrollPeriod( );
        }
    }

    // Ignore other keys while the menu is scrolling
    if ( page->isMenuIdle( ) )
    {

        if ( input_.keystate(UserInput::KeyCodeMenu) && !menuMode_)
        {
            state = RETROFE_MENUMODE_START_REQUEST;
        }

        if ( menuMode_ || (
            !input_.keystate(UserInput::KeyCodePageUp) &&
            !input_.keystate(UserInput::KeyCodePageDown) &&
            !input_.keystate(UserInput::KeyCodeLetterUp) &&
            !input_.keystate(UserInput::KeyCodeLetterDown) &&
            !input_.keystate(UserInput::KeyCodeFavPlaylist) &&
            !input_.keystate(UserInput::KeyCodeNextPlaylist) &&
            !input_.keystate(UserInput::KeyCodePrevPlaylist) &&
            !input_.keystate(UserInput::KeyCodeAddPlaylist) &&
            !input_.keystate(UserInput::KeyCodeRemovePlaylist) &&
            !input_.keystate(UserInput::KeyCodeRandom) &&
            !input_.keystate(UserInput::KeyCodeMenu)) )
        {
            keyLastTime_ = 0;
            keyDelayTime_= 0.3f;
        }
        else if ( (currentTime_ - keyLastTime_) > keyDelayTime_ || keyLastTime_ == 0 )
        {
            keyLastTime_ = currentTime_;
            keyDelayTime_-= .05f;
            if ( keyDelayTime_< 0.1f ) keyDelayTime_= 0.1f;

            // All of these keys will mean new screen update => we must render
            forceRender(true);

            if (input_.keystate(UserInput::KeyCodePageUp))
            {
                attract_.reset( );
                page->pageScroll(Page::ScrollDirectionBack);
                state = RETROFE_MENUJUMP_REQUEST;
            }
            if (input_.keystate(UserInput::KeyCodePageDown))
            {
                attract_.reset( );
                page->pageScroll(Page::ScrollDirectionForward);
                state = RETROFE_MENUJUMP_REQUEST;
            }
            if (input_.keystate(UserInput::KeyCodeLetterUp))
            {
                attract_.reset( );
                page->letterScroll(Page::ScrollDirectionBack);
                state = RETROFE_MENUJUMP_REQUEST;
            }
            if (input_.keystate(UserInput::KeyCodeLetterDown))
            {
                attract_.reset( );
                page->letterScroll(Page::ScrollDirectionForward);
                state = RETROFE_MENUJUMP_REQUEST;
            }
            if ( input_.newKeyPressed(UserInput::KeyCodeFavPlaylist) )
            {
                attract_.reset( );
                page->favPlaylist( );
                state = RETROFE_PLAYLIST_REQUEST;
            }
            if ( input_.newKeyPressed(UserInput::KeyCodeNextPlaylist) )
            {
                attract_.reset( );
                page->nextPlaylist( );
                state = RETROFE_PLAYLIST_REQUEST;
            }
            if ( input_.newKeyPressed(UserInput::KeyCodePrevPlaylist) )
            {
                attract_.reset( );
                page->prevPlaylist( );
                state = RETROFE_PLAYLIST_REQUEST;
            }
            if ( input_.newKeyPressed(UserInput::KeyCodeRemovePlaylist) )
            {
                attract_.reset( );
                page->removePlaylist( );
                state = RETROFE_PLAYLIST_REQUEST;
            }
            if ( input_.newKeyPressed(UserInput::KeyCodeAddPlaylist) )
            {
                attract_.reset( );
                page->addPlaylist( );
                state = RETROFE_PLAYLIST_REQUEST;
            }
            if ( input_.keystate(UserInput::KeyCodeRandom) )
            {
                attract_.reset( );
                page->selectRandom( );
                state = RETROFE_MENUJUMP_REQUEST;
            }
        }

        if (input_.keystate(UserInput::KeyCodeAdminMode))
        {
            //todo: add admin mode support
        }

        if (input_.keystate(UserInput::KeyCodeSelect))
        {
            attract_.reset( );
            nextPageItem_ = page->getSelectedItem( );

            if ( nextPageItem_ )
            {
                if ( nextPageItem_->leaf )
                {
                    if ( menuMode_ )
                    {
                        state = RETROFE_HANDLE_MENUENTRY;
                    }
                    else
                    {
                        state = RETROFE_LAUNCH_ENTER;
                    }
                }
                else
                {
                    state = RETROFE_NEXT_PAGE_REQUEST;
                }
            }
        }

        if (input_.keystate(UserInput::KeyCodeBack))
        {
            attract_.reset( );
            if ( back( exit ) || exit )
            {
                state = (exit) ? RETROFE_QUIT_REQUEST : RETROFE_BACK_REQUEST;
            }
        }

        if (input_.keystate(UserInput::KeyCodeQuit))
        {
            attract_.reset( );
            state = RETROFE_QUIT_REQUEST;
        }
    }

    // Check if we're done scrolling
    if ( !input_.keystate(UserInput::KeyCodeUp) &&
         !input_.keystate(UserInput::KeyCodeLeft) &&
         !input_.keystate(UserInput::KeyCodeDown) &&
         !input_.keystate(UserInput::KeyCodeRight) &&
         !input_.keystate(UserInput::KeyCodePageUp) &&
         !input_.keystate(UserInput::KeyCodePageDown) &&
         !input_.keystate(UserInput::KeyCodeLetterUp) &&
         !input_.keystate(UserInput::KeyCodeLetterDown) &&
         !attract_.isActive( ) )
    {
        page->resetScrollPeriod( );
        if (page->isMenuScrolling( ))
        {
            attract_.reset( );
            state = RETROFE_HIGHLIGHT_REQUEST;
        }
    }

    return state;
}


// Load a page
Page *RetroFE::loadPage( )
{
    std::string layoutName;
    bool userLayout;
    config_.getProperty( "layout", layoutName );
    config_.getProperty( "userTheme", userLayout );
    PageBuilder pb( layoutName, "layout", config_, &fontcache_, false, userLayout);
    Page *page = pb.buildPage( );

    if ( !page )
    {
        Logger::write( Logger::ZONE_ERROR, "RetroFE", "Could not create page" );
    }

    return page;
}


// Load the splash page
Page *RetroFE::loadSplashPage( )
{
    std::string layoutName;
    bool userLayout;
    config_.getProperty( "layout", layoutName );
    config_.getProperty( "userTheme", userLayout );
    PageBuilder pb( layoutName, "splash", config_, &fontcache_, false, userLayout);
    Page * page = pb.buildPage( );
    page->start( );

    return page;
}


// Load a collection
CollectionInfo *RetroFE::getCollection(std::string collectionName)
{

    // Check if subcollections should be merged or split
    bool subsSplit = false;
    config_.getProperty( "subsSplit", subsSplit );

    // Build the collection
    CollectionInfoBuilder cib(config_, *metadb_);
    CollectionInfo *collection = cib.buildCollection( collectionName );
    collection->subsSplit = subsSplit;
    cib.injectMetadata( collection );

    DIR *dp;
    struct dirent *dirp;

    std::string path = Utils::combinePath( Configuration::absolutePath, "collections", collectionName );
    dp = opendir( path.c_str( ) );

    // Loading sub collection files
    while ( (dirp = readdir( dp )) != NULL )
    {
        std::string file = dirp->d_name;

        size_t position = file.find_last_of( "." );
        std::string basename = (std::string::npos == position)? file : file.substr( 0, position );

        std::string comparator = ".sub";
        int start = file.length( ) - comparator.length( );

        if ( start >= 0 )
        {
            if ( file.compare( start, comparator.length( ), comparator ) == 0 )
            {
                Logger::write( Logger::ZONE_INFO, "RetroFE", "Loading subcollection into menu: " + basename );

                CollectionInfo *subcollection = cib.buildCollection( basename, collectionName );
                collection->addSubcollection( subcollection );
                subcollection->subsSplit = subsSplit;
                cib.injectMetadata( subcollection );
            }
        }
    }
    closedir( dp );

    bool menuSort = true;
    config_.getProperty( "collections." + collectionName + ".list.menuSort", menuSort );

    if ( menuSort )
    {
        collection->sortItems( );
    }

    MenuParser mp;
    mp.buildMenuItems( collection, menuSort);

    cib.addPlaylists( collection );
    collection->sortPlaylists( );

    // Add extra info, if available
    for ( std::vector<Item *>::iterator it = collection->items.begin( ); it != collection->items.end( ); it++ )
    {
        std::string path = Utils::combinePath( Configuration::absolutePath, "collections", collectionName, "info", (*it)->name + ".conf" );
        (*it)->loadInfo( path );
    }

    // Remove parenthesis and brackets, if so configured
    bool showParenthesis    = true;
    bool showSquareBrackets = true;

    (void)config_.getProperty( "showParenthesis", showParenthesis );
    (void)config_.getProperty( "showSquareBrackets", showSquareBrackets );

    typedef std::map<std::string, std::vector <Item *> *> Playlists_T;
    for ( Playlists_T::iterator itP = collection->playlists.begin( ); itP != collection->playlists.end( ); itP++ )
    {
        for ( std::vector <Item *>::iterator itI = itP->second->begin( ); itI != itP->second->end( ); itI++ )
        {
            if ( !showParenthesis )
            {
                std::string::size_type firstPos  = (*itI)->title.find_first_of( "(" );
                std::string::size_type secondPos = (*itI)->title.find_first_of( ")", firstPos );

                while ( firstPos != std::string::npos && secondPos != std::string::npos )
                {
                    firstPos  = (*itI)->title.find_first_of( "(" );
                    secondPos = (*itI)->title.find_first_of( ")", firstPos );

                    if ( firstPos != std::string::npos )
                    {
                        (*itI)->title.erase( firstPos, (secondPos - firstPos) + 1 );
                    }
                }
            }
            if ( !showSquareBrackets )
            {
                std::string::size_type firstPos  = (*itI)->title.find_first_of( "[" );
                std::string::size_type secondPos = (*itI)->title.find_first_of( "]", firstPos );

                while ( firstPos != std::string::npos && secondPos != std::string::npos )
                {
                    firstPos  = (*itI)->title.find_first_of( "[" );
                    secondPos = (*itI)->title.find_first_of( "]", firstPos );

                    if ( firstPos != std::string::npos && secondPos != std::string::npos )
                    {
                        (*itI)->title.erase( firstPos, (secondPos - firstPos) + 1 );
                    }
                }
            }
        }
    }

    return collection;
}


// Load a menu
CollectionInfo *RetroFE::getMenuCollection( std::string collectionName )
{
    std::string menuPath = Utils::combinePath( Configuration::absolutePath, "menu" );
    std::string menuFile = Utils::combinePath( menuPath, collectionName + ".txt" );
    std::vector<Item *> menuVector;
    CollectionInfoBuilder cib( config_, *metadb_ );
    CollectionInfo *collection = new CollectionInfo( collectionName, menuPath, "", "", "" );
    cib.ImportBasicList( collection, menuFile, menuVector );
    for ( std::vector<Item *>::iterator it = menuVector.begin( ); it != menuVector.end( ); ++it)
    {
        (*it)->leaf = false;
        size_t position = (*it)->name.find( "=" );
        if ( position != std::string::npos )
        {
            (*it)->ctrlType  = Utils::trimEnds( (*it)->name.substr( position+1, (*it)->name.size( )-1 ) );
            (*it)->name      = Utils::trimEnds( (*it)->name.substr( 0, position ) );
            (*it)->title     = (*it)->name;
            (*it)->fullTitle = (*it)->name;
            (*it)->leaf      = true;
        }
        (*it)->collectionInfo = collection;
        collection->items.push_back( *it );
    }
    collection->playlists["all"] = &collection->items;
    return collection;
}



/* Handler for SIGUSR1, caused by closing the console */
void RetroFE::handle_sigusr1(int sig)
{
    printf("Caught signal USR1 %d\n", sig);
    std::stringstream ss;
    ss << "Caught signal USR1: " << sig;
    Logger::write( Logger::ZONE_ERROR, "RetroFE", ss.str() );

    /* Exit menu if it was launched */
    MenuMode::stop();

    /** Poweroff */
    quick_poweroff();
}


/* Quick save and turn off the console */
void RetroFE::quick_poweroff()
{
    /* Send command to cancel any previously scheduled powerdown */
    if (popen(SHELL_CMD_POWERDOWN_HANDLE, "r") == NULL)
    {
        /* Countdown is still ticking, so better do nothing
           than start writing and get interrupted!
        */
        printf("Failed to cancel scheduled shutdown\n");
        std::stringstream ss;
        ss << "Failed to run command " << SHELL_CMD_POWERDOWN_HANDLE;
        Logger::write( Logger::ZONE_ERROR, "RetroFE", ss.str() );
        exit(0);
    }

    /* Perform Instant Play save and shutdown */
    execlp(SHELL_CMD_POWERDOWN, SHELL_CMD_POWERDOWN);

    /* Should not be reached */
    printf("Failed to perform shutdown\n");

    /* Exit Emulator */
    exit(0);
}
