/****************************************************************************************************

RepRapFirmware - G Codes

This class interprets G Codes from one or more sources, and calls the functions in Move, Heat etc
that drive the machine to do what the G Codes command.

-----------------------------------------------------------------------------------------------------

Version 0.1

13 February 2013

Adrian Bowyer
RepRap Professional Ltd
http://reprappro.com

Licence: GPL

****************************************************************************************************/

#ifndef GCODES_H
#define GCODES_H

#define STACK 5
#define GCODE_LENGTH 100 // Maximum length of internally-generated G Code string

#define AXIS_LETTERS { 'X', 'Y', 'Z' }			// The axes in a GCode
#define FEEDRATE_LETTER 'F'						// GCode feedrate
#define EXTRUDE_LETTER 'E'						// GCode extrude

typedef uint16_t EndstopChecks;					// must be large enough to hold a bitmap of drive numbers or ZProbeActive


// Small class to hold an individual GCode and provide functions to allow it to be parsed

class GCodeBuffer
{
  public:
    GCodeBuffer(Platform* p, const char* id);
    //const char *Identity() const { return identity; }
    void Init(); 										// Set it up
    bool Put(char c);									// Add a character to the end
    bool Put(const char *str, size_t len);				// Add an entire string
    bool IsEmpty() const;								// Does this buffer contain any code?
    unsigned int Length() const;						// How many bytes have been fed into this buffer?
    bool Seen(char c);									// Is a character present?
    float GetFValue();									// Get a float after a key letter
    int GetIValue();									// Get an integer after a key letter
    long GetLValue();									// Get a long integer after a key letter
    const char* GetUnprecedentedString(bool optional = false);	// Get a string with no preceding key letter
    const char* GetString();							// Get a string after a key letter
    const void GetFloatArray(float a[], int& length);	// Get a :-separated list of floats after a key letter
    const void GetLongArray(long l[], int& length);		// Get a :-separated list of longs after a key letter
    const char* Buffer() const;
    bool Active() const;
    void SetFinished(bool f);							// Set the G Code executed (or not)
    void Pause();
    void Resume();
    const char* WritingFileDirectory() const;			// If we are writing the G Code to a file, where that file is
    void SetWritingFileDirectory(const char* wfd);		// Set the directory for the file to write the GCode in
    int GetToolNumberAdjust() const { return toolNumberAdjust; }
    void SetToolNumberAdjust(int arg) { toolNumberAdjust = arg; }
    void SetCommsProperties(uint32_t arg) { checksumRequired = (arg & 1); }

  private:

    enum State { idle, executing, paused };
    int CheckSum();										// Compute the checksum (if any) at the end of the G Code
    Platform* platform;									// Pointer to the RepRap's controlling class
    char gcodeBuffer[GCODE_LENGTH];						// The G Code
    const char* identity;								// Where we are from (web, file, serial line etc)
    int gcodePointer;									// Index in the buffer
    int readPointer;									// Where in the buffer to read next
    bool inComment;										// Are we after a ';' character?
    bool checksumRequired;								// True if we only accept commands with a valid checksum
    State state;										// Idle, executing or paused
    const char* writingFileDirectory;					// If the G Code is going into a file, where that is
    int toolNumberAdjust;
};

//****************************************************************************************************

// The item class for the internal code queue

// We don't want some commands to be executed too early, so we should queue these codes and execute
// them just in time in GCodes::Spin while regular moves are being fed into the look-ahead queue.

const unsigned int codeQueueLength = 8;

class CodeQueueItem
{
	public:

		CodeQueueItem(CodeQueueItem *n);
		void Init(GCodeBuffer *gb, unsigned int executeAtMove);
		void SetNext(CodeQueueItem *n);
		CodeQueueItem *Next() const;

		unsigned int ExecuteAtMove() const;
		const char *GetCommand() const;
		size_t GetCommandLength() const;
		GCodeBuffer *GetSource() const;

