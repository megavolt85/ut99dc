/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#include "LaunchPrivate.h"

#include <SDL2/SDL.h>

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern "C" {TCHAR THIS_PACKAGE[64]=TEXT("Launch");}

// Memory allocator.
#include "FMallocAnsi.h"
FMallocAnsi Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
#include "FFileManagerLinux.h"
FFileManagerLinux FileManager;

// Config.
#include "FConfigCacheIni.h"


/*-----------------------------------------------------------------------------
	Initialization
-----------------------------------------------------------------------------*/

//
// Creates a UEngine object.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
	DOUBLE LoadTime = appSeconds();

	// Set exec hook.
	GExec = NULL;

	// Update first-run.
	INT FirstRun=0;
	if (FirstRun<ENGINE_VERSION)
		FirstRun = ENGINE_VERSION;
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );

	// Create the global engine object.
	UClass* EngineClass;
	EngineClass = UObject::StaticLoadClass(
		UGameEngine::StaticClass(), NULL, 
		TEXT("ini:Engine.Engine.GameEngine"), 
		NULL, LOAD_NoFail, NULL 
	);
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();
	debugf( TEXT("Startup time: %f seconds."), appSeconds()-LoadTime );

	return Engine;
	unguard;
}

//
// Handle an error.
//
void HandleError( const char* Exception )
{
	GIsGuarded=0;
	GIsCriticalError=1;
	debugf( NAME_Exit, "Shutting down after catching exception" );
	debugf( NAME_Exit, "Exiting due to exception" );
	GErrorHist[ARRAY_COUNT(GErrorHist)-1]=0;
#ifdef PLATFORM_SDL
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, LocalizeError("Critical"), GErrorHist, SDL_GetKeyboardFocus() );
#elif defined(PLATFORM_DREAMCAST)
	if( Exception )
		FatalError( "FATAL ERROR:\n%s\n\n%s", Exception, GErrorHist );
	else
		FatalError( "FATAL ERROR:\n%s", GErrorHist );
#endif
}

/*-----------------------------------------------------------------------------
	Main Loop
-----------------------------------------------------------------------------*/

//
// X game message loop.
//
static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	// Loop while running.
	GIsRunning = 1;
	DOUBLE OldTime = appSeconds();
	DOUBLE SecondStartTime = OldTime;
	INT TickCount = 0;
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		guard(UpdateWorld);
		DOUBLE NewTime   = appSeconds();
		FLOAT  DeltaTime = NewTime - OldTime;
		Engine->Tick( DeltaTime );
		if( GWindowManager )
			GWindowManager->Tick( DeltaTime );
		OldTime = NewTime;
		TickCount++;
		if( OldTime > SecondStartTime + 1 )
		{
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
			SecondStartTime = OldTime;
			TickCount = 0;
		}
		unguard;

		// Enforce optional maximum tick rate.
		guard(EnforceTickRate);
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
		unguard;
	}
	GIsRunning = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

//
// Simple copy.
// 

void SimpleCopy(TCHAR* fromfile, TCHAR* tofile)
{
	INT c;
	FILE* from;
	FILE* to;
	from = fopen(fromfile, "r");
	if (from == NULL)
		return;
	to = fopen(tofile, "w");
	if (to == NULL)
	{
		printf("Can't open or create %s", tofile);
		return;
	}
	while ((c = getc(from)) != EOF)
		putc(c, to);
	fclose(from);
	fclose(to);
}

//
// Exit wound.
//
int CleanUpOnExit(int ErrorLevel)
{
	GFileManager->Delete(TEXT("Running.ini"),0,0);
	debugf( NAME_Title, LocalizeGeneral("Exit") );
	appPreExit();
	GIsGuarded = 0;

	// Shutdown.
	appExit();
	GIsStarted = 0;

	// Restore the user's configuration.
	TCHAR baseconfig[PATH_MAX] = TEXT("");
	if( getcwd(baseconfig, sizeof(baseconfig)) == NULL )
	{
		appStrcpy( baseconfig, TEXT("./User.ini") );
	}
	else
	{
		appStrcat(baseconfig, "/User.ini");
	}

	TCHAR userconfig[PATH_MAX] = TEXT("");
	sprintf(userconfig, "~/.utconf");

	TCHAR exec[PATH_MAX] = TEXT("");
	sprintf(exec, "cp -f %s %s", baseconfig, userconfig);
	//system( exec );

	return ErrorLevel;
}

//
// Entry point.
//
int main( int argc, char* argv[] )
{
	try
	{
	guard(main);
	
	INT ErrorLevel = 0;
	GIsStarted	   = 1;

	// Set module name.
	appStrcpy( GModule, argv[0] );

	// Set the package name.
	appStrcpy( THIS_PACKAGE, appPackage() );	

	// Get the command line.
	TCHAR CmdLine[1024], *CmdLinePtr=CmdLine;
	*CmdLinePtr = 0;
	for( INT i=1; i<argc; i++ )
	{
		if( i>1 )
			appStrcat( CmdLine, " " );
		appStrcat( CmdLine, argv[i] );
	}

	// Take care of .ini swapping.
	TCHAR userconfig[PATH_MAX] = TEXT("");
	sprintf(userconfig, "~/.utconf");

	TCHAR baseconfig[PATH_MAX] = TEXT("");
	if( getcwd(baseconfig, sizeof(baseconfig)) == NULL )
	{
		appStrcpy( baseconfig, TEXT("./User.ini") );
	}
	else
	{
		appStrcat(baseconfig, "/User.ini");
	}

	TCHAR exec[PATH_MAX] = TEXT("");
	sprintf(exec, "cp -f %s %s", userconfig, baseconfig);
	//system( exec );

	//SimpleCopy( userconfig, baseconfig );

	// Init core.
	GIsClient = 1; 
	GIsGuarded = 0;
	appInit( TEXT("UnrealTournament"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );

	// Init mode.
	GIsServer		= 1;
	GIsClient		= !ParseParam(appCmdLine(), TEXT("SERVER"));
	GIsEditor		= 0;
	GIsScriptable	= 1;
	GLazyLoad		= !GIsClient || ParseParam(appCmdLine(), TEXT("LAZY"));

		// Init console log.
	if (ParseParam(CmdLine, TEXT("LOG")))
	{
			Warn.AuxOut = GLog;
			GLog		= &Warn;
	}
	

	// Init engine.
	UEngine* Engine = InitEngine();
	if( Engine )
	{
		debugf( NAME_Title, LocalizeGeneral("Run") );

		// Optionally Exec and exec file.
		FString Temp;
		if( Parse(CmdLine, TEXT("EXEC="), Temp) )
		{
			Temp = FString(TEXT("exec ")) + Temp;
			if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
				Engine->Client->Viewports(0)->Exec( *Temp, *GLog );
		}

		// Start main engine loop.
		debugf( TEXT("Entering main loop.") );
		if ( !GIsRequestingExit )
			MainLoop( Engine );
	}

	// Finish up.
	return CleanUpOnExit(ErrorLevel);

	unguard;
	}
	catch (...)
	{
		// Chained abort.  Do cleanup.
		HandleError(NULL);
		return CleanUpOnExit(1);
	}
}