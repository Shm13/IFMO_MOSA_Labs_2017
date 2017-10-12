#include "precomp.h"
//sukkkkkerwer
// keyboard handler
HANDLE hKeyboard;

// event to wait on for keyboard input
HANDLE hEvent;

// input buffer
CHAR line[1024];



NTSTATUS
cliOpenInputDevice (OUT PHANDLE handle, WCHAR* deviceName) // open input device such as keyboards
{
    UNICODE_STRING driver;
    OBJECT_ATTRIBUTES objectAttrs;
    IO_STATUS_BLOCK iosb; // status and information about the requested operation (FILE_CREATED, FILE_OPENED, etc...)
    HANDLE hDriver;
    NTSTATUS status;

    RtlInitUnicodeString (&driver, L"\\Device\\KeyboardClass0"); // make unicode from wchar*, from winternl.h
    
    // initialize the object attributes, from Ntdef.h (ntddk.h)
    InitializeObjectAttributes (&objectAttrs,			// save attrs here
                                &driver, 				// device name here
                                OBJ_CASE_INSENSITIVE, 	// atributes
                                NULL,  					// root dir - do not need
                                NULL); 					// set description, optional

    // open a handle to it, from winternl.h
    status = NtCreateFile (&hDriver, // save handler here 
                           SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, 	// desired access
                           &objectAttrs,										// objectAttributes
                           &iosb,												// status block
                           NULL,												// initial allocation size, optional
                           FILE_ATTRIBUTE_NORMAL,								// the file attributes, default
                           0,													// type of share access
                           FILE_OPEN,											// specifies what to do
                           FILE_DIRECTORY_FILE,					// the file being created or opened is a directory file
                           NULL,								// pointer to an EA buffer used to pass extended attributes
                           0);									// length of the EA buffer

    // Create an event that will be used to wait on the device
    InitializeObjectAttributes (&objectAttrs, NULL, 0, NULL, NULL);
    status = NtCreateEvent (&hEvent, 			// save attrs here
						    EVENT_ALL_ACCESS,   // all possible access rights to the event object
						    &objectAttrs,   	// objectAttributes
						    1, 					// event type (NotificationEvent)
						    0);					// initial state (TRUE/FALSE)

    // Return the handle
    *handle = hDriver;
    return status;
}

// routine waits for input from an input device
NTSTATUS
cliWaitForInput (IN HANDLE hDriver,       	// device handler
                 IN PVOID buffer,         	// input buffer
                 IN OUT PULONG bufferSize)	// size of input buffer
{
    IO_STATUS_BLOCK iosb; 		// status and information about the requested operation (FILE_CREATED, FILE_OPENED, etc...)
    LARGE_INTEGER byteOffset;
    NTSTATUS status;

    RtlZeroMemory (&iosb, sizeof(iosb)); 			 // fills a block of memory with zeros
    RtlZeroMemory (&byteOffset, sizeof(byteOffset)); // fills a block of memory with zeros

    // Try to read the data
    status = NtReadFile (hDriver, 		// handle to the file object
                         hEvent,  		// a handle to an event object
                         NULL,			// reserved
                         NULL,			// reserved
                         &iosb, 		// status block
                         buffer,		// buffer that receives the data read from the file
                         *bufferSize,   // the size, in bytes, of the buffer pointed to by Buffer
                         &byteOffset,   // starting byte offset in the file where the read operation will begin (LARGE_INTEGER)
                         NULL);			// device and intermediate drivers should set this pointer to NULL (optional)

    // Check if data is pending and indeterminate state
    if (status == STATUS_PENDING) // if I/O is in progress
    {
        // Wait on the data to be read
        status = NtWaitForSingleObject (hEvent, // waits until the hEvent object attains a state of signaled
									   TRUE, 	// the alert can be delivered
								       NULL); 	// timeout 
    }

    // Return status and how much data was read
    *bufferSize = (ULONG)iosb.Information;
    return status;
}

// get one character
CHAR
сliGetChar (IN HANDLE hDriver)
{
	KEYBOARD_INPUT_DATA keyboardData;
	KBD_RECORD kbd_rec;
	ULONG bufferLength = sizeof(KEYBOARD_INPUT_DATA);

	cliWaitForInput(hDriver, &keyboardData, &bufferLength);

	IntTranslateKey(&keyboardData, &kbd_rec);

	if (!kbd_rec.bKeyDown)
	{
		return (-1);
	}
	return kbd_rec.asciiChar;
}


WCHAR putChar[2] = L" "; // one character
UNICODE_STRING unicodeChar = {2, 2, putChar}; // 2 byte unicode character


NTSTATUS
cliPutChar (IN WCHAR c)
{
    // initialize the string
    unicodeChar.Buffer[0] = c;

    // print the character
    return NtDisplayString (&unicodeChar);
}

NTSTATUS
cliPrintString (IN PCHAR line)
{
	NTSTATUS status;
	
	// loop every character
    while (*line != '\0')
    {
        // Print the character
        status = cliPutChar(*line);
		line ++;
    }
	
		
	// return status
    return status;
}


// Input buffer
CHAR line[1024];
CHAR currentPosition = 0;

PCHAR
cliGetLine (IN HANDLE hDriver)
{
    CHAR c;
    BOOLEAN first = FALSE;

    // Wait for a new character
    while (TRUE)
    {
        // get the character that was pressed
        c = сliGetChar(hDriver);

        // check if this was ENTER
        if (c == '\r') // such Windows...
        {
			line[currentPosition] = ANSI_NULL;
            currentPosition = 0;
            
			// return it
            return line;
        }
		// skip backspace
        else if (c == '\b')
        {
            continue;
        }

        // c sure it's not NULL.
        if (!c || c == -1) continue;

        // add it to our line buffer
        line[currentPosition] = c;
        currentPosition++;

        // echo here:
        cliPutChar(c);
    }
}


void NtProcessStartup (void* StartupArgument) 
{ 
	NTSTATUS status;
	PCHAR command;
		
    // setup keyboard input:
    status = cliOpenInputDevice (&hKeyboard, L"\\Device\\KeyboardClass0");
	
	cliPrintString ("Type 'quit' to exit\n");
	cliPrintString (">>>");
	
	while (TRUE)
    {
        // get the line that was entered and display a new line
        command = cliGetLine (hKeyboard);
        cliPrintString ("\n");

        // make sure there's actually a command
        if (*command)
        {
            // process the command and do a new line again.
            if (!_strnicmp(command, "quit", 4)) // exit if "quit"
			{
				NtTerminateProcess(NtCurrentProcess(), 0); // exit native application
			}
			cliPrintString ("\n");
			cliPrintString (command);
			cliPrintString ("\n");
        }

        // display the prompt, and restart the loop
        cliPrintString (">>>");
    }
}