		void Execute();
		bool IsExecuting() const;

	private:

		char command[GCODE_LENGTH];
		size_t commandLength;
		unsigned int moveToExecute;

		GCodeBuffer *source;
		CodeQueueItem *next;
		bool executing;
};

//****************************************************************************************************

// The GCode interpreter

class GCodes
{   
  public:
  
    GCodes(Platform* p, Webserver* w);
    void Spin();														// Called in a tight loop to make this class work
    void Init();														// Set it up
    void Exit();														// Shut it down
    void Reset();														// Reset some parameter to defaults
    bool ReadMove(float* m, EndstopChecks& ce);							// Called by the Move class to get a movement set by the last G Code
    void QueueFileToPrint(const char* fileName);						// Open a file of G Codes to run
    void DeleteFile(const char* fileName);								// Does what it says
    bool GetProbeCoordinates(int count, float& x, float& y, float& z) const;	// Get pre-recorded probe coordinates
    const char* GetCurrentCoordinates() const;							// Get where we are as a string
    float FractionOfFilePrinted() const;								// Returns the current file-based progress or -1.0 if no file is being printed
    bool PrintingAFile() const;											// Are we in the middle of printing a file?
    bool DoingFileMacro() const;										// Or still busy processing a macro file?
    void Diagnostics();													// Send helpful information out
    bool HaveIncomingData() const;										// Is there something that we have to do?
    bool GetAxisIsHomed(uint8_t axis) const { return axisIsHomed[axis]; } // Is the axis at 0?
    void SetAxisIsHomed(uint8_t axis) { axisIsHomed[axis] = true; }		// Tell us that the axis is now homes
    bool CoolingInverted() const;										// Is the current fan value inverted?
    void MoveQueued();													// Called by the Move class to announce a new move
    void MoveCompleted();												// Called by the DDA class to indicate that a move has been completed (called by ISR)
    bool HaveAux() const;												// Any device on the AUX line?
    
    bool IsPausing() const;
    bool IsResuming() const;

  private:
  
