#include "precomp.h"

// хэндл клавиатуры
HANDLE hKeyboard;

// хэндл события
HANDLE hEvent;

// входной буфер
CHAR line[1024];



NTSTATUS
cliOpenInputDevice (OUT PHANDLE handle, WCHAR* deviceName) // открыть устройство клавиатура
{
    UNICODE_STRING driver;
    OBJECT_ATTRIBUTES objectAttrs;
    IO_STATUS_BLOCK iosb; // статус выполнения функции и информация о заданном праве доступа (FILE_CREATED, FILE_OPENED, etc...)
    HANDLE hDriver;
    NTSTATUS status; 

    RtlInitUnicodeString (&driver, L"\\Device\\KeyboardClass0"); // из wchar* в unicode
    
    // инициализация атрибутов объекта
    InitializeObjectAttributes (&objectAttrs,			// сохраняет атрибуты в этой переменной
                                &driver, 				// передаём имя устройства
                                OBJ_CASE_INSENSITIVE, 	// поле для атрибутов
                                NULL,  					// корневая папка (нет необходимости)
                                NULL); 					// дескриптор (необязательное поле)

    // создаём(открываем) файл клавиатуры
    status = NtCreateFile (&hDriver, // хэндл сохраняется в эту переменную 
                           SYNCHRONIZE | GENERIC_READ | FILE_READ_ATTRIBUTES, 	// Запрашиваемый доступ
                           &objectAttrs,										// инициализированные атрибуты
                           &iosb,												// статус-блок
                           NULL,												// размер файла после создания
                           FILE_ATTRIBUTE_NORMAL,								// атрибуты файла, по умолчанию
                           0,													// совместное использование файла
                           FILE_OPEN,											// указывает, как создать файл (файл уже должен существовать)
                           FILE_DIRECTORY_FILE,					// опции создания
                           NULL,								// буфер расширенных атрибутов
                           0);									// длина предыдущего параметра

    // создаём событие, которое будет ожидать ввода с клавиатуры
    InitializeObjectAttributes (&objectAttrs, NULL, 0, NULL, NULL);
    status = NtCreateEvent (&hEvent, 			// атрибуты сохраняются тут
						    EVENT_ALL_ACCESS,   // все возможные права доступа к объекту
						    &objectAttrs,   	// инициализированные атрибуты
						    1, 					// тип события (NotificationEvent)
						    0);					// начальное состояние (TRUE/FALSE)

    // возвращаем хэндл
    *handle = hDriver;
    return status;
}

// ожидание ввода с устройства
NTSTATUS
cliWaitForInput (IN HANDLE hDriver,       	// хэндл клавиатуры
                 IN PVOID buffer,         	// буфер для ввода
                 IN OUT PULONG bufferSize)	// размер буфера ввода
{
    IO_STATUS_BLOCK iosb; 		// статус выполнения функции и информация о заданном праве доступа (FILE_CREATED, FILE_OPENED, etc...)
    LARGE_INTEGER byteOffset;
    NTSTATUS status;

    RtlZeroMemory (&iosb, sizeof(iosb)); 			 // заполняет блок памяти нулями
    RtlZeroMemory (&byteOffset, sizeof(byteOffset)); // заполняет блок памяти нулями

    // попытка прочитать данные 
    status = NtReadFile (hDriver, 		// хэндл файла клавиатуры
                         hEvent,  		// хэндл события ожидания ввода с клавиатуры
                         NULL,			// адрес APC-функции (не используется)
                         NULL,			// параметр для APC-функции
                         &iosb, 		// статус-блок
                         buffer,		// буфер, в который посещаются прочитанные данные из файла
                         *bufferSize,   // размер буфера в байтах
                         &byteOffset,   // байт смещения
                         NULL);			// зарезервированно

    // проверка, если данные ожидают ввода
    if (status == STATUS_PENDING) // если операция ввода/вывода выполняется
    {
        // ожидание чтения 
        status = NtWaitForSingleObject (hEvent, // ожидает ответа от хэндла объекта
									   TRUE, 	// сигнальный поток
								       NULL); 	// таймаут 
    }

    // возвращает статус и объём прочитанной информации
    *bufferSize = (ULONG)iosb.Information;
    return status;
}

