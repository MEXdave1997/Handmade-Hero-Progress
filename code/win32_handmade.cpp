//NOTE(David): Continue from here - https://youtu.be/w7ay7QXmo_o?t=4008
#include <Windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct Win32_OffScreen_Buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

// TODO(David): This is a global for now.
global_variable bool Running;
global_variable Win32_OffScreen_Buffer GlobalBackuffer;

struct Win32_Window_Dimension
{
	int Width;
	int Height;
};

Win32_Window_Dimension
GetWindowDimension(HWND Window)
{
	Win32_Window_Dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

internal void
Win32RenderWeirdgradient(Win32_OffScreen_Buffer Buffer, int XOffset, int YOffset)
{
	// TODO(David): Let's see what the optimizer does.
	int Width = Buffer.Width;
	int Height = Buffer.Height;

	uint8 *Row = (uint8 *)Buffer.Memory;
	for (int Y = 0; Y < Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Width; ++X)
		{
			uint8 Blue = (X + XOffset);
			uint8 Green = (Y + YOffset);

			/*
				Memory:		BB GG RR xx
				Register: 	xx RR GG BB

				Pixel (32-bits) 
			*/

			*Pixel++ = ((Green << 8) | Blue);	
		}

		Row += Buffer.Pitch;
	}
}

internal void
Win32ResizeDIBSection(Win32_OffScreen_Buffer *Buffer, int Width, int Height)
{
	// TODO(David): Bulletproof this.
	// Maybe don't free first, free after, then free first if that fails.

	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	// NOTE(David): When the biHeight field is negative this is the clue to
	// Windows to treat this bitmap as top-down, not bottom-up, meaning that
	// the first three bytes of the image are the color for the top left pixel
	// in the bitmap, not the bottom left! 
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	

	// NOTE(David): Thank you to Chris Hecker of Spy Party fame
	// for clarifying the deal with StretchDIBits and BitBlt!
	// No more DC for us.
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Width*Buffer->BytesPerPixel;

	// TODO(David): Probably clear this to black
}

internal void
Win32DisplayBufferToWindow(HDC DeviceContext,
						   int WindowWidth, int WindowHeight,
						   Win32_OffScreen_Buffer Buffer,
						   int X, int Y, int Width, int Height)
{
	// TODO(David): Aspect Ratio Correction
	StretchDIBits(DeviceContext,
				  /*
					X, Y, Width, Height,
					X, Y, Width, Height, 
				  */
				  0, 0, WindowWidth, WindowHeight,
				  0, 0, Buffer.Width, Buffer.Height,
				  Buffer.Memory,
				  &Buffer.Info,
				  DIB_RGB_COLORS,
				  SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
						UINT Message, 
						WPARAM WParam, 
						LPARAM LParam)
{
	LRESULT Result = 0;

	switch (Message)
	{
		case WM_SIZE:
		{
		} break;

		case WM_CLOSE:
		{
			// TODO(David): Handle this with a message to the User?
			Running = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_DESTROY:
		{
			// TODO(David): Handle this as an error - recreate window?
			Running = false;
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			Win32_Window_Dimension Dimension = GetWindowDimension(Window);
			Win32DisplayBufferToWindow(DeviceContext,
									   Dimension.Width, Dimension.Height,
									   GlobalBackuffer, X, Y, Width, Height);
			EndPaint(Window, &Paint);
		}

		default:
		{
			// OutputDebugStringA("default\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return Result;
}

int CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)
{
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackuffer , 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	// WindowClass.hIcon = ;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClassA(&WindowClass))
	{
		HWND Window =
			CreateWindowExA(0,
							WindowClass.lpszClassName,
							"Handmade Hero",
							WS_OVERLAPPEDWINDOW | WS_VISIBLE,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							0,
							0,
							Instance,
							0);
		if (Window)
		{
			int XOffset = 0;
			int YOffset = 0;

			Running = true;
			while (Running)
			{
				MSG Message;

				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}
	
				Win32RenderWeirdgradient(GlobalBackuffer, XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);
				Win32_Window_Dimension Dimension = GetWindowDimension(Window);
				Win32DisplayBufferToWindow(DeviceContext,
										   Dimension.Width, Dimension.Height,
										   GlobalBackuffer, 0, 0, Dimension.Width, Dimension.Height);
				ReleaseDC(Window, DeviceContext);

				++XOffset;
				YOffset += 2;
			}
		}
		else
		{
			// TODO(David): Logging
		}
	}
	else
	{
		// TODO(David): Logging
	}

	return 0;
}