    void DoFilePrint(GCodeBuffer* gb);									// Get G Codes from a file and print them
    bool AllMovesAreFinishedAndMoveBufferIsLoaded();					// Wait for move queue to exhaust and the current position is loaded
    bool DoCannedCycleMove(EndstopChecks ce);							// Do a move from an internally programmed canned cycle
    bool DoFileMacro(const char* fileName);								// Run a GCode macro in a file
    bool FileMacroCyclesReturn();										// End a macro
    bool CanQueueCode(GCodeBuffer *gb) const;							// Can we queue this code for delayed execution?
    bool ActOnCode(GCodeBuffer* gb, bool executeImmediately = false);	// Do a G, M or T Code
    bool HandleGcode(GCodeBuffer* gb);									// Do a G code
    bool HandleMcode(GCodeBuffer* gb);									// Do an M code
    bool HandleTcode(GCodeBuffer* gb);									// Do a T code
    void CancelPrint();													// Cancel the current print
    int SetUpMove(GCodeBuffer* gb);										// Pass a move on to the Move module
    bool DoDwell(GCodeBuffer *gb);										// Wait for a bit
    bool DoDwellTime(float dwell);										// Really wait for a bit
    bool DoHome(StringRef& reply, bool& error);							// Home some axes
    bool DoSingleZProbeAtPoint();										// Probe at a given point
    bool DoSingleZProbe();												// Probe where we are
    bool SetSingleZProbeAtAPosition(GCodeBuffer *gb, StringRef& reply);	// Probes at a given position - see the comment at the head of the function itself
    bool SetBedEquationWithProbe(StringRef& reply);						// Probes a series of points and sets the bed equation
    bool SetPrintZProbe(GCodeBuffer *gb, StringRef& reply);				// Either return the probe value, or set its threshold
    void SetOrReportOffsets(StringRef& reply, GCodeBuffer *gb);			// Deal with a G10
    bool SetPositions(GCodeBuffer *gb);									// Deal with a G92
    void SetPositions(float positionNow[DRIVES]);						// Set the current position to be this
    bool LoadMoveBufferFromGCode(GCodeBuffer *gb,  						// Set up a move for the Move class
    		bool doingG92, bool applyLimits);
    bool NoHome() const;												// Are we homing and not finished?
    bool Push();														// Push feedrate etc on the stack
    bool Pop();															// Pop feedrate etc
    void DisableDrives();												// Turn the motors off
    void SetEthernetAddress(GCodeBuffer *gb, int mCode);				// Does what it says
    void SetMACAddress(GCodeBuffer *gb);								// Deals with an M540
    void HandleReply(bool error,						 				// Handle G-Code replies
    		const char* reply, char gMOrT, int code, bool resend);
    bool OpenFileToWrite(const char* directory,							// Start saving GCodes in a file
    		const char* fileName, GCodeBuffer *gb);
    void WriteGCodeToFile(GCodeBuffer *gb);								// Write this GCode into a file
    bool SendConfigToLine();											// Deal with M503
    void WriteHTMLToFile(char b, GCodeBuffer *gb);						// Save an HTML file (usually to upload a new web interface)
    bool OffsetAxes(GCodeBuffer *gb);									// Set offsets - deprecated, use G10
    void SetPidParameters(GCodeBuffer *gb, int heater, StringRef& reply);	// Set the P/I/D parameters for a heater
    void SetHeaterParameters(GCodeBuffer *gb, StringRef& reply);		// Set the thermistor and ADC parameters for a heater
    void ManageTool(GCodeBuffer *gb, StringRef& reply);					// Create a new tool definition
    void SetToolHeaters(Tool *tool, float temperature);					// Set all a tool's heaters to the temperature.  For M104...
    bool ChangeTool(int newToolNumber);									// Select a new tool
    bool ToolHeatersAtSetTemperatures(const Tool *tool) const;			// Wait for the heaters associated with the specified tool to reach their set temperatures