// получаем один символ
CHAR
сliGetChar (IN HANDLE hDriver)
{
	KEYBOARD_INPUT_DATA keyboardData;
	KBD_RECORD kbd_rec;
	ULONG bufferLength = sizeof(KEYBOARD_INPUT_DATA);

	cliWaitForInput(hDriver, &keyboardData, &bufferLength);

	IntTranslateKey(&keyboardData, &kbd_rec);

	if (!kbd_rec.bKeyDown) //проверка, если клавиша не нажата, то возвращаем -1
	{
		return (-1);
	}
	return kbd_rec.asciiChar; //иначе, возвращаем код символа в ascii
}


WCHAR putChar[2] = L" "; // один символ
UNICODE_STRING unicodeChar = {2, 2, putChar}; // 2-хбайтовый символ unicode


NTSTATUS
cliPutChar (IN WCHAR c)
{
    // инициализация строки
    unicodeChar.Buffer[0] = c;

    // печать символа
    return NtDisplayString (&unicodeChar);
}

NTSTATUS
cliPrintString (IN PCHAR line)
{
	NTSTATUS status;
	
	// пока не наткулись на конец строки
    while (*line != '\0')
    {
        // печатем символы
        status = cliPutChar(*line);
		line ++;
    }
	
		
	// статус функции
    return status;
}


// входной буфер
CHAR line[1024];
CHAR currentPosition = 0;

PCHAR
cliGetLine (IN HANDLE hDriver)
{
    CHAR c;
    BOOLEAN first = FALSE;

    // ждём новый символ
    while (TRUE)
    {
        // получаем символ
        c = сliGetChar(hDriver);

        // проверка, не Enter ли это
        if (c == '\r')
        {
			line[currentPosition] = ANSI_NULL; //нуль-терминатор
            currentPosition = 0; //обнуляем позицию считывания
            
			// возвращаем строку
            return line;
        }
		// на backspace внимания не обращаем
        else if (c == '\b')
        {
            continue;
        }

        // убеждаемся, что символ не NULL.
        if (!c || c == -1) continue;

        // добавляем его в строковый буфер
        line[currentPosition] = c;
        currentPosition++;

        // вывод на экран
        cliPutChar(c);
    }
}


BOOLEAN closeFile (HANDLE hFile)
{
	NTSTATUS status = 0;

	status = NtClose(hFile);

	if (status == STATUS_SUCCESS)
	{
		return TRUE;
	}

	return FALSE;
}


BOOLEAN openFile (HANDLE* phRetFile, // Сохронять хендлер сюда
				  WCHAR* fileName)   // Название файла
{
	HANDLE hFile;
	UNICODE_STRING ustrFileName; 	 		// В ФС имена файлов сохраняются в UTF-16LE, 2 байта
	IO_STATUS_BLOCK ioStatusBlock;			// Cтатус выполнения функции и информация о заданном праве доступа (FILE_CREATED, FILE_OPENED, etc
	WCHAR wszFileName[1024] = L"\\??\\";    // An NT Object Path string MUST begin with \??\. UNC во все поля
	OBJECT_ATTRIBUTES objectAttributes;     // Атрибуты файла
	NTSTATUS status;

	wcscat (wszFileName, fileName);			// Дополняет строку

	RtlInitUnicodeString (&ustrFileName, wszFileName); // Initializes a counted Unicode string (Winternl.h)

	InitializeObjectAttributes (&objectAttributes, 		// сохраняет атрибуты в этой переменной
								&ustrFileName,   	 	// имя файла
								OBJ_CASE_INSENSITIVE,	// поле для атрибутов
								NULL,					// корневая папка (нет необходимости)
                                NULL); 					// дескриптор (необязательное поле)

	
	status = NtCreateFile (&hFile, 									// хэндл сохраняется в эту переменную
						   GENERIC_WRITE|SYNCHRONIZE|GENERIC_READ,  // Запрашиваемый доступ
						   &objectAttributes, 						// инициализированные атрибуты
						   &ioStatusBlock, 							// статус-блок
						   0, 										// размер файла после создания
						   FILE_ATTRIBUTE_NORMAL, 					// атрибуты файла, по умолчанию
						   0, 										// совместное использование файла
						   FILE_OPEN, 								// указывает, как создать файл (файл уже должен существовать)
						   FILE_SYNCHRONOUS_IO_NONALERT,  			// опции создания
						   NULL, 									// буфер расширенных атрибутов
						   0); 										// длина предыдущего параметра

	if (!NT_SUCCESS(status)) // Если что-то пошло не так
	{
		cliPrintString ("CreateFile() failed");
		return FALSE;
	}

	*phRetFile = hFile; // Записывает хендлер

	return TRUE;
}