    Platform* platform;							// The RepRap machine
    bool active;								// Live and running?
    Webserver* webserver;						// The webserver class
    float dwellTime;							// How long a pause for a dwell (seconds)?
    bool dwellWaiting;							// We are in a dwell
    GCodeBuffer* webGCode;						// The sources...
    GCodeBuffer* fileGCode;						// ...
    GCodeBuffer* serialGCode;					// ...
    GCodeBuffer* auxGCode;						// ...
    GCodeBuffer* fileMacroGCode;				// ...
    GCodeBuffer* queuedGCode;					// ... of G Codes
    bool moveAvailable;							// Have we seen a move G Code and set it up?
    float moveBuffer[DRIVES+1]; 				// Move coordinates; last is feed rate
    EndstopChecks endStopsToCheck;				// Which end stops we check them on the next move
    bool drivesRelative; 						// Are movements relative - all except X, Y and Z
    bool axesRelative;   						// Are movements relative - X, Y and Z
    bool drivesRelativeStack[STACK];			// For dealing with Push and Pop
    bool axesRelativeStack[STACK];				// For dealing with Push and Pop
    float feedrateStack[STACK];					// For dealing with Push and Pop
    float extruderPositionStack[STACK][DRIVES-AXES];	// For dealing with Push and Pop
    FileData fileStack[STACK];
    bool doingFileMacroStack[STACK];			// For dealing with Push and Pop
    int8_t stackPointer;						// Push and Pop stack pointer
    char axisLetters[AXES]; 					// 'X', 'Y', 'Z'
    float lastExtruderPosition[DRIVES - AXES];	// Extruder position of the last move fed into the Move class
	float record[DRIVES+1];						// Temporary store for move positions
	float moveToDo[DRIVES+1];					// Where to go set by G1 etc
	bool activeDrive[DRIVES+1];					// Is this drive involved in a move?
	bool offSetSet;								// Are any axis offsets non-zero?
    float distanceScale;						// MM or inches
    FileData fileBeingPrinted;
    FileData fileToPrint;
    FileStore* fileBeingWritten;				// A file to write G Codes (or sometimes HTML) in
    FileStore* configFile;						// A file containing a macro
    bool doingFileMacro, returningFromMacro;	// Are we executing a macro file?
    bool isPausing, isResuming;					// Are we dealing with a print interrupt?
    bool doPauseMacro;							// Do we need to run pause.g and resume.g?
    float fractionOfFilePrinted;				// Only used to record the main file when a macro is being printed
    char* eofString;							// What's at the end of an HTML file?
    uint8_t eofStringCounter;					// Check the...
    uint8_t eofStringLength;					// ... EoF string as we read.
    bool homing;								// Are we homing any axes?
    bool homeX;									// True to home the X axis this move
    bool homeY;									// True to home the Y axis this move
    bool homeZ;									// True to home the Z axis this move
    int probeCount;								// Counts multiple probe points
    int8_t cannedCycleMoveCount;				// Counts through internal (i.e. not macro) canned cycle moves
    bool cannedCycleMoveQueued;					// True if a canned cycle move has been set
    bool zProbesSet;							// True if all Z probing is done and we can set the bed equation
    bool settingBedEquationWithProbe;			// True if we're executing G32 without a macro
    float longWait;								// Timer for things that happen occasionally (seconds)
    bool limitAxes;								// Don't think outside the box.
    bool axisIsHomed[3];						// These record which of the axes have been homed
    bool waitingForMoveToComplete;
    bool coolingInverted;
    float lastFanValue;
    int8_t toolChangeSequence;					// Steps through the tool change procedure
    CodeQueueItem *internalCodeQueue;			// Linked list of all the queued codes
    CodeQueueItem *releasedQueueItems;			// Linked list of all released queue items
    unsigned int totalMoves;					// Total number of moves that have been fed into the look-ahead
    volatile unsigned int movesCompleted;		// Number of moves that have been completed (changed by ISR)
    bool auxDetected;							// Have we processed at least one G-Code from an AUX device?
};

//*****************************************************************************************************

// Get an Int after a G Code letter

inline int GCodeBuffer::GetIValue()
{
  return (int)GetLValue();
}

inline const char* GCodeBuffer::Buffer() const
{
  return gcodeBuffer;
}

inline bool GCodeBuffer::Active() const
{
  return state == executing;
}

inline void GCodeBuffer::SetFinished(bool f)
{
	if (f)
	{
		state = idle;
		gcodeBuffer[0] = 0;
	}
	else
	{
		f = executing;
	}
}

inline void GCodeBuffer::Pause()
{
	if (state == executing)
	{
		state = paused;
	}
}

inline void GCodeBuffer::Resume()
{
	if (state == paused)
	{
		state = executing;
	}
}

inline const char* GCodeBuffer::WritingFileDirectory() const
{
	return writingFileDirectory;
}

inline void GCodeBuffer::SetWritingFileDirectory(const char* wfd)
{
	writingFileDirectory = wfd;
}

inline bool GCodes::PrintingAFile() const
{
	return (FractionOfFilePrinted() >= 0.0);
}

inline bool GCodes::DoingFileMacro() const
{
	return doingFileMacro;
}

inline bool GCodes::HaveIncomingData() const
{
	return fileBeingPrinted.IsLive() ||
			webserver->GCodeAvailable() ||
			(platform->GetLine()->Status() & byteAvailable) ||
			(platform->GetAux()->Status() & byteAvailable);
}

inline bool GCodes::NoHome() const
{
   return !(homeX || homeY || homeZ);
}

inline bool GCodes::CoolingInverted() const
{
	return coolingInverted;
}

#endif