BOOLEAN getFileSize (HANDLE hFile, 				// Хэндлер файла
					 LONGLONG* pRetFileSize) 	// размер файла запишется сюда
{
	IO_STATUS_BLOCK sIoStatus;				// Cтатус выполнения функции
	FILE_STANDARD_INFORMATION sFileInfo;	// Информация о файле
	NTSTATUS status = 0;

	// NtQueryInformationFile - routine returns various kinds of information about a file object.
	status = NtQueryInformationFile (hFile, // хэндлер
									 &sIoStatus, // 
									 &sFileInfo, 						// структура с информацией о файле
									 sizeof(FILE_STANDARD_INFORMATION), // размер этой стркутуры
									 FileStandardInformation);		    // тип запрашиваемой информации
									 
	if (status == STATUS_SUCCESS)
	{
		if (pRetFileSize)
		{
			*pRetFileSize = sFileInfo.EndOfFile.QuadPart;
		}
		return TRUE;
	}

	return FALSE;
}


BOOLEAN readFile (HANDLE hFile, 
				  LPVOID pOutBuffer, 
				  DWORD dwOutBufferSize, 
				  DWORD* pRetReadedSize)
{
	IO_STATUS_BLOCK sIoStatus;
	NTSTATUS status = 0;

	status = NtReadFile (hFile, // хэндл
						 NULL,  // эвент, тут он не нужен
						 NULL,  // зарезервированно
						 NULL,  // зарезервированно
						 &sIoStatus, 
						 pOutBuffer, 	   	// буфер с данными
						 dwOutBufferSize,  	// размер буфера
						 NULL, 				// offset, для чтения "за" файлом. Тут не нужно
						 NULL); 			// должен быть NULL
						 
	if (status == STATUS_SUCCESS) 
	{
		if (pRetReadedSize) 
		{
			*pRetReadedSize = sIoStatus.Information; // Сколько байтов прочитано
		}

		return TRUE;
	}
	return FALSE;
}


VOID typeFile(WCHAR *filename)
{
	HANDLE hFile = NULL;
	BYTE Buf[8192];
	LONGLONG fileSize = 0;
	DWORD readedSize = 0;
	DWORD readedSizeTotal = 0;
	BOOLEAN bResult = 0;
	
	bResult = openFile(&hFile, filename); // открыть файл
	if (bResult == FALSE) 
	{
		return;
	}
	
	
	if (getFileSize(hFile, &fileSize) == FALSE) // получить размер файла
	{
		closeFile(hFile);
		return;
	}
	
	
	while (1) // Чтение в цикле по 8 Кб - по 2 страници PAE
	{
		readedSize = 0;

		if (readFile(hFile, Buf, 8192, &readedSize) == FALSE) 
		{
			closeFile(hFile);
			return;
		}
		cliPrintString(Buf); // Печать тут
		
		readedSizeTotal += readedSize;
		if (readedSizeTotal == fileSize)
		{
			// Конец файла...
			break;
		}
	}
}


void NtProcessStartup (void* StartupArgument) 
{ 
	NTSTATUS status;
	PCHAR command;
	
	typeFile(L"C:\\test.txt");
	
    // устанавливаем ввод с клавиатуры
    status = cliOpenInputDevice (&hKeyboard, L"\\Device\\KeyboardClass0");
	
	cliPrintString ("Type 'quit' to exit\n");
	cliPrintString (">>>");
	
	while (TRUE)
    {
        // получаем строку, введённую в поле и выводим ее
        command = cliGetLine (hKeyboard);
        cliPrintString ("\n");

        // проверяем, не команда ли была введена
        if (*command)
        {
            // выполняем команду и переходим на новую строку
            if (!_strnicmp(command, "quit", 4)) // выходим, если введено "quit"
			{
				NtTerminateProcess(NtCurrentProcess(), 0); // функция завершения процесса
			}
			cliPrintString ("\n");
			cliPrintString (command);
			cliPrintString ("\n");
        }

        // показываем елку и ждём ввода информации
        cliPrintString (">>>");
    }
}